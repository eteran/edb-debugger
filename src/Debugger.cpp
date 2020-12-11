/*
Copyright (C) 2006 - 2015 Evan Teran
                          evan.teran@gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ArchProcessor.h"
#include "CommentServer.h"
#include "Configuration.h"
#include "Debugger.h"
#include "DebuggerInternal.h"
#include "DialogAbout.h"
#include "DialogArguments.h"
#include "DialogAttach.h"
#include "DialogMemoryRegions.h"
#include "DialogOpenProgram.h"
#include "DialogOptions.h"
#include "DialogPlugins.h"
#include "DialogThreads.h"
#include "Expression.h"
#include "IAnalyzer.h"
#include "IBinary.h"
#include "IBreakpoint.h"
#include "IDebugEvent.h"
#include "IDebugger.h"
#include "IPlugin.h"
#include "IProcess.h"
#include "IThread.h"
#include "Instruction.h"
#include "MemoryRegions.h"
#include "QHexView"
#include "RecentFileManager.h"
#include "RegionBuffer.h"
#include "RegisterViewModelBase.h"
#include "SessionError.h"
#include "SessionManager.h"
#include "State.h"
#include "Symbol.h"
#include "SymbolManager.h"
#include "Theme.h"
#include "edb.h"

#if defined(Q_OS_LINUX)
#include "linker.h"
#endif

#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QScreen>
#include <QSettings>
#include <QShortcut>
#include <QStringListModel>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVector>
#include <QtDebug>

#include <clocale>
#include <cstring>
#include <memory>

#if defined(Q_OS_UNIX)
#include <signal.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>
#if defined(Q_OS_LINUX) || defined(Q_OS_OPENBSD) || defined(Q_OS_FREEBSD)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

constexpr quint64 initial_bp_tag    = Q_UINT64_C(0x494e4954494e5433); // "INITINT3" in hex
constexpr quint64 stepover_bp_tag   = Q_UINT64_C(0x535445504f564552); // "STEPOVER" in hex
constexpr quint64 run_to_cursor_tag = Q_UINT64_C(0x474f544f48455245); // "GOTOHERE" in hex
#ifdef Q_OS_LINUX
constexpr quint64 ld_loader_tag = Q_UINT64_C(0x4c49424556454e54); // "LIBEVENT" in hex
#endif

template <class Addr>
void handle_library_event(IProcess *process, edb::address_t debug_pointer) {
#ifdef Q_OS_LINUX
	edb::linux_struct::r_debug<Addr> dynamic_info;
	const bool ok = (process->readBytes(debug_pointer, &dynamic_info, sizeof(dynamic_info)) == sizeof(dynamic_info));
	if (ok) {

		// NOTE(eteran): at least on my system, the name of
		//               what is being loaded is either in
		//               r8 or r13 depending on which event
		//               we are looking at.
		// TODO(eteran): find a way to get the name reliably

		switch (dynamic_info.r_state) {
		case edb::linux_struct::r_debug<Addr>::RT_CONSISTENT:
			// TODO(eteran): enable this once we are confident
#if 0
			edb::v1::memory_regions().sync();
#endif
			break;
		case edb::linux_struct::r_debug<Addr>::RT_ADD:
			//qDebug("LIBRARY LOAD EVENT");
			break;
		case edb::linux_struct::r_debug<Addr>::RT_DELETE:
			//qDebug("LIBRARY UNLOAD EVENT");
			break;
		}
	}
#else
	Q_UNUSED(process)
	Q_UNUSED(debug_pointer)
#endif
}

template <class Addr>
edb::address_t find_linker_hook_address(IProcess *process, edb::address_t debug_pointer) {
#ifdef Q_OS_LINUX
	edb::linux_struct::r_debug<Addr> dynamic_info;
	const bool ok = process->readBytes(debug_pointer, &dynamic_info, sizeof(dynamic_info));
	if (ok) {
		return edb::address_t::fromZeroExtended(dynamic_info.r_brk);
	}
#else
	Q_UNUSED(process)
	Q_UNUSED(debug_pointer)
#endif
	return edb::address_t(0);
}

//--------------------------------------------------------------------------
// Name: is_instruction_ret
//--------------------------------------------------------------------------
bool is_instruction_ret(edb::address_t address) {

	uint8_t buffer[edb::Instruction::MaxSize];
	if (const int size = edb::v1::get_instruction_bytes(address, buffer)) {
		edb::Instruction inst(buffer, buffer + size, address);
		return is_ret(inst);
	}
	return false;
}

class RunUntilRet : public IDebugEventHandler {
	Q_DECLARE_TR_FUNCTIONS(RunUntilRet)

public:
	//--------------------------------------------------------------------------
	// Name: RunUntilRet
	//--------------------------------------------------------------------------
	RunUntilRet() {
		edb::v1::add_debug_event_handler(this);
	}

	//--------------------------------------------------------------------------
	// Name: ~RunUntilRet
	//--------------------------------------------------------------------------
	~RunUntilRet() override {
		edb::v1::remove_debug_event_handler(this);

		for (const auto &bp : ownBreakpoints_) {
			if (!bp.second.expired()) {
				edb::v1::debugger_core->removeBreakpoint(bp.first);
			}
		}
	}

	//--------------------------------------------------------------------------
	// Name: pass_back_to_debugger
	// Desc: Makes the previous handler the event handler again and deletes this.
	//--------------------------------------------------------------------------
	virtual edb::EventStatus pass_back_to_debugger() {
		delete this;
		return edb::DEBUG_NEXT_HANDLER;
	}

	//--------------------------------------------------------------------------
	// Name: handle_event
	//--------------------------------------------------------------------------
	//TODO: Need to handle stop/pause button
	edb::EventStatus handleEvent(const std::shared_ptr<IDebugEvent> &event) override {

		if (!event->isTrap()) {
			return pass_back_to_debugger();
		}

		if (IProcess *process = edb::v1::debugger_core->process()) {

			State state;
			process->currentThread()->getState(&state);

			edb::address_t address               = state.instructionPointer();
			IDebugEvent::TRAP_REASON trap_reason = event->trapReason();
			IDebugEvent::REASON reason           = event->reason();

			qDebug() << QString("Event at address 0x%1").arg(address, 0, 16);

			/*
			 * An IDebugEvent::TRAP_BREAKPOINT can happen for the following reasons:
			 * 1. We hit a user-set breakpoint.
			 * 2. We hit an internal breakpoint due to our RunUntilRet algorithm.
			 * 3. We hit a syscall (this shouldn't be; it may be a ptrace bug).
			 * 4. We have exited in some form or another.
			 * First check for exit, then breakpoint (user-set, then internal; adjust for RIP in both cases),
			 * then finally for the syscall bug.
			 */
			if (trap_reason == IDebugEvent::TRAP_BREAKPOINT) {
				qDebug() << "Trap breakpoint";

				//Take care of exit/terminated conditions; address == 0 may suffice to catch all, but not 100% sure.
				if (reason == IDebugEvent::EVENT_EXITED || reason == IDebugEvent::EVENT_TERMINATED || address == 0) {
					qDebug() << "The process is no longer running.";
					return pass_back_to_debugger();
				}

				//Check the previous byte for 0xcc to see if it was an actual breakpoint
				std::shared_ptr<IBreakpoint> bp = edb::v1::find_triggered_breakpoint(address);

				//If there was a bp there, then we hit a block terminator as part of our RunUntilRet
				//algorithm, or it is a user-set breakpoint.
				if (bp && bp->enabled()) { //Isn't it always enabled if trap_reason is breakpoint, anyway?

					const edb::address_t prev_address = bp->address();

					bp->hit();

					//Adjust RIP since 1st byte was replaced with 0xcc and we are now 1 byte after it.
					state.setInstructionPointer(prev_address);
					process->currentThread()->setState(state);
					address = prev_address;

					//If it wasn't internal, it was a user breakpoint. Pass back to Debugger.
					if (!bp->internal()) {
						qDebug() << "Previous was not an internal breakpoint.";
						return pass_back_to_debugger();
					}
					qDebug() << "Previous was an internal breakpoint.";
					bp->disable();
					edb::v1::debugger_core->removeBreakpoint(bp->address());
				} else {
					//No breakpoint if it was a syscall; continue.
					return edb::DEBUG_CONTINUE;
				}
			}

			//If we are on our ret (or the instr after?), then ret.
			if (address == returnAddress_) {
				qDebug() << QString("On our terminator at 0x%1").arg(address, 0, 16);
				if (is_instruction_ret(address)) {
					qDebug() << "Found ret; passing back to debugger";
					return pass_back_to_debugger();
				} else {
					//If not a ret, then step so we can find the next block terminator.
					qDebug() << "Not ret. Single-stepping";
					return edb::DEBUG_CONTINUE_STEP;
				}
			}

			//If we stepped (either because it was the first event or because we hit a jmp/jcc),
			//then find the next block terminator and edb::DEBUG_CONTINUE.
			//TODO: What if we started on a ret? Set bp, then the proc runs away?
			uint8_t buffer[edb::Instruction::MaxSize];
			while (const int size = edb::v1::get_instruction_bytes(address, buffer)) {

				//Get the instruction
				edb::Instruction inst(buffer, buffer + size, 0);
				qDebug() << QString("Scanning for terminator at 0x%1: found %2").arg(address, 0, 16).arg(inst.mnemonic().c_str());

				//Check if it's a proper block terminator (ret/jmp/jcc/hlt)
				if (inst) {
					if (is_terminator(inst)) {
						qDebug() << QString("Found terminator %1 at 0x%2").arg(QString(inst.mnemonic().c_str())).arg(address, 0, 16);
						//If we already had a breakpoint there, then just continue.
						if (std::shared_ptr<IBreakpoint> bp = edb::v1::debugger_core->findBreakpoint(address)) {
							qDebug() << QString("Already a breakpoint at terminator 0x%1").arg(address, 0, 16);
							return edb::DEBUG_CONTINUE;
						}

						//Otherwise, attempt to set a breakpoint there and continue.
						if (std::shared_ptr<IBreakpoint> bp = edb::v1::debugger_core->addBreakpoint(address)) {
							ownBreakpoints_.emplace_back(address, bp);
							qDebug() << QString("Setting breakpoint at terminator 0x%1").arg(address, 0, 16);
							bp->setInternal(true);
							bp->setOneTime(true); //If the 0xcc get's rm'd on next event, then
												  //don't set it one time; we'll hande it manually
							returnAddress_ = address;
							return edb::DEBUG_CONTINUE;
						} else {
							QMessageBox::critical(edb::v1::debugger_ui,
												  tr("Error running until return"),
												  tr("Failed to set breakpoint on a block terminator at address %1.").arg(address.toPointerString()));
							return pass_back_to_debugger();
						}
					}
				} else {
					//Invalid instruction or some other problem. Pass it back to the debugger.
					QMessageBox::critical(edb::v1::debugger_ui,
										  tr("Error running until return"),
										  tr("Failed to disassemble instruction at address %1.").arg(address.toPointerString()));
					return pass_back_to_debugger();
				}

				address += inst.byteSize();
			}

			//If we end up out here, we've got bigger problems. Pass it back to the debugger.
			QMessageBox::critical(edb::v1::debugger_ui,
								  tr("Error running until return"),
								  tr("Stepped outside the loop, address=%1.").arg(address.toPointerString()));
			return pass_back_to_debugger();
		}

		qDebug() << "The process is no longer running.";
		return pass_back_to_debugger();
	}

private:
	std::vector<std::pair<edb::address_t, std::weak_ptr<IBreakpoint>>> ownBreakpoints_;
	edb::address_t lastCallReturn_ = 0;
	edb::address_t returnAddress_  = 0;
};

}

//------------------------------------------------------------------------------
// Name: Debugger
// Desc:
//------------------------------------------------------------------------------
Debugger::Debugger(QWidget *parent)
	: QMainWindow(parent),
	  ttyProc_(new QProcess(this)),
	  argumentsDialog_(new DialogArguments),
	  timer_(new QTimer(this)),
	  recentFileManager_(new RecentFileManager(this)),
	  stackViewInfo_(nullptr),
	  commentServer_(std::make_shared<CommentServer>()) {
	setupUi();

	// connect the timer to the debug event
	connect(timer_, &QTimer::timeout, this, &Debugger::nextDebugEvent);

	// create a context menu for the tab bar as well
	connect(ui.tabWidget, &TabWidget::customContextMenuRequested, this, &Debugger::tabContextMenu);

	// CPU Shortcuts
	gotoAddressAction_ = createAction(tr("&Goto Expression..."), QKeySequence(tr("Ctrl+G")), &Debugger::gotoTriggered);

	editCommentAction_           = createAction(tr("Add &Comment..."), QKeySequence(tr(";")), &Debugger::mnuCPUEditComment);
	toggleBreakpointAction_      = createAction(tr("&Toggle Breakpoint"), QKeySequence(tr("F2")), &Debugger::mnuCPUToggleBreakpoint);
	conditionalBreakpointAction_ = createAction(tr("Add &Conditional Breakpoint"), QKeySequence(tr("Shift+F2")), &Debugger::mnuCPUAddConditionalBreakpoint);
	runToThisLineAction_         = createAction(tr("R&un to this Line"), QKeySequence(tr("F4")), &Debugger::mnuCPURunToThisLine);
	runToLinePassAction_         = createAction(tr("Run to this Line (Pass Signal To Application)"), QKeySequence(tr("Shift+F4")), &Debugger::mnuCPURunToThisLinePassSignal);
	editBytesAction_             = createAction(tr("Binary &Edit..."), QKeySequence(tr("Ctrl+E")), &Debugger::mnuModifyBytes);
	fillWithZerosAction_         = createAction(tr("&Fill with 00's"), QKeySequence(), &Debugger::mnuCPUFillZero);
	fillWithNOPsAction_          = createAction(tr("Fill with &NOPs"), QKeySequence(), &Debugger::mnuCPUFillNop);
	setAddressLabelAction_       = createAction(tr("Set &Label..."), QKeySequence(tr(":")), &Debugger::mnuCPULabelAddress);
	followConstantInDumpAction_  = createAction(tr("Follow Constant In &Dump"), QKeySequence(), &Debugger::mnuCPUFollowInDump);
	followConstantInStackAction_ = createAction(tr("Follow Constant In &Stack"), QKeySequence(), &Debugger::mnuCPUFollowInStack);

	followAction_ = createAction(tr("&Follow"), QKeySequence(tr("Return")), [this]() {
		QWidget *const widget = QApplication::focusWidget();
		if (qobject_cast<QDisassemblyView *>(widget)) {
			mnuCPUFollow();
		} else {
			QKeyEvent *event = new QKeyEvent(QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier);
			QCoreApplication::postEvent(widget, event);
		}
	});

	// these get updated when we attach/run a new process, so it's OK to hard code them here
#if defined(EDB_X86_64)
	setRIPAction_  = createAction(tr("&Set %1 to this Instruction").arg("RIP"), QKeySequence(tr("Ctrl+*")), &Debugger::mnuCPUSetEIP);
	gotoRIPAction_ = createAction(tr("&Goto %1").arg("RIP"), QKeySequence(tr("*")), &Debugger::mnuCPUJumpToEIP);
#elif defined(EDB_X86)
	setRIPAction_  = createAction(tr("&Set %1 to this Instruction").arg("EIP"), QKeySequence(tr("Ctrl+*")), &Debugger::mnuCPUSetEIP);
	gotoRIPAction_ = createAction(tr("&Goto %1").arg("EIP"), QKeySequence(tr("*")), &Debugger::mnuCPUJumpToEIP);
#elif defined(EDB_ARM32) || defined(EDB_ARM64)
	setRIPAction_  = createAction(tr("&Set %1 to this Instruction").arg("PC"), QKeySequence(tr("Ctrl+*")), &Debugger::mnuCPUSetEIP);
	gotoRIPAction_ = createAction(tr("&Goto %1").arg("PC"), QKeySequence(tr("*")), &Debugger::mnuCPUJumpToEIP);
#else
#error "This doesn't initialize actions and will lead to crash"
#endif

	// Data Dump Shorcuts
	dumpFollowInCPUAction_   = createAction(tr("Follow Address In &CPU"), QKeySequence(), &Debugger::mnuDumpFollowInCPU);
	dumpFollowInDumpAction_  = createAction(tr("Follow Address In &Dump"), QKeySequence(), &Debugger::mnuDumpFollowInDump);
	dumpFollowInStackAction_ = createAction(tr("Follow Address In &Stack"), QKeySequence(), &Debugger::mnuDumpFollowInStack);
	dumpSaveToFileAction_    = createAction(tr("&Save To File"), QKeySequence(), &Debugger::mnuDumpSaveToFile);

	// Register View Shortcuts
	registerFollowInDumpAction_    = createAction(tr("&Follow In Dump"), QKeySequence(), &Debugger::mnuRegisterFollowInDump);
	registerFollowInDumpTabAction_ = createAction(tr("&Follow In Dump (New Tab)"), QKeySequence(), &Debugger::mnuRegisterFollowInDumpNewTab);
	registerFollowInStackAction_   = createAction(tr("&Follow In Stack"), QKeySequence(), &Debugger::mnuRegisterFollowInStack);

	// Stack View Shortcuts
	stackFollowInCPUAction_   = createAction(tr("Follow Address In &CPU"), QKeySequence(), &Debugger::mnuStackFollowInCPU);
	stackFollowInDumpAction_  = createAction(tr("Follow Address In &Dump"), QKeySequence(), &Debugger::mnuStackFollowInDump);
	stackFollowInStackAction_ = createAction(tr("Follow Address In &Stack"), QKeySequence(), &Debugger::mnuStackFollowInStack);

	// these get updated when we attach/run a new process, so it's OK to hard code them here
#if defined(EDB_X86_64)
	stackGotoRSPAction_ = createAction(tr("Goto %1").arg("RSP"), QKeySequence(), &Debugger::mnuStackGotoESP);
	stackGotoRBPAction_ = createAction(tr("Goto %1").arg("RBP"), QKeySequence(), &Debugger::mnuStackGotoEBP);
	stackPushAction_    = createAction(tr("&Push %1").arg("QWORD"), QKeySequence(), &Debugger::mnuStackPush);
	stackPopAction_     = createAction(tr("P&op %1").arg("QWORD"), QKeySequence(), &Debugger::mnuStackPop);
#elif defined(EDB_X86)
	stackGotoRSPAction_ = createAction(tr("Goto %1").arg("ESP"), QKeySequence(), &Debugger::mnuStackGotoESP);
	stackGotoRBPAction_ = createAction(tr("Goto %1").arg("EBP"), QKeySequence(), &Debugger::mnuStackGotoEBP);
	stackPushAction_    = createAction(tr("&Push %1").arg("DWORD"), QKeySequence(), &Debugger::mnuStackPush);
	stackPopAction_     = createAction(tr("P&op %1").arg("DWORD"), QKeySequence(), &Debugger::mnuStackPop);
#elif defined(EDB_ARM32)
	stackGotoRSPAction_ = createAction(tr("Goto %1").arg("SP"), QKeySequence(), &Debugger::mnuStackGotoESP);
	stackGotoRBPAction_ = createAction(tr("Goto %1").arg("FP"), QKeySequence(), &Debugger::mnuStackGotoEBP);
	stackGotoRBPAction_->setDisabled(true); // FIXME(ARM): this just stubs it out since it likely won't really work
	stackPushAction_ = createAction(tr("&Push %1").arg("DWORD"), QKeySequence(), &Debugger::mnuStackPush);
	stackPushAction_->setDisabled(true); // FIXME(ARM): this just stubs it out since it likely won't really work
	stackPopAction_ = createAction(tr("P&op %1").arg("DWORD"), QKeySequence(), &Debugger::mnuStackPop);
	stackPopAction_->setDisabled(true); // FIXME(ARM): this just stubs it out since it likely won't really work
#elif defined(EDB_ARM64)
	stackGotoRSPAction_ = createAction(tr("Goto %1").arg("SP"), QKeySequence(), &Debugger::mnuStackGotoESP);
	stackGotoRSPAction_->setDisabled(true); // FIXME(ARM): this just stubs it out since it likely won't really work
	stackGotoRBPAction_ = createAction(tr("Goto %1").arg("FP"), QKeySequence(), &Debugger::mnuStackGotoEBP);
	stackGotoRBPAction_->setDisabled(true); // FIXME(ARM): this just stubs it out since it likely won't really work
	stackPushAction_ = createAction(tr("&Push %1").arg("QWORD"), QKeySequence(), &Debugger::mnuStackPush);
	stackPushAction_->setDisabled(true); // FIXME(ARM): this just stubs it out since it likely won't really work
	stackPopAction_ = createAction(tr("P&op %1").arg("QWORD"), QKeySequence(), &Debugger::mnuStackPop);
	stackPopAction_->setDisabled(true); // FIXME(ARM): this just stubs it out since it likely won't really work
#else
#error "This doesn't initialize actions and will lead to crash"
#endif

	// set these to have no meaningful "data" (yet)
	followConstantInDumpAction_->setData(qlonglong(0));
	followConstantInStackAction_->setData(qlonglong(0));

	setAcceptDrops(true);

	// setup the list model for instruction details list
	listModel_ = new QStringListModel(this);
	ui.listView->setModel(listModel_);

	// setup the recent file manager
	ui.action_Recent_Files->setMenu(recentFileManager_->createMenu());
	connect(recentFileManager_, &RecentFileManager::fileSelected, this, &Debugger::openFile);

	// make us the default event handler
	edb::v1::add_debug_event_handler(this);

	// enable the arch processor
#if 0
	ui.registerList->setModel(&edb::v1::arch_processor().get_register_view_model());
	edb::v1::arch_processor().setup_register_view();
#endif

	// default the working directory to ours
	workingDirectory_ = QDir().absolutePath();

	// let the plugins setup their menus
	finishPluginSetup();

	// Make sure number formatting and reading code behaves predictably when using standard C++ facilities.
	// NOTE: this should only be done after the plugins have finished loading, since some dynamic libraries
	// (e.g. libkdecore), which are indirectly loaded by the plugins, re-set locale to "" once again. (This
	// first time is QApplication.)
	std::setlocale(LC_NUMERIC, "C");
}

//------------------------------------------------------------------------------
// Name: ~Debugger
// Desc:
//------------------------------------------------------------------------------
Debugger::~Debugger() {

	for (QObject *plugin : edb::v1::plugin_list()) {
		if (auto p = qobject_cast<IPlugin *>(plugin)) {
			p->fini();
		}
	}

	// kill our xterm and wait for it to die
	ttyProc_->kill();
	ttyProc_->waitForFinished(3000);
	edb::v1::remove_debug_event_handler(this);
}

template <class F>
QAction *Debugger::createAction(const QString &text, const QKeySequence &keySequence, F func) {
	auto action = new QAction(text, this);
	action->setShortcut(keySequence);
	addAction(action);
	connect(action, &QAction::triggered, this, func);
	return action;
}

//------------------------------------------------------------------------------
// Name: update_menu_state
// Desc:
//------------------------------------------------------------------------------
void Debugger::updateMenuState(GuiState state) {

	switch (state) {
	case Paused:
		ui.actionRun_Until_Return->setEnabled(true);
		ui.action_Restart->setEnabled(true);
		ui.action_Run->setEnabled(true);
		ui.action_Pause->setEnabled(false);
		ui.action_Step_Into->setEnabled(true);
		ui.action_Step_Over->setEnabled(true);
		ui.actionStep_Out->setEnabled(true);
		ui.action_Step_Into_Pass_Signal_To_Application->setEnabled(true);
		ui.action_Step_Over_Pass_Signal_To_Application->setEnabled(true);
		ui.action_Run_Pass_Signal_To_Application->setEnabled(true);
		ui.action_Detach->setEnabled(true);
		ui.action_Kill->setEnabled(true);
		tabCreate_->setEnabled(true);
		status_->setText(tr("paused"));
		status_->repaint();
		break;
	case Running:
		ui.actionRun_Until_Return->setEnabled(false);
		ui.action_Restart->setEnabled(false);
		ui.action_Run->setEnabled(false);
		ui.action_Pause->setEnabled(true);
		ui.action_Step_Into->setEnabled(false);
		ui.action_Step_Over->setEnabled(false);
		ui.actionStep_Out->setEnabled(false);
		ui.action_Step_Into_Pass_Signal_To_Application->setEnabled(false);
		ui.action_Step_Over_Pass_Signal_To_Application->setEnabled(false);
		ui.action_Run_Pass_Signal_To_Application->setEnabled(false);
		ui.action_Detach->setEnabled(true);
		ui.action_Kill->setEnabled(true);
		tabCreate_->setEnabled(true);
		status_->setText(tr("running"));
		status_->repaint();
		break;
	case Terminated:
		ui.actionRun_Until_Return->setEnabled(false);
		ui.action_Restart->setEnabled(recentFileManager_->entryCount() > 0);
		ui.action_Run->setEnabled(false);
		ui.action_Pause->setEnabled(false);
		ui.action_Step_Into->setEnabled(false);
		ui.action_Step_Over->setEnabled(false);
		ui.actionStep_Out->setEnabled(false);
		ui.action_Step_Into_Pass_Signal_To_Application->setEnabled(false);
		ui.action_Step_Over_Pass_Signal_To_Application->setEnabled(false);
		ui.action_Run_Pass_Signal_To_Application->setEnabled(false);
		ui.action_Detach->setEnabled(false);
		ui.action_Kill->setEnabled(false);
		tabCreate_->setEnabled(false);
		status_->setText(tr("terminated"));
		status_->repaint();
		break;
	}

	guiState_ = state;
}

//------------------------------------------------------------------------------
// Name: create_tty
// Desc: creates a TTY object for our command line I/O
//------------------------------------------------------------------------------
QString Debugger::createTty() {

	QString result_tty = ttyFile_;

#if defined(Q_OS_LINUX) || defined(Q_OS_OPENBSD) || defined(Q_OS_FREEBSD)
	// we attempt to reuse an open output window
	if (edb::v1::config().tty_enabled && ttyProc_->state() != QProcess::Running) {
		const QString command = edb::v1::config().tty_command;

		if (!command.isEmpty()) {

			// ok, creating a new one...
			// first try to get a 'unique' filename, i would love to use a system
			// temp file API... but there doesn't seem to be one which will create
			// a pipe...only ordinary files!
			const auto temp_pipe = QString("%1/edb_temp_file_%2_%3").arg(QDir::tempPath()).arg(qrand()).arg(getpid());

			// make sure it isn't already there, and then make the pipe
			::unlink(qPrintable(temp_pipe));
			::mkfifo(qPrintable(temp_pipe), S_IRUSR | S_IWUSR);

			// this is a basic shell script which will output the tty to a file (the pipe),
			// ignore kill sigs, close all standard IO, and then just hang
			const auto shell_script = QString(
										  "tty > %1;"
										  "trap \"\" INT QUIT TSTP;"
										  "exec<&-; exec>&-;"
										  "while :; do sleep 3600; done")
										  .arg(temp_pipe);

			// parse up the command from the options, white space delimited
			QStringList proc_args     = edb::v1::parse_command_line(command);
			const QString tty_command = proc_args.takeFirst().trimmed();

			// start constructing the arguments for the term
			const QFileInfo command_info(tty_command);

			if (command_info.fileName() == "gnome-terminal") {
				// NOTE(eteran): gnome-terminal at some point dropped support for -e
				// in favor of using "everything after --"
				// See issue: https://github.com/eteran/edb-debugger/issues/774
				proc_args << "--hide-menubar"
						  << "--title" << tr("edb output")
						  << "--";
			} else if (command_info.fileName() == "konsole") {
				proc_args << "--hide-menubar"
						  << "--title" << tr("edb output")
						  << "--nofork"
						  << "--hold"
						  << "-e";
			} else {
				proc_args << "-title" << tr("edb output")
						  << "-hold"
						  << "-e";
			}

			proc_args << "sh"
					  << "-c" << QString("%1").arg(shell_script);

			qDebug() << "Running Terminal: " << tty_command;
			qDebug() << "Terminal Args: " << proc_args;

			// make the tty process object and connect it's death signal to our cleanup
			connect(ttyProc_, SIGNAL(finished(int, QProcess::ExitStatus)), SLOT(ttyProcFinished(int, QProcess::ExitStatus)));

			ttyProc_->start(tty_command, proc_args);

			if (ttyProc_->waitForStarted(3000)) {

				// try to read from the pipe, but with a 2 second timeout
				int fd = open(qPrintable(temp_pipe), O_RDWR);
				if (fd != -1) {
					fd_set set;
					FD_ZERO(&set);    // clear the set
					FD_SET(fd, &set); // add our file descriptor to the set

					struct timeval timeout;
					timeout.tv_sec  = 2;
					timeout.tv_usec = 0;

					char buf[256] = {};
					const int rv  = select(fd + 1, &set, nullptr, nullptr, &timeout);
					switch (rv) {
					case -1:
						qDebug() << "An error occured while attempting to get the TTY of the terminal sub-process";
						break;
					case 0:
						qDebug() << "A Timeout occured while attempting to get the TTY of the terminal sub-process";
						break;
					default:
						if (read(fd, buf, sizeof(buf)) != -1) {
							result_tty = QString(buf).trimmed();
						}
						break;
					}

					::close(fd);
				}

			} else {
				qDebug().nospace() << "Could not launch the desired terminal [" << tty_command << "], please check that it exists and you have proper permissions.";
			}

			// cleanup, god i wish there was an easier way than this!
			::unlink(qPrintable(temp_pipe));
		}
	}
#endif

	qDebug() << "Terminal process has TTY: " << result_tty;

	return result_tty;
}

//------------------------------------------------------------------------------
// Name: tty_proc_finished
// Desc: cleans up the data associated with a TTY when the terminal dies
//------------------------------------------------------------------------------
void Debugger::ttyProcFinished(int exit_code, QProcess::ExitStatus exit_status) {
	Q_UNUSED(exit_code)
	Q_UNUSED(exit_status)

	ttyFile_.clear();
}

//------------------------------------------------------------------------------
// Name: current_tab
// Desc:
//------------------------------------------------------------------------------
int Debugger::currentTab() const {
	return ui.tabWidget->currentIndex();
}

//------------------------------------------------------------------------------
// Name: current_data_view_info
// Desc:
//------------------------------------------------------------------------------
std::shared_ptr<DataViewInfo> Debugger::currentDataViewInfo() const {
	return dataRegions_[currentTab()];
}

//------------------------------------------------------------------------------
// Name: set_debugger_caption
// Desc: sets the caption part to also show the application name and pid
//------------------------------------------------------------------------------
void Debugger::setDebuggerCaption(const QString &appname) {
	setWindowTitle(tr("edb - %1 [%2]").arg(appname).arg(edb::v1::debugger_core->process()->pid()));
}

//------------------------------------------------------------------------------
// Name: delete_data_tab
// Desc:
//------------------------------------------------------------------------------
void Debugger::deleteDataTab() {
	const int current = currentTab();

	// get a pointer to the info we need (before removing it from the list!)
	// this seems redundant to current_data_view_info(), but we need the
	// index too, so may as well waste one line to avoid duplicate work
	std::shared_ptr<DataViewInfo> info = dataRegions_[current];

	// remove it from the list
	dataRegions_.remove(current);

	// remove the tab associated with it
	ui.tabWidget->removeTab(current);
}

//------------------------------------------------------------------------------
// Name: create_data_tab
// Desc:
//------------------------------------------------------------------------------
void Debugger::createDataTab() {
	const int current = currentTab();

	// duplicate the current region
	auto new_data_view = std::make_shared<DataViewInfo>((current != -1) ? dataRegions_[current]->region : nullptr);

	auto hexview = std::make_shared<QHexView>();

	Theme theme = Theme::load();

	QColor addressForegroundColor = theme.text[Theme::Address].foreground().color();
	QColor alternatingByteColor   = theme.text[Theme::AlternatingByte].foreground().color();
	QColor nonPrintableTextColor  = theme.text[Theme::NonPrintingCharacter].foreground().color();
	hexview->setAddressColor(addressForegroundColor);
	hexview->setAlternateWordColor(alternatingByteColor);
	hexview->setNonPrintableTextColor(nonPrintableTextColor);

	//QColor coldZoneColor_         = Qt::gray;
	//QColor lineColor_             = Qt::black;

	new_data_view->view = hexview;

	// setup the context menu
	hexview->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(hexview.get(), &QHexView::customContextMenuRequested, this, &Debugger::mnuDumpContextMenu);

	// show the initial data for this new view
	if (new_data_view->region) {
		hexview->setAddressOffset(new_data_view->region->start());
	} else {
		hexview->setAddressOffset(0);
	}

	// NOTE(eteran): for issue #522, allow comments in data view when single word width
	hexview->setCommentServer(commentServer_.get());

	hexview->setData(new_data_view->stream.get());

	const Configuration &config = edb::v1::config();

	// set the default view options
	hexview->setShowAddress(config.data_show_address);
	hexview->setShowHexDump(config.data_show_hex);
	hexview->setShowAsciiDump(config.data_show_ascii);
	hexview->setShowComments(config.data_show_comments);
	hexview->setRowWidth(config.data_row_width);
	hexview->setWordWidth(config.data_word_width);
	hexview->setShowAddressSeparator(config.show_address_separator);

	// Setup data views according to debuggee bitness
	if (edb::v1::debuggeeIs64Bit()) {
		hexview->setAddressSize(QHexView::Address64);
	} else {
		hexview->setAddressSize(QHexView::Address32);
	}

	// set the default font
	QFont dump_font;
	dump_font.fromString(config.data_font);
	hexview->setFont(dump_font);

	dataRegions_.push_back(new_data_view);

	// create the tab!
	if (new_data_view->region) {
		ui.tabWidget->addTab(hexview.get(), tr("%1-%2").arg(
												edb::v1::format_pointer(new_data_view->region->start()),
												edb::v1::format_pointer(new_data_view->region->end())));
	} else {
		ui.tabWidget->addTab(hexview.get(), tr("%1-%2").arg(
												edb::v1::format_pointer(edb::address_t(0)),
												edb::v1::format_pointer(edb::address_t(0))));
	}

	ui.tabWidget->setCurrentIndex(ui.tabWidget->count() - 1);
}

//------------------------------------------------------------------------------
// Name: finish_plugin_setup
// Desc: finalizes plugin setup by adding each to the menu, we can do this now
//       that we have a GUI widget to attach it to
//------------------------------------------------------------------------------
void Debugger::finishPluginSetup() {

	// call the init function for each plugin, this is done after
	// ALL plugins are loaded in case there are inter-plugin dependencies
	for (QObject *plugin : edb::v1::plugin_list()) {
		if (auto p = qobject_cast<IPlugin *>(plugin)) {
			p->init();
		}
	}

	// setup the menu for all plugins that which to do so
	QPointer<DialogOptions> options = qobject_cast<DialogOptions *>(edb::v1::dialog_options());
	for (QObject *plugin : edb::v1::plugin_list()) {
		if (auto p = qobject_cast<IPlugin *>(plugin)) {
			if (QMenu *const menu = p->menu(this)) {
				ui.menu_Plugins->addMenu(menu);
			}

			if (QWidget *const options_page = p->optionsPage()) {
				if (options) {
					options->addOptionsPage(options_page);
				}
			}

			// setup the shortcuts for these actions
			const QList<QAction *> register_actions = p->registerContextMenu();
			const QList<QAction *> cpu_actions      = p->cpuContextMenu();
			const QList<QAction *> stack_actions    = p->stackContextMenu();
			const QList<QAction *> data_actions     = p->dataContextMenu();
			const QList<QAction *> actions          = register_actions + cpu_actions + stack_actions + data_actions;

			for (QAction *action : actions) {
				QKeySequence shortcut = action->shortcut();
				if (!shortcut.isEmpty()) {
					connect(new QShortcut(shortcut, this), &QShortcut::activated, action, &QAction::trigger);
				}
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: get_goto_expression
// Desc:
//------------------------------------------------------------------------------
Result<edb::address_t, QString> Debugger::getGotoExpression() {

	boost::optional<edb::address_t> address = edb::v2::get_expression_from_user(tr("Goto Expression"), tr("Expression:"));
	if (address) {
		return *address;
	} else {
		return make_unexpected(tr("No Address"));
	}
}

//------------------------------------------------------------------------------
// Name: get_follow_register
// Desc:
//------------------------------------------------------------------------------
Result<edb::reg_t, QString> Debugger::getFollowRegister() const {

	const Register reg = activeRegister();
	if (!reg || reg.bitSize() > 8 * sizeof(edb::address_t)) {
		return make_unexpected(tr("No Value"));
	}

	return reg.valueAsAddress();
}

//------------------------------------------------------------------------------
// Name: goto_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::gotoTriggered() {
	QWidget *const widget = QApplication::focusWidget();
	if (auto hexview = qobject_cast<QHexView *>(widget)) {
		if (hexview == stackView_.get()) {
			mnuStackGotoAddress();
		} else {
			mnuDumpGotoAddress();
		}
	} else if (qobject_cast<QDisassemblyView *>(widget)) {
		mnuCPUJumpToAddress();
	}
}

//------------------------------------------------------------------------------
// Name: setup_ui
// Desc: creates the UI
//------------------------------------------------------------------------------
void Debugger::setupUi() {
	// setup the global pointers as early as possible.
	// NOTE:  this should never be changed after this point
	// NOTE:  this is important that this happens BEFORE any components which
	// read settings as it could end up being a memory leak (and therefore never
	// calling it's destructor which writes the settings to disk!).
	edb::v1::debugger_ui = this;

	ui.setupUi(this);

	status_ = new QLabel(this);
	ui.statusbar->insertPermanentWidget(0, status_);

	// add toggles for the dock windows
	ui.menu_View->addAction(ui.dataDock->toggleViewAction());
	ui.menu_View->addAction(ui.stackDock->toggleViewAction());
	ui.menu_View->addAction(ui.toolBar->toggleViewAction());

	ui.action_Restart->setEnabled(recentFileManager_->entryCount() > 0);

	// make sure our widgets use custom context menus
	ui.cpuView->setContextMenuPolicy(Qt::CustomContextMenu);

	// set the listbox to about 4 lines
	const QFontMetrics &fm = ui.listView->fontMetrics();
	ui.listView->setFixedHeight(fm.height() * 4);

	setupStackView();
	setupTabButtons();

	// remove the one in the designer and put in our built one.
	// it's a bit ugly to do it this way, but the designer wont let me
	// make a tabless entry..and the ones i create look slightly diff
	// (probably lack of layout manager in mine...
	ui.tabWidget->clear();
	mnuDumpCreateTab();

	// apply any fonts we may have stored
	applyDefaultFonts();

	// apply the default setting for showing address separators
	applyDefaultShowSeparator();
}

//------------------------------------------------------------------------------
// Name: setup_stack_view
// Desc:
//------------------------------------------------------------------------------
void Debugger::setupStackView() {

	stackView_ = std::make_shared<QHexView>();

	Theme theme = Theme::load();

	QColor addressForegroundColor = theme.text[Theme::Address].foreground().color();
	QColor alternatingByteColor   = theme.text[Theme::AlternatingByte].foreground().color();
	QColor nonPrintableTextColor  = theme.text[Theme::NonPrintingCharacter].foreground().color();

	stackView_->setAddressColor(addressForegroundColor);
	stackView_->setAlternateWordColor(alternatingByteColor);
	stackView_->setNonPrintableTextColor(nonPrintableTextColor);

	stackView_->setUserConfigRowWidth(false);
	stackView_->setUserConfigWordWidth(false);

	ui.stackDock->setWidget(stackView_.get());

	// setup the context menu
	stackView_->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(stackView_.get(), &QHexView::customContextMenuRequested, this, &Debugger::mnuStackContextMenu);

	// we placed a view in the designer, so just set it here
	// this may get transitioned to heap allocated, we'll see
	stackViewInfo_.view = stackView_;

	// setup the comment server for the stack viewer
	stackView_->setCommentServer(commentServer_.get());
}

//------------------------------------------------------------------------------
// Name: closeEvent
// Desc: triggered on main window close, saves window state
//------------------------------------------------------------------------------
void Debugger::closeEvent(QCloseEvent *event) {

	// make sure sessions still get recorded even if they just close us
	const QString filename = sessionFilename();
	if (!filename.isEmpty()) {
		SessionManager::instance().saveSession(filename);
	}

	if (IDebugger *core = edb::v1::debugger_core) {
		core->endDebugSession();
	}

	// ensure that the detach event fires so that everyone who cases will be notified
	Q_EMIT detachEvent();

	QSettings settings;
	const QByteArray state = saveState();
	settings.beginGroup("Window");
	settings.setValue("window.state", state);
	settings.setValue("window.width", width());
	settings.setValue("window.height", height());
	settings.setValue("window.x", x());
	settings.setValue("window.y", y());
	settings.setValue("window.stack.show_address.enabled", stackView_->showAddress());
	settings.setValue("window.stack.show_hex.enabled", stackView_->showHexDump());
	settings.setValue("window.stack.show_ascii.enabled", stackView_->showAsciiDump());
	settings.setValue("window.stack.show_comments.enabled", stackView_->showComments());

	QByteArray dissassemblyState = ui.cpuView->saveState();
	settings.setValue("window.disassembly.state", dissassemblyState);

	settings.endGroup();
	event->accept();
}

//------------------------------------------------------------------------------
// Name: showEvent
// Desc: triggered on show, restores window state
//------------------------------------------------------------------------------
void Debugger::showEvent(QShowEvent *) {

	QSettings settings;
	settings.beginGroup("Window");
	const QByteArray state = settings.value("window.state").toByteArray();
	const int width        = settings.value("window.width", -1).toInt();
	const int height       = settings.value("window.height", -1).toInt();
	const int x            = settings.value("window.x", -1).toInt();
	const int y            = settings.value("window.y", -1).toInt();

	if (width != -1) {
		resize(width, size().height());
	}

	if (height != -1) {
		resize(size().width(), height);
	}

	const Configuration &config = edb::v1::config();
	switch (config.startup_window_location) {
	case Configuration::SystemDefault:
		break;
	case Configuration::Centered: {
		QDesktopWidget desktop;
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
		QScreen *screen = QGuiApplication::primaryScreen();
		QRect sg        = screen->geometry();
#else
		QRect sg = desktop.screenGeometry();
#endif
		int x = (sg.width() - this->width()) / 2;
		int y = (sg.height() - this->height()) / 2;
		move(x, y);
	} break;
	case Configuration::Restore:
		if (x != -1 && y != -1) {
			move(x, y);
		}
		break;
	}

	stackView_->setShowAddress(settings.value("window.stack.show_address.enabled", true).toBool());
	stackView_->setShowHexDump(settings.value("window.stack.show_hex.enabled", true).toBool());
	stackView_->setShowAsciiDump(settings.value("window.stack.show_ascii.enabled", true).toBool());
	stackView_->setShowComments(settings.value("window.stack.show_comments.enabled", true).toBool());

	int row_width  = 1;
	int word_width = edb::v1::pointer_size();

	stackView_->setRowWidth(row_width);
	stackView_->setWordWidth(word_width);

	QByteArray disassemblyState = settings.value("window.disassembly.state").toByteArray();
	ui.cpuView->restoreState(disassemblyState);

	settings.endGroup();
	restoreState(state);
}

//------------------------------------------------------------------------------
// Name: dragEnterEvent
// Desc: triggered when dragging data onto the main window
//------------------------------------------------------------------------------
void Debugger::dragEnterEvent(QDragEnterEvent *event) {
	const QMimeData *mimeData = event->mimeData();

	// check for our needed mime type (file)
	// make sure it's only one file
	if (mimeData->hasUrls() && mimeData->urls().size() == 1) {
		// extract the local path of the file
		QList<QUrl> urls = mimeData->urls();
		QUrl url         = urls[0].toLocalFile();
		if (!url.isEmpty()) {
			event->accept();
		}
	}
}

//------------------------------------------------------------------------------
// Name: dropEvent
// Desc: triggered when data was dropped onto the main window
//------------------------------------------------------------------------------
void Debugger::dropEvent(QDropEvent *event) {
	const QMimeData *mimeData = event->mimeData();

	if (mimeData->hasUrls() && mimeData->urls().size() == 1) {
		QList<QUrl> urls = mimeData->urls();
		const QString s  = urls[0].toLocalFile();
		if (!s.isEmpty()) {
			Q_ASSERT(edb::v1::debugger_core);

			commonOpen(s, QList<QByteArray>(), QString(), QString());
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_actionAbout_QT_triggered
// Desc: shows an About Qt dialog box
//------------------------------------------------------------------------------
void Debugger::on_actionAbout_QT_triggered() {
	QMessageBox::aboutQt(this, tr("About Qt"));
}

//------------------------------------------------------------------------------
// Name: apply_default_fonts
// Desc: applies the configuration's fonts to all necessary widgets
//------------------------------------------------------------------------------
void Debugger::applyDefaultFonts() {

	QFont font;
	const Configuration &config = edb::v1::config();

	// set some default fonts
	if (font.fromString(config.stack_font)) {
		stackView_->setFont(font);
	}

	if (font.fromString(config.registers_font)) {
#if 0
		ui.registerList->setFont(font);
#endif
	}

	if (font.fromString(config.disassembly_font)) {
		ui.cpuView->setFont(font);
	}

	if (font.fromString(config.data_font)) {
		Q_FOREACH (const std::shared_ptr<DataViewInfo> &data_view, dataRegions_) {
			data_view->view->setFont(font);
		}
	}
}

/**
 * creates the add/remove tab buttons in the data view
 *
 * @brief Debugger::setupTabButtons
 */
void Debugger::setupTabButtons() {
	// add the corner widgets to the data view
	tabCreate_ = new QToolButton(ui.tabWidget);
	tabDelete_ = new QToolButton(ui.tabWidget);

	tabCreate_->setToolButtonStyle(Qt::ToolButtonIconOnly);
	tabDelete_->setToolButtonStyle(Qt::ToolButtonIconOnly);
	tabCreate_->setIcon(QIcon::fromTheme("tab-new"));
	tabDelete_->setIcon(QIcon::fromTheme("tab-close"));
	tabCreate_->setAutoRaise(true);
	tabDelete_->setAutoRaise(true);
	tabCreate_->setEnabled(false);
	tabDelete_->setEnabled(false);

	ui.tabWidget->setCornerWidget(tabCreate_, Qt::TopLeftCorner);
	ui.tabWidget->setCornerWidget(tabDelete_, Qt::TopRightCorner);

	connect(tabCreate_, &QToolButton::clicked, this, &Debugger::mnuDumpCreateTab);
	connect(tabDelete_, &QToolButton::clicked, this, &Debugger::mnuDumpDeleteTab);
}

/**
 * @brief Debugger::activeRegister
 * @return
 */
Register Debugger::activeRegister() const {
	const auto &model = edb::v1::arch_processor().registerViewModel();
	const auto index  = model.activeIndex();
	if (!index.data(RegisterViewModelBase::Model::IsNormalRegisterRole).toBool()) {
		return {};
	}

	const auto regName = index.sibling(index.row(), RegisterViewModelBase::Model::NAME_COLUMN).data().toString();
	if (regName.isEmpty()) {
		return {};
	}

	if (IDebugger *core = edb::v1::debugger_core) {
		if (IProcess *process = core->process()) {
			State state;
			process->currentThread()->getState(&state);
			return state[regName];
		}
	}
	return {};
}

//------------------------------------------------------------------------------
// Name: on_registerList_customContextMenuRequested
// Desc: context menu handler for register view
//------------------------------------------------------------------------------
QList<QAction *> Debugger::currentRegisterContextMenuItems() const {
	QList<QAction *> allActions;
	const auto reg = activeRegister();
	if (reg.type() & (Register::TYPE_GPR | Register::TYPE_IP)) {

		QList<QAction *> actions;
		actions << registerFollowInDumpAction_;
		actions << registerFollowInDumpTabAction_;
		actions << registerFollowInStackAction_;

		allActions.append(actions);
	}
	allActions.append(getPluginContextMenuItems(&IPlugin::registerContextMenu));
	return allActions;
}

// Flag-toggling functions.  Not sure if this is the best solution, but it works.

//------------------------------------------------------------------------------
// Name: toggle_flag
// Desc: toggles flag register at bit position pos
// Param: pos The position of the flag bit to toggle
//------------------------------------------------------------------------------
void Debugger::toggleFlag(int pos) {
	// TODO Maybe this should just return w/o action if no process is loaded.

	// Get the state and get the flag register
	if (IProcess *process = edb::v1::debugger_core->process()) {
		if (std::shared_ptr<IThread> thread = process->currentThread()) {
			State state;
			thread->getState(&state);
			edb::reg_t flags = state.flags();

			// Toggle the flag
			flags ^= (1 << pos);
			state.setFlags(flags);
			thread->setState(state);

			updateUi();
			refreshUi();
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_cpuView_breakPointToggled
// Desc: handler for toggling the breakpoints
//------------------------------------------------------------------------------
void Debugger::on_cpuView_breakPointToggled(edb::address_t address) {
	edb::v1::toggle_breakpoint(address);
}

//------------------------------------------------------------------------------
// Name: on_action_About_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_About_triggered() {

	QPointer<DialogAbout> dlg = new DialogAbout(this);
	dlg->exec();
	delete dlg;
}

//------------------------------------------------------------------------------
// Name: apply_default_show_separator
// Desc:
//------------------------------------------------------------------------------
void Debugger::applyDefaultShowSeparator() {
	const bool show = edb::v1::config().show_address_separator;

	ui.cpuView->setShowAddressSeparator(show);
	stackView_->setShowAddressSeparator(show);
	Q_FOREACH (const std::shared_ptr<DataViewInfo> &data_view, dataRegions_) {
		data_view->view->setShowAddressSeparator(show);
	}
}

//------------------------------------------------------------------------------
// Name: on_action_Configure_Debugger_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Configure_Debugger_triggered() {

	edb::v1::dialog_options()->exec();

	// reload symbols in case they changed, or our symbol files changes
	edb::v1::reload_symbols();

	// re-read the memory region information
	edb::v1::memory_regions().sync();

	// apply the selected fonts
	applyDefaultFonts();

	// apply changes to the GUI options
	applyDefaultShowSeparator();

	// show changes
	refreshUi();
}

//----------------------------------------------------------------------
// Name: stepOver
//----------------------------------------------------------------------
template <class F1, class F2>
void Debugger::stepOver(F1 run_func, F2 step_func) {

	if (IProcess *process = edb::v1::debugger_core->process()) {
		if (std::shared_ptr<IThread> thread = process->currentThread()) {
			State state;
			thread->getState(&state);

			const edb::address_t ip = state.instructionPointer();
			uint8_t buffer[edb::Instruction::MaxSize];
			if (const int sz = edb::v1::get_instruction_bytes(ip, buffer)) {
				edb::Instruction inst(buffer, buffer + sz, 0);
				if (inst && edb::v1::arch_processor().canStepOver(inst)) {

					// add a temporary breakpoint at the instruction just
					// after the call
					if (std::shared_ptr<IBreakpoint> bp = edb::v1::debugger_core->addBreakpoint(ip + inst.byteSize())) {
						bp->setInternal(true);
						bp->setOneTime(true);
						bp->tag = stepover_bp_tag;
						run_func();
						return;
					}
				}
			}
		}
	}

	// if all else fails, it's a step into
	step_func();
}

//------------------------------------------------------------------------------
// Name: follow_memory
// Desc:
//------------------------------------------------------------------------------
template <class F>
void Debugger::followMemory(edb::address_t address, F follow_func) {
	if (!follow_func(address)) {
		QMessageBox::critical(this,
							  tr("No Memory Found"),
							  tr("There appears to be no memory at that location (<strong>%1</strong>)").arg(edb::v1::format_pointer(address)));
	}
}

//------------------------------------------------------------------------------
// Name: follow_register_in_dump
// Desc:
//------------------------------------------------------------------------------
void Debugger::followRegisterInDump(bool tabbed) {

	if (const Result<edb::address_t, QString> address = getFollowRegister()) {
		if (!edb::v1::dump_data(*address, tabbed)) {
			QMessageBox::critical(this,
								  tr("No Memory Found"),
								  tr("There appears to be no memory at that location (<strong>%1</strong>)").arg(edb::v1::format_pointer(address.value())));
		}
	}
}

//------------------------------------------------------------------------------
// Name: mnuStackGotoESP
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuStackGotoESP() {
	if (IProcess *process = edb::v1::debugger_core->process()) {
		if (std::shared_ptr<IThread> thread = process->currentThread()) {
			State state;
			thread->getState(&state);
			followMemory(state.stackPointer(), [](edb::address_t address) {
				return edb::v1::dump_stack(address);
			});
		}
	}
}

//------------------------------------------------------------------------------
// Name: mnuStackGotoEBP
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuStackGotoEBP() {
	if (IProcess *process = edb::v1::debugger_core->process()) {
		if (std::shared_ptr<IThread> thread = process->currentThread()) {
			State state;
			thread->getState(&state);
			followMemory(state.framePointer(), [](edb::address_t address) {
				return edb::v1::dump_stack(address);
			});
		}
	}
}

//------------------------------------------------------------------------------
// Name: mnuCPUJumpToEIP
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuCPUJumpToEIP() {
	if (IProcess *process = edb::v1::debugger_core->process()) {
		if (std::shared_ptr<IThread> thread = process->currentThread()) {
			State state;
			thread->getState(&state);
			followMemory(state.instructionPointer(), [](edb::address_t address) {
				return edb::v1::jump_to_address(address);
			});
		}
	}
}

//------------------------------------------------------------------------------
// Name: mnuCPUJumpToAddress
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuCPUJumpToAddress() {

	if (const Result<edb::address_t, QString> address = getGotoExpression()) {
		followMemory(*address, [](edb::address_t address) {
			return edb::v1::jump_to_address(address);
		});
	}
}

//------------------------------------------------------------------------------
// Name: mnuDumpGotoAddress
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuDumpGotoAddress() {
	if (const Result<edb::address_t, QString> address = getGotoExpression()) {
		followMemory(*address, [](edb::address_t address) {
			return edb::v1::dump_data(address);
		});
	}
}

//------------------------------------------------------------------------------
// Name: mnuStackGotoAddress
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuStackGotoAddress() {
	if (const Result<edb::address_t, QString> address = getGotoExpression()) {
		followMemory(*address, [](edb::address_t address) {
			return edb::v1::dump_stack(address);
		});
	}
}

//------------------------------------------------------------------------------
// Name: mnuRegisterFollowInStack
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuRegisterFollowInStack() {

	if (const Result<edb::address_t, QString> address = getFollowRegister()) {
		followMemory(*address, [](edb::address_t address) {
			return edb::v1::dump_stack(address);
		});
	}
}

//------------------------------------------------------------------------------
// Name: getFollowAddress
// Desc:
//------------------------------------------------------------------------------
template <class Ptr>
Result<edb::address_t, QString> Debugger::getFollowAddress(const Ptr &hexview) {

	Q_ASSERT(hexview);

	const size_t pointer_size = edb::v1::pointer_size();

	if (hexview->hasSelectedText()) {
		const QByteArray data = hexview->selectedBytes();

		if (data.size() == static_cast<int>(pointer_size)) {
			edb::address_t d(0);
			std::memcpy(&d, data.data(), pointer_size);

			return d;
		}
	}

	QMessageBox::critical(this,
						  tr("Invalid Selection"),
						  tr("Please select %1 bytes to use this function.").arg(pointer_size));

	return make_unexpected(tr("Invalid Selection"));
}

//------------------------------------------------------------------------------
// Name: followInStack
// Desc:
//------------------------------------------------------------------------------
template <class Ptr>
void Debugger::followInStack(const Ptr &hexview) {

	if (const Result<edb::address_t, QString> address = getFollowAddress(hexview)) {
		followMemory(*address, [](edb::address_t address) {
			return edb::v1::dump_stack(address);
		});
	}
}

//------------------------------------------------------------------------------
// Name: followInDump
// Desc:
//------------------------------------------------------------------------------
template <class Ptr>
void Debugger::followInDump(const Ptr &hexview) {

	if (const Result<edb::address_t, QString> address = getFollowAddress(hexview)) {
		followMemory(*address, [](edb::address_t address) {
			return edb::v1::dump_data(address);
		});
	}
}

//------------------------------------------------------------------------------
// Name: followInCpu
// Desc:
//------------------------------------------------------------------------------
template <class Ptr>
void Debugger::followInCpu(const Ptr &hexview) {

	if (const Result<edb::address_t, QString> address = getFollowAddress(hexview)) {
		followMemory(*address, [](edb::address_t address) {
			return edb::v1::jump_to_address(address);
		});
	}
}

//------------------------------------------------------------------------------
// Name: mnuDumpFollowInCPU
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuDumpFollowInCPU() {
	followInCpu(qobject_cast<QHexView *>(ui.tabWidget->currentWidget()));
}

//------------------------------------------------------------------------------
// Name: mnuDumpFollowInDump
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuDumpFollowInDump() {
	followInDump(qobject_cast<QHexView *>(ui.tabWidget->currentWidget()));
}

//------------------------------------------------------------------------------
// Name: mnuDumpFollowInStack
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuDumpFollowInStack() {
	followInStack(qobject_cast<QHexView *>(ui.tabWidget->currentWidget()));
}

//------------------------------------------------------------------------------
// Name: mnuStackFollowInDump
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuStackFollowInDump() {
	followInDump(stackView_);
}

//------------------------------------------------------------------------------
// Name: mnuStackFollowInCPU
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuStackFollowInCPU() {
	followInCpu(stackView_);
}

//------------------------------------------------------------------------------
// Name: mnuStackFollowInStack
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuStackFollowInStack() {
	followInStack(stackView_);
}

//------------------------------------------------------------------------------
// Name: on_actionApplication_Arguments_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_actionApplication_Arguments_triggered() {
	argumentsDialog_->exec();
}

//------------------------------------------------------------------------------
// Name: on_actionApplication_Working_Directory_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_actionApplication_Working_Directory_triggered() {
	const QString new_dir = QFileDialog::getExistingDirectory(
		this,
		tr("Application Working Directory"),
		QString(),
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (!new_dir.isEmpty()) {
		workingDirectory_ = new_dir;
	}
}

//------------------------------------------------------------------------------
// Name: mnuStackPush
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuStackPush() {
	Register value(edb::v1::debuggeeIs32Bit() ? make_Register("", edb::value32(0), Register::TYPE_GPR) : make_Register("", edb::value64(0), Register::TYPE_GPR));

	if (IProcess *process = edb::v1::debugger_core->process()) {
		if (std::shared_ptr<IThread> thread = process->currentThread()) {
			State state;
			thread->getState(&state);

			// ask for a replacement
			if (edb::v1::get_value_from_user(value, tr("Enter value to push"))) {

				// if they said ok, do the push, just like the hardware would do
				edb::v1::push_value(&state, value.valueAsInteger());

				// update the state
				thread->setState(state);
				updateUi();
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: mnuStackPop
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuStackPop() {
	if (IProcess *process = edb::v1::debugger_core->process()) {
		if (std::shared_ptr<IThread> thread = process->currentThread()) {
			State state;
			thread->getState(&state);
			edb::v1::pop_value(&state);
			thread->setState(state);
			updateUi();
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_cpuView_customContextMenuRequested
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_cpuView_customContextMenuRequested(const QPoint &pos) {
	QMenu menu;

	auto displayMenu = new QMenu(tr("Display"), &menu);
	displayMenu->addAction(QIcon::fromTheme(QString::fromUtf8("view-restore")), tr("Restore Column Defaults"), ui.cpuView, SLOT(resetColumns()));

	auto editMenu = new QMenu(tr("&Edit"), &menu);
	editMenu->addAction(editBytesAction_);
	editMenu->addAction(fillWithZerosAction_);
	editMenu->addAction(fillWithNOPsAction_);

	menu.addMenu(displayMenu);
	menu.addMenu(editMenu);

	menu.addAction(editCommentAction_);
	menu.addAction(setAddressLabelAction_);

	menu.addAction(gotoAddressAction_);
	menu.addAction(gotoRIPAction_);

	const edb::address_t address = ui.cpuView->selectedAddress();
	int size                     = ui.cpuView->selectedSize();

	if (IProcess *process = edb::v1::debugger_core->process()) {

		Q_UNUSED(process)

		uint8_t buffer[edb::Instruction::MaxSize + 1];
		if (edb::v1::get_instruction_bytes(address, buffer, &size)) {
			edb::Instruction inst(buffer, buffer + size, address);
			if (inst) {

				if (is_call(inst) || is_jump(inst)) {
					if (is_immediate(inst[0])) {
						menu.addAction(followAction_);
					}

					/*
					if(is_expression(inst.operand(0))) {
						if(inst.operand(0).expression().base == edb::Operand::REG_RIP && inst.operand(0).expression().index == edb::Operand::REG_NULL && inst.operand(0).expression().scale == 1) {
							menu.addAction(followAction_);
							followAction_->setData(static_cast<qlonglong>(address + inst.operand(0).displacement()));
						}
					}
					*/
				} else {
					for (std::size_t i = 0; i < inst.operandCount(); ++i) {
						if (is_immediate(inst[i])) {
							menu.addAction(followConstantInDumpAction_);
							menu.addAction(followConstantInStackAction_);

							followConstantInDumpAction_->setData(static_cast<qlonglong>(util::to_unsigned(inst[i]->imm)));
							followConstantInStackAction_->setData(static_cast<qlonglong>(util::to_unsigned(inst[i]->imm)));
						}
					}
				}
			}
		}
	}

	menu.addSeparator();
	menu.addAction(setRIPAction_);
	menu.addAction(runToThisLineAction_);
	menu.addAction(runToLinePassAction_);
	menu.addSeparator();
	menu.addSeparator();
	menu.addAction(toggleBreakpointAction_);
	menu.addAction(conditionalBreakpointAction_);

	addPluginContextMenu(&menu, &IPlugin::cpuContextMenu);

	menu.exec(ui.cpuView->viewport()->mapToGlobal(pos));
}

//------------------------------------------------------------------------------
// Name: mnuCPUFollow
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuCPUFollow() {

	const edb::address_t address = ui.cpuView->selectedAddress();
	int size                     = ui.cpuView->selectedSize();
	uint8_t buffer[edb::Instruction::MaxSize + 1];
	if (!edb::v1::get_instruction_bytes(address, buffer, &size))
		return;

	const edb::Instruction inst(buffer, buffer + size, address);
	if (!is_call(inst) && !is_jump(inst))
		return;
	if (!is_immediate(inst[0]))
		return;

	const edb::address_t addressToFollow = util::to_unsigned(inst[0]->imm);
	if (auto action = qobject_cast<QAction *>(sender())) {
		Q_UNUSED(action)
		followMemory(addressToFollow, edb::v1::jump_to_address);
	}
}

//------------------------------------------------------------------------------
// Name: mnuCPUFollowInDump
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuCPUFollowInDump() {
	if (auto action = qobject_cast<QAction *>(sender())) {
		const edb::address_t address = action->data().toULongLong();
		followMemory(address, [](edb::address_t address) {
			return edb::v1::dump_data(address);
		});
	}
}

//------------------------------------------------------------------------------
// Name: mnuCPUFollowInStack
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuCPUFollowInStack() {
	if (auto action = qobject_cast<QAction *>(sender())) {
		const edb::address_t address = action->data().toULongLong();
		followMemory(address, [](edb::address_t address) {
			return edb::v1::dump_stack(address);
		});
	}
}

//------------------------------------------------------------------------------
// Name: mnuStackToggleLock
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuStackToggleLock(bool locked) {
	stackViewLocked_ = locked;
}

//------------------------------------------------------------------------------
// Name: mnuStackContextMenu
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuStackContextMenu(const QPoint &pos) {

	QMenu *const menu = stackView_->createStandardContextMenu();

	menu->addSeparator();
	menu->addAction(stackFollowInCPUAction_);
	menu->addAction(stackFollowInDumpAction_);
	menu->addAction(stackFollowInStackAction_);
	menu->addAction(gotoAddressAction_);
	menu->addAction(stackGotoRSPAction_);
	menu->addAction(stackGotoRBPAction_);

	menu->addSeparator();
	menu->addAction(editBytesAction_);
	menu->addSeparator();
	menu->addAction(stackPushAction_);
	menu->addAction(stackPopAction_);

	// lockable stack feature
	menu->addSeparator();
	auto action = new QAction(tr("&Lock Stack"), this);
	action->setCheckable(true);
	action->setChecked(stackViewLocked_);
	menu->addAction(action);
	connect(action, &QAction::toggled, this, &Debugger::mnuStackToggleLock);

	addPluginContextMenu(menu, &IPlugin::stackContextMenu);

	menu->exec(stackView_->mapToGlobal(pos));
	delete menu;
}

//------------------------------------------------------------------------------
// Name: mnuDumpContextMenu
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuDumpContextMenu(const QPoint &pos) {
	auto s = qobject_cast<QHexView *>(sender());

	Q_ASSERT(s);

	QMenu *const menu = s->createStandardContextMenu();
	menu->addSeparator();
	menu->addAction(dumpFollowInCPUAction_);
	menu->addAction(dumpFollowInDumpAction_);
	menu->addAction(dumpFollowInStackAction_);
	menu->addAction(gotoAddressAction_);
	menu->addSeparator();
	menu->addAction(editBytesAction_);
	menu->addSeparator();
	menu->addAction(dumpSaveToFileAction_);

	addPluginContextMenu(menu, &IPlugin::dataContextMenu);

	menu->exec(s->mapToGlobal(pos));
	delete menu;
}

//------------------------------------------------------------------------------
// Name: mnuDumpSaveToFile
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuDumpSaveToFile() {
	auto s = qobject_cast<QHexView *>(ui.tabWidget->currentWidget());

	Q_ASSERT(s);

	const QString filename = QFileDialog::getSaveFileName(
		this,
		tr("Save File"),
		lastOpenDirectory_);

	if (!filename.isEmpty()) {
		QFile file(filename);
		file.open(QIODevice::WriteOnly);
		if (file.isOpen()) {
			file.write(s->allBytes());
		}
	}
}

//------------------------------------------------------------------------------
// Name: cpu_fill
// Desc:
//------------------------------------------------------------------------------
void Debugger::cpuFill(uint8_t byte) {
	const edb::address_t address = ui.cpuView->selectedAddress();
	const unsigned int size      = ui.cpuView->selectedSize();

	if (size != 0) {
		if (IProcess *process = edb::v1::debugger_core->process()) {
			if (edb::v1::overwrite_check(address, size)) {
				QByteArray bytes(size, byte);

				process->writeBytes(address, bytes.data(), size);

				// do a refresh, not full update
				refreshUi();
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: mnuCPUEditComment
// Desc: Adds/edits a comment at the selected address.
//------------------------------------------------------------------------------
void Debugger::mnuCPUEditComment() {
	const edb::address_t address = ui.cpuView->selectedAddress();

	bool got_text;
	const QString comment = QInputDialog::getText(
		this,
		tr("Edit Comment"),
		tr("Comment:"),
		QLineEdit::Normal,
		ui.cpuView->getComment(address),
		&got_text);

	//If we got a comment, add it.
	if (got_text && !comment.isEmpty()) {
		ui.cpuView->addComment(address, comment);
	} else if (got_text && comment.isEmpty()) {
		//If the user backspaced the comment, remove the comment since
		//there's no need for a null string to take space in the hash.
		ui.cpuView->removeComment(address);
	} else {
		//The only other real case is that we didn't got_text.  No change.
		return;
	}

	refreshUi();
}

//------------------------------------------------------------------------------
// Name: mnuCPURemoveComment
// Desc: Removes a comment at the selected address.
//------------------------------------------------------------------------------
void Debugger::mnuCPURemoveComment() {
	const edb::address_t address = ui.cpuView->selectedAddress();
	ui.cpuView->removeComment(address);
	refreshUi();
}

//------------------------------------------------------------------------------
// Name: run_to_this_line
// Desc:
//------------------------------------------------------------------------------
void Debugger::runToThisLine(ExceptionResume pass_signal) {
	const edb::address_t address    = ui.cpuView->selectedAddress();
	std::shared_ptr<IBreakpoint> bp = edb::v1::find_breakpoint(address);
	if (!bp) {
		bp = edb::v1::create_breakpoint(address);
		if (!bp) return;
		bp->setOneTime(true);
		bp->setInternal(true);
		bp->tag = run_to_cursor_tag;
	}

	resumeExecution(pass_signal, Run, ResumeFlag::None);
}

//------------------------------------------------------------------------------
// Name: mnuCPURunToThisLinePassSignal
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuCPURunToThisLinePassSignal() {
	runToThisLine(PassException);
}

//------------------------------------------------------------------------------
// Name: mnuCPURunToThisLine
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuCPURunToThisLine() {
	runToThisLine(IgnoreException);
}

//------------------------------------------------------------------------------
// Name: mnuCPUToggleBreakpoint
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuCPUToggleBreakpoint() {
	const edb::address_t address = ui.cpuView->selectedAddress();
	edb::v1::toggle_breakpoint(address);
}

//------------------------------------------------------------------------------
// Name: mnuCPUAddConditionalBreakpoint
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuCPUAddConditionalBreakpoint() {
	bool ok;
	const edb::address_t address = ui.cpuView->selectedAddress();

	const QString condition = QInputDialog::getText(this, tr("Set Breakpoint Condition"), tr("Expression:"), QLineEdit::Normal, QString(), &ok);
	if (ok) {
		if (std::shared_ptr<IBreakpoint> bp = edb::v1::create_breakpoint(address)) {
			if (!condition.isEmpty()) {
				bp->condition = condition;
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: mnuCPURemoveBreakpoint
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuCPURemoveBreakpoint() {
	const edb::address_t address = ui.cpuView->selectedAddress();
	edb::v1::remove_breakpoint(address);
}

//------------------------------------------------------------------------------
// Name: mnuCPUFillZero
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuCPUFillZero() {
	cpuFill(0x00);
}

//------------------------------------------------------------------------------
// Name: mnuCPUFillNop
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuCPUFillNop() {
	if (IDebugger *core = edb::v1::debugger_core) {
		cpuFill(core->nopFillByte());
	}
}

//------------------------------------------------------------------------------
// Name: mnuCPULabelAddress
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuCPULabelAddress() {

	const edb::address_t address = ui.cpuView->selectedAddress();

	bool ok;
	const QString text = QInputDialog::getText(
		this,
		tr("Set Label"),
		tr("Label:"),
		QLineEdit::Normal,
		edb::v1::symbol_manager().findAddressName(address),
		&ok);

	if (ok) {
		edb::v1::symbol_manager().setLabel(address, text);
		refreshUi();
	}
}

//------------------------------------------------------------------------------
// Name: mnuCPUSetEIP
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuCPUSetEIP() {
	if (IProcess *process = edb::v1::debugger_core->process()) {
		if (std::shared_ptr<IThread> thread = process->currentThread()) {
			const edb::address_t address = ui.cpuView->selectedAddress();
			State state;
			thread->getState(&state);
			state.setInstructionPointer(address);
			thread->setState(state);
			updateUi();
		}
	}
}

//------------------------------------------------------------------------------
// Name: mnuCPUModify
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuCPUModify() {
	const edb::address_t address = ui.cpuView->selectedAddress();
	const unsigned int size      = ui.cpuView->selectedSize();

	uint8_t buf[edb::Instruction::MaxSize];

	Q_ASSERT(size <= sizeof(buf));

	if (IProcess *process = edb::v1::debugger_core->process()) {
		const bool ok = process->readBytes(address, buf, size);
		if (ok) {
			QByteArray bytes = QByteArray::fromRawData(reinterpret_cast<const char *>(buf), size);
			if (edb::v1::get_binary_string_from_user(bytes, tr("Edit Binary String"))) {
				edb::v1::modify_bytes(address, bytes.size(), bytes, 0x00);
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: modifyBytes
// Desc:
//------------------------------------------------------------------------------
template <class Ptr>
void Debugger::modifyBytes(const Ptr &hexview) {
	if (hexview) {
		const edb::address_t address = hexview->selectedBytesAddress();
		QByteArray bytes             = hexview->selectedBytes();

		if (edb::v1::get_binary_string_from_user(bytes, tr("Edit Binary String"))) {
			edb::v1::modify_bytes(address, bytes.size(), bytes, 0x00);
		}
	}
}

//------------------------------------------------------------------------------
// Name: mnuDumpModify
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuModifyBytes() {

	QWidget *const focusedWidget = QApplication::focusWidget();
	if (focusedWidget == ui.cpuView) {
		mnuCPUModify();
	} else if (focusedWidget == stackView_.get()) {
		mnuStackModify();
	} else {
		// not CPU or Stack, assume one of the data views..
		mnuDumpModify();
	}
}

//------------------------------------------------------------------------------
// Name: mnuDumpModify
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuDumpModify() {
	modifyBytes(qobject_cast<QHexView *>(ui.tabWidget->currentWidget()));
}

//------------------------------------------------------------------------------
// Name: mnuStackModify
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuStackModify() {
	modifyBytes(stackView_);
}

//------------------------------------------------------------------------------
// Name: breakpoint_condition_true
// Desc:
//------------------------------------------------------------------------------
bool Debugger::isBreakpointConditionTrue(const QString &condition) {

	if (boost::optional<edb::address_t> condition_value = edb::v2::eval_expression(condition)) {
		return *condition_value;
	}
	return true;
}

//------------------------------------------------------------------------------
// Name: handle_trap
// Desc: returns true if we should resume as if this trap never happened
//------------------------------------------------------------------------------
edb::EventStatus Debugger::handleTrap(const std::shared_ptr<IDebugEvent> &event) {

	// we just got a trap event, there are a few possible causes
	// #1: we hit a 0xcc breakpoint, if so, then we want to stop
	// #2: we did a step
	// #3: we hit a 0xcc naturally in the program
	// #4: we hit a hardware breakpoint!
	IProcess *process = edb::v1::debugger_core->process();
	Q_ASSERT(process);

	State state;
	process->currentThread()->getState(&state);

	// look it up in our breakpoint list, make sure it is one of OUR int3s!
	// if it is, we need to backup EIP and pause ourselves
	const std::shared_ptr<IBreakpoint> bp = event->trapReason() == IDebugEvent::TRAP_STEPPING ? nullptr : edb::v1::find_triggered_breakpoint(state.instructionPointer());

	if (bp && bp->enabled()) {

		const edb::address_t previous_ip = bp->address();

		// TODO: check if the breakpoint was corrupted?
		bp->hit();

		// back up eip the size of a breakpoint, since we executed a breakpoint
		// instead of the real code that belongs there
		state.setInstructionPointer(previous_ip);
		process->currentThread()->setState(state);

#if defined(Q_OS_LINUX)
		// test if we have hit our internal LD hook BP. If so, read in the r_debug
		// struct so we can get the state, then we can just resume
		// TODO(eteran): add an option to let the user stop of debug events
		if (bp->internal() && bp->tag == ld_loader_tag) {

			if (dynamicInfoBreakpointSet_) {
				if (debugtPointer_) {
					if (edb::v1::debuggeeIs32Bit()) {
						handle_library_event<uint32_t>(process, debugtPointer_);
					} else {
						handle_library_event<uint64_t>(process, debugtPointer_);
					}
				}
			}

			if (edb::v1::config().break_on_library_load) {
				return edb::DEBUG_STOP;
			} else {
				return edb::DEBUG_CONTINUE_BP;
			}
		}
#endif

		const QString condition = bp->condition;

		// handle conditional breakpoints
		if (!condition.isEmpty()) {
			if (!isBreakpointConditionTrue(condition)) {
				return edb::DEBUG_CONTINUE_BP;
			}
		}

		// if it's a one time breakpoint then we should remove it upon
		// triggering, this is mainly used for situations like step over

		if (bp->oneTime()) {
			edb::v1::debugger_core->removeBreakpoint(bp->address());
		}
	}

	return edb::DEBUG_STOP;
}

//------------------------------------------------------------------------------
// Name: handle_event_stopped
// Desc:
//------------------------------------------------------------------------------
edb::EventStatus Debugger::handleEventStopped(const std::shared_ptr<IDebugEvent> &event) {

	// ok we just came in from a stop, we need to test some things,
	// generally, we will want to check if it was a step, if it was, was it
	// because we just hit a break point or because we wanted to run, but had
	// to step first in case were were on a breakpoint already...

	edb::v1::clear_status();

	if (event->isKill()) {
		QMessageBox::information(
			this,
			tr("Application Killed"),
			tr("The debugged application was killed."));

		on_action_Detach_triggered();
		return edb::DEBUG_STOP;
	}

	if (event->isError()) {
		const IDebugEvent::Message message = event->errorDescription();
		edb::v1::set_status(message.statusMessage, 0);
		if (edb::v1::config().enable_signals_message_box)
			QMessageBox::information(this, message.caption, message.message);
		return edb::DEBUG_STOP;
	}

	if (event->isTrap()) {
		return handleTrap(event);
	}

	if (event->isStop()) {
		// user asked to pause the debugged process
		return edb::DEBUG_STOP;
	}

	Q_ASSERT(edb::v1::debugger_core);
	QMap<qlonglong, QString> known_exceptions = edb::v1::debugger_core->exceptions();
	auto it                                   = known_exceptions.find(event->code());

	if (it != known_exceptions.end()) {

		const Configuration &config = edb::v1::config();

		QString exception_name = it.value();

		edb::v1::set_status(tr("%1 signal received. Shift+Step/Run to pass to program, Step/Run to ignore").arg(exception_name), 0);
		if (config.enable_signals_message_box) {
			QMessageBox::information(this, tr("Debug Event"),
									 tr(
										 "<p>The debugged application has received a debug event-> <strong>%1 (%2)</strong></p>"
										 "<p>If you would like to pass this event to the application press Shift+[F7/F8/F9]</p>"
										 "<p>If you would like to ignore this event, press [F7/F8/F9]</p>")
										 .arg(event->code())
										 .arg(exception_name));
		}
	} else {
		edb::v1::set_status(tr("Signal received: %1. Shift+Step/Run to pass to program, Step/Run to ignore").arg(event->code()), 0);
		if (edb::v1::config().enable_signals_message_box) {
			QMessageBox::information(this, tr("Debug Event"),
									 tr(
										 "<p>The debugged application has received a debug event-> <strong>%1</strong></p>"
										 "<p>If you would like to pass this event to the application press Shift+[F7/F8/F9]</p>"
										 "<p>If you would like to ignore this event, press [F7/F8/F9]</p>")
										 .arg(event->code()));
		}
	}

	return edb::DEBUG_STOP;
}

//------------------------------------------------------------------------------
// Name: handle_event_terminated
// Desc:
//------------------------------------------------------------------------------
edb::EventStatus Debugger::handleEventTerminated(const std::shared_ptr<IDebugEvent> &event) {
	on_action_Detach_triggered();
	QMessageBox::information(
		this,
		tr("Application Terminated"),
		tr("The debugged application was terminated with exit code %1.").arg(event->code()));

	return edb::DEBUG_STOP;
}

//------------------------------------------------------------------------------
// Name: handle_event_exited
// Desc:
//------------------------------------------------------------------------------
edb::EventStatus Debugger::handleEventExited(const std::shared_ptr<IDebugEvent> &event) {
	on_action_Detach_triggered();
	QMessageBox::information(
		this,
		tr("Application Exited"),
		tr("The debugged application exited normally with exit code %1.").arg(event->code()));

	return edb::DEBUG_STOP;
}

//------------------------------------------------------------------------------
// Name: handle_event
// Desc:
//------------------------------------------------------------------------------
edb::EventStatus Debugger::handleEvent(const std::shared_ptr<IDebugEvent> &event) {

	Q_ASSERT(edb::v1::debugger_core);

	edb::EventStatus status;
	switch (event->reason()) {
	// either a syncronous event (STOPPED)
	// or an asyncronous event (SIGNALED)
	case IDebugEvent::EVENT_STOPPED:
		status = handleEventStopped(event);
		break;

	case IDebugEvent::EVENT_TERMINATED:
		status = handleEventTerminated(event);
		break;

	// normal exit
	case IDebugEvent::EVENT_EXITED:
		status = handleEventExited(event);
		break;

	default:
		Q_ASSERT(false);
		return edb::DEBUG_EXCEPTION_NOT_HANDLED;
	}

	Q_ASSERT(!(reenableBreakpointStep_ && reenableBreakpointRun_));

	// re-enable any breakpoints we previously disabled
	if (reenableBreakpointStep_) {
		reenableBreakpointStep_->enable();
		reenableBreakpointStep_ = nullptr;
	} else if (reenableBreakpointRun_) {
		reenableBreakpointRun_->enable();
		reenableBreakpointRun_ = nullptr;
		status                 = edb::DEBUG_CONTINUE;
	}

	return status;
}

//------------------------------------------------------------------------------
// Name: update_tab_caption
// Desc:
//------------------------------------------------------------------------------
void Debugger::updateTabCaption(const std::shared_ptr<QHexView> &view, edb::address_t start, edb::address_t end) {
	const int index       = ui.tabWidget->indexOf(view.get());
	const QString caption = ui.tabWidget->data(index).toString();

	if (caption.isEmpty()) {
		ui.tabWidget->setTabText(index, tr("%1-%2").arg(edb::v1::format_pointer(start), edb::v1::format_pointer(end)));
	} else {
		ui.tabWidget->setTabText(index, tr("[%1] %2-%3").arg(caption, edb::v1::format_pointer(start), edb::v1::format_pointer(end)));
	}
}

//------------------------------------------------------------------------------
// Name: update_data
// Desc:
//------------------------------------------------------------------------------
void Debugger::updateData(const std::shared_ptr<DataViewInfo> &v) {

	Q_ASSERT(v);

	const std::shared_ptr<QHexView> &view = v->view;

	Q_ASSERT(view);

	v->update();

	updateTabCaption(view, v->region->start(), v->region->end());
}

//------------------------------------------------------------------------------
// Name: clear_data
// Desc:
//------------------------------------------------------------------------------
void Debugger::clearData(const std::shared_ptr<DataViewInfo> &v) {

	Q_ASSERT(v);

	const std::shared_ptr<QHexView> &view = v->view;

	Q_ASSERT(view);

	view->clear();
	view->scrollTo(0);

	updateTabCaption(view, 0, 0);
}

//------------------------------------------------------------------------------
// Name: do_jump_to_address
// Desc:
//------------------------------------------------------------------------------
void Debugger::doJumpToAddress(edb::address_t address, const std::shared_ptr<IRegion> &r, bool scrollTo) {

	ui.cpuView->setRegion(r);
	if (scrollTo && !ui.cpuView->addressShown(address)) {
		ui.cpuView->scrollTo(address);
	}
	ui.cpuView->setSelectedAddress(address);
}

//------------------------------------------------------------------------------
// Name: update_disassembly
// Desc:
//------------------------------------------------------------------------------
void Debugger::updateDisassembly(edb::address_t address, const std::shared_ptr<IRegion> &r) {
	ui.cpuView->setCurrentAddress(address);
	doJumpToAddress(address, r, true);
	listModel_->setStringList(edb::v1::arch_processor().updateInstructionInfo(address));
}

//------------------------------------------------------------------------------
// Name: update_stack_view
// Desc:
//------------------------------------------------------------------------------
void Debugger::updateStackView(const State &state) {
	if (!edb::v1::dump_stack(state.stackPointer(), !stackViewLocked_)) {
		stackView_->clear();
		stackView_->scrollTo(0);
	}
}

//------------------------------------------------------------------------------
// Name: update_cpu_view
// Desc:
//------------------------------------------------------------------------------
std::shared_ptr<IRegion> Debugger::updateCpuView(const State &state) {
	const edb::address_t address = state.instructionPointer();

	if (std::shared_ptr<IRegion> region = edb::v1::memory_regions().findRegion(address)) {
		updateDisassembly(address, region);
		return region;
	} else {
		ui.cpuView->clear();
		ui.cpuView->scrollTo(0);
		listModel_->setStringList(QStringList());
		return nullptr;
	}
}

//------------------------------------------------------------------------------
// Name: update_data_views
// Desc:
//------------------------------------------------------------------------------
void Debugger::updateDataViews() {

	// update all data views with the current region data
	Q_FOREACH (const std::shared_ptr<DataViewInfo> &info, dataRegions_) {

		// make sure the regions are still valid..
		if (info->region && edb::v1::memory_regions().findRegion(info->region->start())) {
			updateData(info);
		} else {
			clearData(info);
		}
	}
}

//------------------------------------------------------------------------------
// Name: refresh_gui
// Desc: refreshes all the different displays
//------------------------------------------------------------------------------
void Debugger::refreshUi() {

	ui.cpuView->update();
	stackView_->update();

	Q_FOREACH (const std::shared_ptr<DataViewInfo> &info, dataRegions_) {
		info->view->update();
	}

	if (edb::v1::debugger_core) {
		State state;

		if (IProcess *process = edb::v1::debugger_core->process()) {
			if (std::shared_ptr<IThread> thread = process->currentThread()) {
				thread->getState(&state);
			}
		}

		listModel_->setStringList(edb::v1::arch_processor().updateInstructionInfo(state.instructionPointer()));
	}
}

//------------------------------------------------------------------------------
// Name: update_gui
// Desc: updates all the different displays
//------------------------------------------------------------------------------
void Debugger::updateUi() {

	if (edb::v1::debugger_core) {

		State state;
		if (IProcess *process = edb::v1::debugger_core->process()) {
			if (std::shared_ptr<IThread> thread = process->currentThread()) {
				thread->getState(&state);
			}
		}

		updateDataViews();
		updateStackView(state);

		if (const std::shared_ptr<IRegion> region = updateCpuView(state)) {
			edb::v1::arch_processor().updateRegisterView(region->name(), state);
		} else {
			edb::v1::arch_processor().updateRegisterView(QString(), state);
		}
	}

	//Signal all connected slots that the GUI has been updated.
	//Useful for plugins with windows that should updated after
	//hitting breakpoints, Step Over, etc.
	Q_EMIT uiUpdated();
}

//------------------------------------------------------------------------------
// Name: resume_status
// Desc:
//------------------------------------------------------------------------------
edb::EventStatus Debugger::resumeStatus(bool pass_exception) {

	if (pass_exception && lastEvent_ && lastEvent_->stopped() && !lastEvent_->isTrap()) {
		return edb::DEBUG_EXCEPTION_NOT_HANDLED;
	} else {
		return edb::DEBUG_CONTINUE;
	}
}

//------------------------------------------------------------------------------
// Name: resume_execution
// Desc: resumes execution, handles the situation of being on a breakpoint as well
//------------------------------------------------------------------------------
void Debugger::resumeExecution(ExceptionResume pass_exception, DebugMode mode, ResumeFlag flags) {

	edb::v1::clear_status();
	Q_ASSERT(edb::v1::debugger_core);

	if (IProcess *process = edb::v1::debugger_core->process()) {
		if (std::shared_ptr<IThread> thread = process->currentThread()) {

			// if necessary pass the trap to the application, otherwise just resume
			// as normal
			const edb::EventStatus status = resumeStatus(pass_exception == PassException);

			// if we are on a breakpoint, disable it
			std::shared_ptr<IBreakpoint> bp;
			if (flags != ResumeFlag::Forced) {
				State state;
				thread->getState(&state);
				bp = edb::v1::debugger_core->findBreakpoint(state.instructionPointer());
				if (bp) {
					bp->disable();
				}
			}

			edb::v1::arch_processor().aboutToResume();

			if (mode == Step) {
				reenableBreakpointStep_ = bp;
				const auto stepStatus   = thread->step(status);
				if (!stepStatus) {
					QMessageBox::critical(this, tr("Error"), tr("Failed to step thread: %1").arg(stepStatus.error()));
					return;
				}
			} else if (mode == Run) {
				reenableBreakpointRun_ = bp;
				if (bp) {
					const auto stepStatus = thread->step(status);
					if (!stepStatus) {
						QMessageBox::critical(this, tr("Error"), tr("Failed to step thread: %1").arg(stepStatus.error()));
						return;
					}
				} else {
					const auto resumeStatus = process->resume(status);
					if (!resumeStatus) {
						QMessageBox::critical(this, tr("Error"), tr("Failed to resume process: %1").arg(resumeStatus.error()));
						return;
					}
				}
			}

			// set the state to 'running'
			updateMenuState(Running);
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_action_Run_Pass_Signal_To_Application_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Run_Pass_Signal_To_Application_triggered() {
	resumeExecution(PassException, Run, ResumeFlag::None);
}

//------------------------------------------------------------------------------
// Name: on_action_Step_Into_Pass_Signal_To_Application_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Step_Into_Pass_Signal_To_Application_triggered() {
	resumeExecution(PassException, Step, ResumeFlag::None);
}

//------------------------------------------------------------------------------
// Name: on_action_Run_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Run_triggered() {
	resumeExecution(IgnoreException, Run, ResumeFlag::None);
}

//------------------------------------------------------------------------------
// Name: on_action_Step_Into_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Step_Into_triggered() {
	resumeExecution(IgnoreException, Step, ResumeFlag::None);
}

//------------------------------------------------------------------------------
// Name: on_action_Detach_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Detach_triggered() {
	detachFromProcess(NoKillOnDetach);
}

//------------------------------------------------------------------------------
// Name: on_action_Kill_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Kill_triggered() {
	detachFromProcess(KillOnDetach);
}

//------------------------------------------------------------------------------
// Name: on_action_Step_Over_Pass_Signal_To_Application_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Step_Over_Pass_Signal_To_Application_triggered() {
	stepOver([this]() { on_action_Run_Pass_Signal_To_Application_triggered(); },
			 [this]() { on_action_Step_Into_Pass_Signal_To_Application_triggered(); });
}

//------------------------------------------------------------------------------
// Name: on_action_Step_Over_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Step_Over_triggered() {
	stepOver([this]() { on_action_Run_triggered(); },
			 [this]() { on_action_Step_Into_triggered(); });
}

//------------------------------------------------------------------------------
// Name: on_actionStep_Out_triggered
// Desc: Step out is the same as run until return, in our context.
//------------------------------------------------------------------------------
void Debugger::on_actionStep_Out_triggered() {
	on_actionRun_Until_Return_triggered();
}

//------------------------------------------------------------------------------
// Name: on_actionRun_Until_Return_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_actionRun_Until_Return_triggered() {

	new RunUntilRet();

	//Step over rather than resume in MODE_STEP so that we can avoid stepping into calls.
	//TODO: If we are sitting on the call and it has a bp, it steps over for some reason...
	stepOver([this]() { on_action_Run_triggered(); },
			 [this]() { on_action_Step_Into_triggered(); });
}

//------------------------------------------------------------------------------
// Name: on_action_Pause_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Pause_triggered() {
	Q_ASSERT(edb::v1::debugger_core);
	if (IProcess *process = edb::v1::debugger_core->process()) {
		process->pause();
	}
}

//------------------------------------------------------------------------------
// Name: cleanup_debugger
// Desc:
//------------------------------------------------------------------------------
void Debugger::cleanupDebugger() {

	timer_->stop();

	ui.cpuView->clearComments();
	edb::v1::memory_regions().clear();
	edb::v1::symbol_manager().clear();
	edb::v1::arch_processor().reset();

	// clear up the data view
	while (ui.tabWidget->count() > 1) {
		mnuDumpDeleteTab();
	}

	ui.tabWidget->setData(0, QString());

	Q_ASSERT(!dataRegions_.isEmpty());
	dataRegions_.first()->region = nullptr;

	Q_EMIT detachEvent();

	setWindowTitle(tr("edb"));

	updateUi();
}

//------------------------------------------------------------------------------
// Name: session_filename
// Desc:
//------------------------------------------------------------------------------
QString Debugger::sessionFilename() const {

	static bool show_path_notice = true;

	QString session_path = edb::v1::config().session_path;
	if (session_path.isEmpty()) {
		if (show_path_notice) {
			qDebug() << "No session path specified. Please set it in the preferences to enable sessions.";
			show_path_notice = false;
		}
		return QString();
	}

	if (!programExecutable_.isEmpty()) {
		QFileInfo info(programExecutable_);

		if (info.isRelative()) {
			info.makeAbsolute();
		}

		auto path          = tr("%1/%2").arg(session_path, info.absolutePath());
		const QString name = info.fileName();

		// ensure that the sub-directory exists
		QDir().mkpath(path);

		return tr("%1/%2.edb").arg(path, name);
	}

	return QString();
}

//------------------------------------------------------------------------------
// Name: detach_from_process
// Desc:
//------------------------------------------------------------------------------
void Debugger::detachFromProcess(DetachAction kill) {

	const QString filename = sessionFilename();
	if (!filename.isEmpty()) {
		SessionManager::instance().saveSession(filename);
	}

	programExecutable_.clear();

	if (edb::v1::debugger_core) {
		if (kill == KillOnDetach)
			edb::v1::debugger_core->kill();
		else
			edb::v1::debugger_core->detach();
	}

	lastEvent_ = nullptr;

	cleanupDebugger();
	updateMenuState(Terminated);
}

//------------------------------------------------------------------------------
// Name: set_initial_debugger_state
// Desc: resets all of the basic data to sane defaults
//------------------------------------------------------------------------------
void Debugger::setInitialDebuggerState() {

	updateMenuState(Paused);
	timer_->start(0);

	edb::v1::symbol_manager().clear();
	edb::v1::memory_regions().sync();

	Q_ASSERT(dataRegions_.size() > 0);

	dataRegions_.first()->region = edb::v1::primary_data_region();

	if (IAnalyzer *const analyzer = edb::v1::analyzer()) {
		analyzer->invalidateAnalysis();
	}

	reenableBreakpointRun_  = nullptr;
	reenableBreakpointStep_ = nullptr;

#ifdef Q_OS_LINUX
	debugtPointer_            = 0;
	dynamicInfoBreakpointSet_ = false;
#endif

	IProcess *process = edb::v1::debugger_core->process();

	const QString executable = process ? process->executable() : QString();

	setDebuggerCaption(executable);

	programExecutable_.clear();
	if (!executable.isEmpty()) {
		programExecutable_ = executable;
	}

	const QString filename = sessionFilename();
	if (!filename.isEmpty()) {

		SessionManager &session_manager = SessionManager::instance();

		if (Result<void, SessionError> session_error = session_manager.loadSession(filename)) {
			QVariantList comments_data = session_manager.comments();
			ui.cpuView->restoreComments(comments_data);
		} else {
			QMessageBox::warning(
				this,
				tr("Error Loading Session"),
				session_error.error().message);
		}
	}

	// create our binary info object for the primary code module
	binaryInfo_ = edb::v1::get_binary_info(edb::v1::primary_code_region());

	commentServer_->clear();
	commentServer_->setComment(process->entryPoint(), "<entry point>");
}

//------------------------------------------------------------------------------
// Name: test_native_binary
// Desc:
//------------------------------------------------------------------------------
void Debugger::testNativeBinary() {
	if (EDB_IS_32_BIT && binaryInfo_ && !binaryInfo_->native()) {
		QMessageBox::warning(
			this,
			tr("Not A Native Binary"),
			tr("The program you just attached to was built for a different architecture than the one that edb was built for. "
			   "For example a AMD64 binary on EDB built for IA32. "
			   "This is not fully supported yet, so you may need to use a version of edb that was compiled for the same architecture as your target program"));
	}
}

//------------------------------------------------------------------------------
// Name: set_initial_breakpoint
// Desc: sets the initial breakpoint so we can stop at the entry point of the
//       application
//------------------------------------------------------------------------------
void Debugger::setInitialBreakpoint(const QString &s) {

	edb::address_t entryPoint = 0;

	if (edb::v1::config().initial_breakpoint == Configuration::MainSymbol) {
		const QString mainSymbol          = QFileInfo(s).fileName() + "!main";
		const std::shared_ptr<Symbol> sym = edb::v1::symbol_manager().find(mainSymbol);

		if (sym) {
			entryPoint = sym->address;
		} else if (edb::v1::config().find_main) {
			entryPoint = edb::v1::locate_main_function();
		}
	}

	if (entryPoint == 0 || edb::v1::config().initial_breakpoint == Configuration::EntryPoint) {
		entryPoint = edb::v1::debugger_core->process()->entryPoint();
	}

	if (entryPoint != 0) {
		if (std::shared_ptr<IBreakpoint> bp = edb::v1::debugger_core->addBreakpoint(entryPoint)) {
			bp->setOneTime(true);
			bp->setInternal(true);
			bp->tag = initial_bp_tag;
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_action_Restart_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Restart_triggered() {

	Q_ASSERT(edb::v1::debugger_core);
	if (edb::v1::debugger_core->process()) {

		workingDirectory_      = edb::v1::debugger_core->process()->currentWorkingDirectory();
		QList<QByteArray> args = edb::v1::debugger_core->process()->arguments();
		const QString exe      = edb::v1::debugger_core->process()->executable();
		const QString in       = edb::v1::debugger_core->process()->stardardInput();
		const QString out      = edb::v1::debugger_core->process()->stardardOutput();

		if (!args.empty()) {
			args.removeFirst();
		}

		if (!exe.isEmpty()) {
			detachFromProcess(KillOnDetach);
			commonOpen(exe, args, in, out);
		}
	} else {
		// TODO(eteran) pulling from the recent file mananger "works", but has
		// a weird side effect, in that you can "restart" a process before you have
		// run ANY, as long as your history isn't empty
		const RecentFileManager::RecentFile file = recentFileManager_->mostRecent();
		if (commonOpen(file.first, file.second, QString(), QString())) {
			argumentsDialog_->setArguments(file.second);
		}
	}
}

//------------------------------------------------------------------------------
// Name: setup_data_views
// Desc:
//------------------------------------------------------------------------------
void Debugger::setupDataViews() {

	// Setup data views according to debuggee bitness
	if (edb::v1::debuggeeIs64Bit()) {
		stackView_->setAddressSize(QHexView::Address64);
		Q_FOREACH (const std::shared_ptr<DataViewInfo> &data_view, dataRegions_) {
			data_view->view->setAddressSize(QHexView::Address64);
		}
	} else {
		stackView_->setAddressSize(QHexView::Address32);
		Q_FOREACH (const std::shared_ptr<DataViewInfo> &data_view, dataRegions_) {
			data_view->view->setAddressSize(QHexView::Address32);
		}
	}

	// Update stack word width
	stackView_->setWordWidth(edb::v1::pointer_size());
}

//------------------------------------------------------------------------------
// Name: common_open
// Desc:
//------------------------------------------------------------------------------
bool Debugger::commonOpen(const QString &s, const QList<QByteArray> &args, const QString &input, const QString &output) {

	// ensure that the previous running process (if any) is dealth with...
	detachFromProcess(KillOnDetach);

	bool ret = false;
	ttyFile_ = createTty();

	QString process_input  = input.isNull() ? ttyFile_ : input;
	QString process_output = output.isNull() ? ttyFile_ : output;

	if (const Status status = edb::v1::debugger_core->open(s, workingDirectory_, args, process_input, process_output)) {
		attachComplete();
		setInitialBreakpoint(s);
		ret = true;
	} else {
		QMessageBox::critical(
			this,
			tr("Could Not Open"),
			tr("Failed to open and attach to process:\n%1.").arg(status.error()));
	}

	updateUi();
	return ret;
}

//------------------------------------------------------------------------------
// Name: execute
// Desc:
//------------------------------------------------------------------------------
void Debugger::execute(const QString &program, const QList<QByteArray> &args, const QString &input, const QString &output) {
	if (commonOpen(program, args, input, output)) {
		recentFileManager_->addFile(program, args);
		argumentsDialog_->setArguments(args);
	}
}

//------------------------------------------------------------------------------
// Name: open_file
// Desc:
//------------------------------------------------------------------------------
void Debugger::openFile(const QString &filename, const QList<QByteArray> &args) {
	if (!filename.isEmpty()) {
		lastOpenDirectory_ = QFileInfo(filename).canonicalFilePath();

		execute(filename, args, QString(), QString());
	}
}

//------------------------------------------------------------------------------
// Name: attach
// Desc:
//------------------------------------------------------------------------------
void Debugger::attach(edb::pid_t pid) {
#if defined(Q_OS_UNIX)
	edb::pid_t current_pid = getpid();
	while (current_pid != 0) {
		if (current_pid == pid) {

			int ret = QMessageBox::question(this,
											tr("Attaching to parent"),
											tr("You are attempting to attach to a process which is a parent of edb, sometimes, this can lead to deadlocks. Do you want to proceed?"),
											QMessageBox::Yes | QMessageBox::No);
			if (ret != QMessageBox::Yes) {
				return;
			}
		}
		current_pid = edb::v1::debugger_core->parentPid(current_pid);
	}
#endif

	if (IProcess *process = edb::v1::debugger_core->process()) {
		if (pid == process->pid()) {
			QMessageBox::critical(
				this,
				tr("Attach"),
				tr("You are already debugging that process!"));
			return;
		}
	}

	if (const auto status = edb::v1::debugger_core->attach(pid)) {

		workingDirectory_ = edb::v1::debugger_core->process()->currentWorkingDirectory();

		QList<QByteArray> args = edb::v1::debugger_core->process()->arguments();

		if (!args.empty()) {
			args.removeFirst();
		}

		argumentsDialog_->setArguments(args);
		attachComplete();
	} else {
		QMessageBox::critical(this, tr("Attach"), tr("Failed to attach to process: %1").arg(status.error()));
	}

	updateUi();
}

//------------------------------------------------------------------------------
// Name: attachComplete
// Desc:
//------------------------------------------------------------------------------
void Debugger::attachComplete() {
	setInitialDebuggerState();

	testNativeBinary();

	setupDataViews();

	QString ip   = edb::v1::debugger_core->instructionPointer().toUpper();
	QString sp   = edb::v1::debugger_core->stackPointer().toUpper();
	QString bp   = edb::v1::debugger_core->framePointer().toUpper();
	QString word = edb::v1::debuggeeIs64Bit() ? "QWORD" : "DWORD";

	setRIPAction_->setText(tr("&Set %1 to this Instruction").arg(ip));
	gotoRIPAction_->setText(tr("&Goto %1").arg(ip));
	stackGotoRSPAction_->setText(tr("Goto %1").arg(sp));
	stackGotoRBPAction_->setText(tr("Goto %1").arg(bp));
	stackPushAction_->setText(tr("&Push %1").arg(word));
	stackPopAction_->setText(tr("P&op %1").arg(word));

	Q_EMIT attachEvent();
}

//------------------------------------------------------------------------------
// Name: on_action_Open_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Open_triggered() {

	static auto dialog = new DialogOpenProgram(this, tr("Choose a file"), lastOpenDirectory_);

	// Set a sensible default dir
	if (recentFileManager_->entryCount() > 0) {
		const RecentFileManager::RecentFile file = recentFileManager_->mostRecent();
		const QDir dir                           = QFileInfo(file.first).dir();
		if (dir.exists()) {
			dialog->setDirectory(dir);
		}
	}

	if (dialog->exec() == QDialog::Accepted) {

		argumentsDialog_->setArguments(dialog->arguments());
		QStringList files      = dialog->selectedFiles();
		const QString filename = files.front();
		workingDirectory_      = dialog->workingDirectory();
		openFile(filename, dialog->arguments());
	}
}

//------------------------------------------------------------------------------
// Name: on_action_Attach_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Attach_triggered() {

	QPointer<DialogAttach> dlg = new DialogAttach(this);

	if (dlg->exec() == QDialog::Accepted) {
		if (dlg) {
			if (const Result<edb::pid_t, QString> pid = dlg->selectedPid()) {
				attach(*pid);
			}
		}
	}

	delete dlg;
}

//------------------------------------------------------------------------------
// Name: on_action_Memory_Regions_triggered
// Desc: displays the memory regions dialog, and optionally dumps some data
//------------------------------------------------------------------------------
void Debugger::on_action_Memory_Regions_triggered() {
	static QPointer<DialogMemoryRegions> dlg = new DialogMemoryRegions(this);
	dlg->show();
}

//------------------------------------------------------------------------------
// Name: on_action_Threads_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Threads_triggered() {
	static QPointer<DialogThreads> dlg = new DialogThreads(this);
	dlg->show();
}

//------------------------------------------------------------------------------
// Name: mnuDumpCreateTab
// Desc: duplicates the current tab creating a new one
//------------------------------------------------------------------------------
void Debugger::mnuDumpCreateTab() {
	createDataTab();
	tabDelete_->setEnabled(ui.tabWidget->count() > 1);
}

//------------------------------------------------------------------------------
// Name: mnuDumpDeleteTab
// Desc: handles removing of a memory view tab
//------------------------------------------------------------------------------
void Debugger::mnuDumpDeleteTab() {
	deleteDataTab();
	tabDelete_->setEnabled(ui.tabWidget->count() > 1);
}

//------------------------------------------------------------------------------
// Name: getPluginContextMenuItems
// Desc: Returns context menu items using supplied function to call for each plugin.
//       NULL pointer items mean "create separator here".
//------------------------------------------------------------------------------
template <class F>
QList<QAction *> Debugger::getPluginContextMenuItems(const F &f) const {
	QList<QAction *> actions;

	for (QObject *plugin : edb::v1::plugin_list()) {
		if (auto p = qobject_cast<IPlugin *>(plugin)) {
			const QList<QAction *> acts = (p->*f)();
			if (!acts.isEmpty()) {
				actions.push_back(nullptr);
				actions.append(acts);
			}
		}
	}
	return actions;
}

//------------------------------------------------------------------------------
// Name: addPluginContextMenu
// Desc:
//------------------------------------------------------------------------------
template <class F, class T>
void Debugger::addPluginContextMenu(const T &menu, const F &f) {
	for (QObject *plugin : edb::v1::plugin_list()) {
		if (auto p = qobject_cast<IPlugin *>(plugin)) {
			const QList<QAction *> acts = (p->*f)();
			if (!acts.isEmpty()) {
				menu->addSeparator();
				menu->addActions(acts);
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_action_Plugins_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Plugins_triggered() {
	static auto dlg = new DialogPlugins(this);
	dlg->show();
}

//------------------------------------------------------------------------------
// Name: jump_to_address
// Desc:
//------------------------------------------------------------------------------
bool Debugger::jumpToAddress(edb::address_t address) {

	if (std::shared_ptr<IRegion> region = edb::v1::memory_regions().findRegion(address)) {
		doJumpToAddress(address, region, true);
		return true;
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: dump_data_range
// Desc:
//------------------------------------------------------------------------------
bool Debugger::dumpDataRange(edb::address_t address, edb::address_t end_address, bool new_tab) {

	if (std::shared_ptr<IRegion> region = edb::v1::memory_regions().findRegion(address)) {
		if (new_tab) {
			mnuDumpCreateTab();
		}

		if (std::shared_ptr<DataViewInfo> info = currentDataViewInfo()) {
			info->region = std::shared_ptr<IRegion>(region->clone());

			if (info->region->contains(end_address)) {
				info->region->setEnd(end_address);
			}

			if (info->region->contains(address)) {
				info->region->setStart(address);
			}

			updateData(info);
			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: dump_data
// Desc:
//------------------------------------------------------------------------------
bool Debugger::dumpData(edb::address_t address, bool new_tab) {

	if (std::shared_ptr<IRegion> region = edb::v1::memory_regions().findRegion(address)) {
		if (new_tab) {
			mnuDumpCreateTab();
		}

		std::shared_ptr<DataViewInfo> info = currentDataViewInfo();

		if (info) {
			info->region = region;
			updateData(info);
			info->view->scrollTo(address - info->region->start());
			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: dump_stack
// Desc:
//------------------------------------------------------------------------------
bool Debugger::dumpStack(edb::address_t address, bool scroll_to) {
	const std::shared_ptr<IRegion> last_region = stackViewInfo_.region;
	stackViewInfo_.region                      = edb::v1::memory_regions().findRegion(address);

	if (stackViewInfo_.region) {
		stackViewInfo_.update();

		if (IProcess *process = edb::v1::debugger_core->process()) {
			if (std::shared_ptr<IThread> thread = process->currentThread()) {

				State state;
				thread->getState(&state);
				stackView_->setColdZoneEnd(state.stackPointer());

				if (scroll_to || stackViewInfo_.region->equals(last_region)) {
					stackView_->scrollTo(address - stackViewInfo_.region->start());
				}
				return true;
			}
		}
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: tab_context_menu
// Desc:
//------------------------------------------------------------------------------
void Debugger::tabContextMenu(int index, const QPoint &pos) {
	QMenu menu;
	QAction *const actionAdd   = menu.addAction(tr("&Set Label"));
	QAction *const actionClear = menu.addAction(tr("&Clear Label"));
	QAction *const chosen      = menu.exec(ui.tabWidget->mapToGlobal(pos));

	if (chosen == actionAdd) {
		bool ok;
		const QString text = QInputDialog::getText(
			this,
			tr("Set Caption"),
			tr("Caption:"),
			QLineEdit::Normal,
			ui.tabWidget->data(index).toString(),
			&ok);

		if (ok && !text.isEmpty()) {
			ui.tabWidget->setData(index, text);
		}
	} else if (chosen == actionClear) {
		ui.tabWidget->setData(index, QString());
	}

	updateUi();
}

//------------------------------------------------------------------------------
// Name: next_debug_event
// Desc:
//------------------------------------------------------------------------------
void Debugger::nextDebugEvent() {

	using namespace std::chrono_literals;

	// TODO(eteran): come up with a nice system to inject a debug event
	//               into the system, for example on windows, we want
	//               to deliver real "memory map updated" events, but
	//               on linux, (at least for now), I would want to send
	//               a fake one before every event so it is always up to
	//               date.

	Q_ASSERT(edb::v1::debugger_core);

	if (std::shared_ptr<IDebugEvent> e = edb::v1::debugger_core->waitDebugEvent(10ms)) {

		lastEvent_ = e;

		// TODO(eteran): disable this in favor of only doing it on library load events
		//               once we are confident. We should be able to just enclose it inside
		//               an "if(!dynamic_info_bp_set_) {" test (since we still want to
		//               do this when the hook isn't set.
		edb::v1::memory_regions().sync();

#if defined(Q_OS_LINUX)
		if (!dynamicInfoBreakpointSet_) {
			if (IProcess *process = edb::v1::debugger_core->process()) {
				if (debugtPointer_ == 0) {
					if ((debugtPointer_ = process->debugPointer()) != 0) {
						edb::address_t r_brk = edb::v1::debuggeeIs32Bit() ? find_linker_hook_address<uint32_t>(process, debugtPointer_) : find_linker_hook_address<uint64_t>(process, debugtPointer_);

						if (r_brk) {
							// TODO(eteran): this is equivalent to ld-2.23.so!_dl_debug_state
							// maybe we should prefer setting this by symbol if possible?
							if (std::shared_ptr<IBreakpoint> bp = edb::v1::debugger_core->addBreakpoint(r_brk)) {
								bp->setInternal(true);
								bp->tag                   = ld_loader_tag;
								dynamicInfoBreakpointSet_ = true;
							}
						}
					}
				}
			}
		}
#endif

		Q_EMIT debugEvent();

		const edb::EventStatus status = edb::v1::execute_debug_event_handlers(e);
		switch (status) {
		case edb::DEBUG_STOP:
			updateUi();
			updateMenuState(edb::v1::debugger_core->process() ? Paused : Terminated);
			break;
		case edb::DEBUG_CONTINUE:
			resumeExecution(IgnoreException, Run, ResumeFlag::Forced);
			break;
		case edb::DEBUG_CONTINUE_BP:
			resumeExecution(IgnoreException, Run, ResumeFlag::None);
			break;
		case edb::DEBUG_CONTINUE_STEP:
			resumeExecution(IgnoreException, Step, ResumeFlag::Forced);
			break;
		case edb::DEBUG_EXCEPTION_NOT_HANDLED:
			resumeExecution(PassException, Run, ResumeFlag::Forced);
			break;
		case edb::DEBUG_NEXT_HANDLER:
			// NOTE(eteran): I don't think this is reachable...
			break;
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_action_Help_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Help_triggered() {
	QDesktopServices::openUrl(QUrl("https://github.com/eteran/edb-debugger/wiki", QUrl::TolerantMode));
}

//------------------------------------------------------------------------------
// Name: statusLabel
// Desc:
//------------------------------------------------------------------------------
QLabel *Debugger::statusLabel() const {
	return status_;
}

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

#include "Debugger.h"
#include "ArchProcessor.h"
#include "CommentServer.h"
#include "Configuration.h"
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
#include "State.h"
#include "Symbol.h"
#include "SymbolManager.h"
#include "SessionManager.h"
#include "SessionError.h"
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
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QSettings>
#include <QShortcut>
#include <QStringListModel>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVector>
#include <QtDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <memory>
#include <cstring>
#include <clocale>

#if defined(Q_OS_UNIX)
#include <signal.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#if defined(Q_OS_LINUX) || defined(Q_OS_OPENBSD) || defined(Q_OS_FREEBSD)
#include <unistd.h>
#include <fcntl.h>
#endif

namespace {

constexpr quint64 initial_bp_tag    = Q_UINT64_C(0x494e4954494e5433); // "INITINT3" in hex
constexpr quint64 stepover_bp_tag   = Q_UINT64_C(0x535445504f564552); // "STEPOVER" in hex
constexpr quint64 run_to_cursor_tag = Q_UINT64_C(0x474f544f48455245); // "GOTOHERE" in hex
#ifdef Q_OS_LINUX
constexpr quint64 ld_loader_tag     = Q_UINT64_C(0x4c49424556454e54); // "LIBEVENT" in hex
#endif

template <class Addr>
void handle_library_event(IProcess *process, edb::address_t debug_pointer) {
#ifdef Q_OS_LINUX
	edb::linux_struct::r_debug<Addr> dynamic_info;
	const bool ok = (process->read_bytes(debug_pointer, &dynamic_info, sizeof(dynamic_info)) == sizeof(dynamic_info));
	if(ok) {

		// NOTE(eteran): at least on my system, the name of
		//               what is being loaded is either in
		//               r8 or r13 depending on which event
		//               we are looking at.
		// TODO(eteran): find a way to get the name reliably

		switch(dynamic_info.r_state) {
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
	const bool ok = process->read_bytes(debug_pointer, &dynamic_info, sizeof(dynamic_info));
	if(ok) {
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

	quint8 buffer[edb::Instruction::MAX_SIZE];
	if(const int size = edb::v1::get_instruction_bytes(address, buffer)) {
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
    RunUntilRet() : last_call_return_(0) {
		edb::v1::add_debug_event_handler(this);
	}

	//--------------------------------------------------------------------------
	// Name: ~RunUntilRet
	//--------------------------------------------------------------------------
	~RunUntilRet() {
		edb::v1::remove_debug_event_handler(this);

		for(const auto& bp : own_breakpoints_) {
			if(!bp.second.expired()) {
				edb::v1::debugger_core->remove_breakpoint(bp.first);
			}
		}
	}

	//--------------------------------------------------------------------------
	// Name: pass_back_to_debugger
	// Desc: Makes the previous handler the event handler again and deletes this.
	//--------------------------------------------------------------------------
	virtual edb::EVENT_STATUS pass_back_to_debugger() {
		delete this;
		return edb::DEBUG_NEXT_HANDLER;
	}

	//--------------------------------------------------------------------------
	// Name: handle_event
	//--------------------------------------------------------------------------
	//TODO: Need to handle stop/pause button
	edb::EVENT_STATUS handle_event(const std::shared_ptr<IDebugEvent> &event) override {

		if(!event->is_trap()) {
			return pass_back_to_debugger();
		}

		if(IProcess *process = edb::v1::debugger_core->process()) {

			State state;
			process->current_thread()->get_state(&state);

			edb::address_t              address = state.instruction_pointer();
			IDebugEvent::TRAP_REASON    trap_reason = event->trap_reason();
			IDebugEvent::REASON         reason = event->reason();

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
				if(bp && bp->enabled()) { //Isn't it always enabled if trap_reason is breakpoint, anyway?

					const edb::address_t prev_address = bp->address();

					bp->hit();

					//Adjust RIP since 1st byte was replaced with 0xcc and we are now 1 byte after it.
					state.set_instruction_pointer(prev_address);
					process->current_thread()->set_state(state);
					address = prev_address;

					//If it wasn't internal, it was a user breakpoint. Pass back to Debugger.
					if (!bp->internal()) {
						qDebug() << "Previous was not an internal breakpoint.";
						return pass_back_to_debugger();
					}
					qDebug() << "Previous was an internal breakpoint.";
					bp->disable();
					edb::v1::debugger_core->remove_breakpoint(bp->address());
				}
				else {
					//No breakpoint if it was a syscall; continue.
					return edb::DEBUG_CONTINUE;
				}
			}

			//If we are on our ret (or the instr after?), then ret.
			if (address == ret_address_) {
				qDebug() << QString("On our terminator at 0x%1").arg(address, 0, 16);
				if (is_instruction_ret(address)) {
					qDebug() << "Found ret; passing back to debugger";
					return pass_back_to_debugger();
				}
				else {
					//If not a ret, then step so we can find the next block terminator.
					qDebug() << "Not ret. Single-stepping";
					return edb::DEBUG_CONTINUE_STEP;
				}
			}

			//If we stepped (either because it was the first event or because we hit a jmp/jcc),
			//then find the next block terminator and edb::DEBUG_CONTINUE.
			//TODO: What if we started on a ret? Set bp, then the proc runs away?
			quint8 buffer[edb::Instruction::MAX_SIZE];
			while (const int size = edb::v1::get_instruction_bytes(address, buffer)) {

				//Get the instruction
				edb::Instruction inst(buffer, buffer + size, 0);
				qDebug() << QString("Scanning for terminator at 0x%1: found %2").arg(
				                address, 0, 16).arg(
				                inst.mnemonic().c_str());

				//Check if it's a proper block terminator (ret/jmp/jcc/hlt)
				if (inst) {
					if (is_terminator(inst)) {
						qDebug() << QString("Found terminator %1 at 0x%2").arg(
						                QString(inst.mnemonic().c_str())).arg(
						                address, 0, 16);
						//If we already had a breakpoint there, then just continue.
						if (std::shared_ptr<IBreakpoint> bp = edb::v1::debugger_core->find_breakpoint(address)) {
							qDebug() << QString("Already a breakpoint at terminator 0x%1").arg(address, 0, 16);
							return edb::DEBUG_CONTINUE;
						}

						//Otherwise, attempt to set a breakpoint there and continue.
						if (std::shared_ptr<IBreakpoint> bp = edb::v1::debugger_core->add_breakpoint(address)) {
							own_breakpoints_.emplace_back(address,bp);
							qDebug() << QString("Setting breakpoint at terminator 0x%1").arg(address, 0, 16);
							bp->set_internal(true);
							bp->set_one_time(true); //If the 0xcc get's rm'd on next event, then
							                        //don't set it one time; we'll hande it manually
							ret_address_ = address;
							return edb::DEBUG_CONTINUE;
						}
						else {
							QMessageBox::critical(edb::v1::debugger_ui,
							                      tr("Error running until return"),
							                      tr("Failed to set breakpoint on a block terminator at address %1.").
							                                arg(address.toPointerString()));
							return pass_back_to_debugger();
						}
					}
				}
				else {
					//Invalid instruction or some other problem. Pass it back to the debugger.
					QMessageBox::critical(edb::v1::debugger_ui,
					                      tr("Error running until return"),
					                      tr("Failed to disassemble instruction at address %1.").
					                                arg(address.toPointerString()));
					return pass_back_to_debugger();
				}

				address += inst.byte_size();
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
	std::vector<std::pair<edb::address_t,std::weak_ptr<IBreakpoint>>> own_breakpoints_;
	edb::address_t      last_call_return_;
	edb::address_t      ret_address_;
};


}

//------------------------------------------------------------------------------
// Name: Debugger
// Desc:
//------------------------------------------------------------------------------
Debugger::Debugger(QWidget *parent) : QMainWindow(parent),
        tty_proc_(new QProcess(this)),
        arguments_dialog_(new DialogArguments),
        timer_(new QTimer(this)),
        recent_file_manager_(new RecentFileManager(this)),
        stack_view_info_(nullptr),
        comment_server_(std::make_shared<CommentServer>())
{
	setup_ui();

	// connect the timer to the debug event
	connect(timer_, &QTimer::timeout, this, &Debugger::next_debug_event);

	// create a context menu for the tab bar as well
	connect(ui.tabWidget, &TabWidget::customContextMenuRequested, this, &Debugger::tab_context_menu);

	// CPU Shortcuts
	gotoAddressAction_           = createAction(tr("&Goto Expression..."),                           QKeySequence(tr("Ctrl+G")),   &Debugger::goto_triggered);

	editCommentAction_           = createAction(tr("Add &Comment..."),                               QKeySequence(tr(";")),        &Debugger::mnuCPUEditComment);
	toggleBreakpointAction_      = createAction(tr("&Toggle Breakpoint"),                            QKeySequence(tr("F2")),       &Debugger::mnuCPUToggleBreakpoint);
	conditionalBreakpointAction_ = createAction(tr("Add &Conditional Breakpoint"),                   QKeySequence(tr("Shift+F2")), &Debugger::mnuCPUAddConditionalBreakpoint);
	runToThisLineAction_         = createAction(tr("R&un to this Line"),                             QKeySequence(tr("F4")),       &Debugger::mnuCPURunToThisLine);
	runToLinePassAction_         = createAction(tr("Run to this Line (Pass Signal To Application)"), QKeySequence(tr("Shift+F4")), &Debugger::mnuCPURunToThisLinePassSignal);
	editBytesAction_             = createAction(tr("Binary &Edit..."),                               QKeySequence(tr("Ctrl+E")),   &Debugger::mnuModifyBytes);
	fillWithZerosAction_         = createAction(tr("&Fill with 00's"),                               QKeySequence(),               &Debugger::mnuCPUFillZero);
	fillWithNOPsAction_          = createAction(tr("Fill with &NOPs"),                               QKeySequence(),               &Debugger::mnuCPUFillNop);
	setAddressLabelAction_       = createAction(tr("Set &Label..."),                                 QKeySequence(tr(":")),        &Debugger::mnuCPULabelAddress);
	followConstantInDumpAction_  = createAction(tr("Follow Constant In &Dump"),                      QKeySequence(),               &Debugger::mnuCPUFollowInDump);
	followConstantInStackAction_ = createAction(tr("Follow Constant In &Stack"),                     QKeySequence(),               &Debugger::mnuCPUFollowInStack);
	followAction_                = createAction(tr("&Follow"),                                       QKeySequence(tr("Return")),   &Debugger::mnuCPUFollow);

	// these get updated when we attach/run a new process, so it's OK to hard code them here
#if defined(EDB_X86_64)
	setRIPAction_                = createAction(tr("&Set %1 to this Instruction").arg("RIP"),        QKeySequence(tr("Ctrl+*")),   &Debugger::mnuCPUSetEIP);
	gotoRIPAction_               = createAction(tr("&Goto %1").arg("RIP"),                           QKeySequence(tr("*")),        &Debugger::mnuCPUJumpToEIP);
#elif defined(EDB_X86)
	setRIPAction_                = createAction(tr("&Set %1 to this Instruction").arg("EIP"),        QKeySequence(tr("Ctrl+*")),   &Debugger::mnuCPUSetEIP);
	gotoRIPAction_               = createAction(tr("&Goto %1").arg("EIP"),                           QKeySequence(tr("*")),        &Debugger::mnuCPUJumpToEIP);
#elif defined(EDB_ARM32) || defined(EDB_ARM64)
	setRIPAction_                = createAction(tr("&Set %1 to this Instruction").arg("PC"),         QKeySequence(tr("Ctrl+*")),   &Debugger::mnuCPUSetEIP);
	gotoRIPAction_               = createAction(tr("&Goto %1").arg("PC"),                            QKeySequence(tr("*")),        &Debugger::mnuCPUJumpToEIP);
#else
#error "This doesn't initialize actions and will lead to crash"
#endif

	// Data Dump Shorcuts
	dumpFollowInCPUAction_       = createAction(tr("Follow Address In &CPU"),                        QKeySequence(),               &Debugger::mnuDumpFollowInCPU);
	dumpFollowInDumpAction_      = createAction(tr("Follow Address In &Dump"),                       QKeySequence(),               &Debugger::mnuDumpFollowInDump);
	dumpFollowInStackAction_     = createAction(tr("Follow Address In &Stack"),                      QKeySequence(),               &Debugger::mnuDumpFollowInStack);
	dumpSaveToFileAction_        = createAction(tr("&Save To File"),                                 QKeySequence(),               &Debugger::mnuDumpSaveToFile);

	// Register View Shortcuts
	registerFollowInDumpAction_    = createAction(tr("&Follow In Dump"),           QKeySequence(), &Debugger::mnuRegisterFollowInDump);
	registerFollowInDumpTabAction_ = createAction(tr("&Follow In Dump (New Tab)"), QKeySequence(), &Debugger::mnuRegisterFollowInDumpNewTab);
	registerFollowInStackAction_   = createAction(tr("&Follow In Stack"),          QKeySequence(), &Debugger::mnuRegisterFollowInStack);

	// Stack View Shortcuts
	stackFollowInCPUAction_   = createAction(tr("Follow Address In &CPU"),   QKeySequence(), &Debugger::mnuStackFollowInCPU);
	stackFollowInDumpAction_  = createAction(tr("Follow Address In &Dump"),  QKeySequence(), &Debugger::mnuStackFollowInDump);
	stackFollowInStackAction_ = createAction(tr("Follow Address In &Stack"), QKeySequence(), &Debugger::mnuStackFollowInStack);

	// these get updated when we attach/run a new process, so it's OK to hard code them here
#if defined(EDB_X86_64)
	stackGotoRSPAction_ = createAction(tr("Goto %1").arg("RSP"),       QKeySequence(), &Debugger::mnuStackGotoESP);
	stackGotoRBPAction_ = createAction(tr("Goto %1").arg("RBP"),       QKeySequence(), &Debugger::mnuStackGotoEBP);
	stackPushAction_    = createAction(tr("&Push %1").arg("QWORD"),    QKeySequence(), &Debugger::mnuStackPush);
	stackPopAction_     = createAction(tr("P&op %1").arg("QWORD"),     QKeySequence(), &Debugger::mnuStackPop);
#elif defined(EDB_X86)
	stackGotoRSPAction_ = createAction(tr("Goto %1").arg("ESP"),       QKeySequence(), &Debugger::mnuStackGotoESP);
	stackGotoRBPAction_ = createAction(tr("Goto %1").arg("EBP"),       QKeySequence(), &Debugger::mnuStackGotoEBP);
	stackPushAction_    = createAction(tr("&Push %1").arg("DWORD"),    QKeySequence(), &Debugger::mnuStackPush);
	stackPopAction_     = createAction(tr("P&op %1").arg("DWORD"),     QKeySequence(), &Debugger::mnuStackPop);
#elif defined(EDB_ARM32)
	stackGotoRSPAction_ = createAction(tr("Goto %1").arg("SP"),       QKeySequence(), &Debugger::mnuStackGotoESP);
	stackGotoRBPAction_ = createAction(tr("Goto %1").arg("FP"),       QKeySequence(), &Debugger::mnuStackGotoEBP);
    stackGotoRBPAction_->setDisabled(true); // FIXME(ARM): this just stubs it out since it likely won't really work
	stackPushAction_    = createAction(tr("&Push %1").arg("DWORD"),    QKeySequence(), &Debugger::mnuStackPush);
    stackPushAction_->setDisabled(true); // FIXME(ARM): this just stubs it out since it likely won't really work
	stackPopAction_     = createAction(tr("P&op %1").arg("DWORD"),     QKeySequence(), &Debugger::mnuStackPop);
    stackPopAction_->setDisabled(true); // FIXME(ARM): this just stubs it out since it likely won't really work
#elif defined(EDB_ARM64)
	stackGotoRSPAction_ = createAction(tr("Goto %1").arg("SP"),       QKeySequence(), &Debugger::mnuStackGotoESP);
    stackGotoRSPAction_->setDisabled(true); // FIXME(ARM): this just stubs it out since it likely won't really work
	stackGotoRBPAction_ = createAction(tr("Goto %1").arg("FP"),       QKeySequence(), &Debugger::mnuStackGotoEBP);
    stackGotoRBPAction_->setDisabled(true); // FIXME(ARM): this just stubs it out since it likely won't really work
	stackPushAction_    = createAction(tr("&Push %1").arg("QWORD"),    QKeySequence(), &Debugger::mnuStackPush);
    stackPushAction_->setDisabled(true); // FIXME(ARM): this just stubs it out since it likely won't really work
	stackPopAction_     = createAction(tr("P&op %1").arg("QWORD"),     QKeySequence(), &Debugger::mnuStackPop);
    stackPopAction_->setDisabled(true); // FIXME(ARM): this just stubs it out since it likely won't really work
#else
#error "This doesn't initialize actions and will lead to crash"
#endif


	// set these to have no meaningful "data" (yet)
	followConstantInDumpAction_->setData(qlonglong(0));
	followConstantInStackAction_->setData(qlonglong(0));

	setAcceptDrops(true);

	// setup the list model for instruction details list
	list_model_ = new QStringListModel(this);
	ui.listView->setModel(list_model_);

	// setup the recent file manager
	ui.action_Recent_Files->setMenu(recent_file_manager_->create_menu());
	connect(recent_file_manager_, &RecentFileManager::file_selected, this, &Debugger::open_file);

	// make us the default event handler
	edb::v1::add_debug_event_handler(this);

	// enable the arch processor
#if 0
	ui.registerList->setModel(&edb::v1::arch_processor().get_register_view_model());
	edb::v1::arch_processor().setup_register_view();
#endif

	// default the working directory to ours
	working_directory_ = QDir().absolutePath();

	// let the plugins setup their menus
	finish_plugin_setup();

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

	for(QObject *plugin: edb::v1::plugin_list()) {
		if(auto p = qobject_cast<IPlugin *>(plugin)) {
			p->fini();
		}
	}

	// kill our xterm and wait for it to die
	tty_proc_->kill();
	tty_proc_->waitForFinished(3000);
	edb::v1::remove_debug_event_handler(this);
}

QAction *Debugger::createAction(const QString &text, const QKeySequence &keySequence, void(Debugger::*slotPtr)()) {
	auto action = new QAction(text, this);
	action->setShortcut(keySequence);
	addAction(action);
	connect(action, &QAction::triggered, this, slotPtr);
	return action;
}

//------------------------------------------------------------------------------
// Name: update_menu_state
// Desc:
//------------------------------------------------------------------------------
void Debugger::update_menu_state(GUI_STATE state) {

	static const QString Paused     = tr("paused");
	static const QString Running    = tr("running");
	static const QString Terminated = tr("terminated");

	switch(state) {
	case PAUSED:
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
		add_tab_->setEnabled(true);
		status_->setText(Paused);
		status_->repaint();
		break;
	case RUNNING:
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
		add_tab_->setEnabled(true);
		status_->setText(Running);
		status_->repaint();
		break;
	case TERMINATED:
		ui.actionRun_Until_Return->setEnabled(false);
		ui.action_Restart->setEnabled(recent_file_manager_->entry_count()>0);
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
		add_tab_->setEnabled(false);
		status_->setText(Terminated);
		status_->repaint();
		break;
	}

	gui_state_ = state;
}

//------------------------------------------------------------------------------
// Name: create_tty
// Desc: creates a TTY object for our command line I/O
//------------------------------------------------------------------------------
QString Debugger::create_tty() {

	QString result_tty = tty_file_;

#if defined(Q_OS_LINUX) || defined(Q_OS_OPENBSD) || defined(Q_OS_FREEBSD)
	// we attempt to reuse an open output window
	if(edb::v1::config().tty_enabled && tty_proc_->state() != QProcess::Running) {
		const QString command = edb::v1::config().tty_command;

		if(!command.isEmpty()) {

			// ok, creating a new one...
			// first try to get a 'unique' filename, i would love to use a system
			// temp file API... but there doesn't seem to be one which will create
			// a pipe...only ordinary files!
			const QString temp_pipe = QString("%1/edb_temp_file_%2_%3").arg(QDir::tempPath()).arg(qrand()).arg(getpid());

			// make sure it isn't already there, and then make the pipe
			::unlink(qPrintable(temp_pipe));
			::mkfifo(qPrintable(temp_pipe), S_IRUSR | S_IWUSR);

			// this is a basic shell script which will output the tty to a file (the pipe),
			// ignore kill sigs, close all standard IO, and then just hang
			const QString shell_script = QString(
				"tty > %1;"
				"trap \"\" INT QUIT TSTP;"
				"exec<&-; exec>&-;"
				"while :; do sleep 3600; done"
				).arg(temp_pipe);

			// parse up the command from the options, white space delimited
			QStringList proc_args = edb::v1::parse_command_line(command);
			const QString tty_command = proc_args.takeFirst().trimmed();

			// start constructing the arguments for the term
			const QFileInfo command_info(tty_command);

			if(command_info.fileName() == "gnome-terminal") {
				proc_args << "--hide-menubar" << "--title" << tr("edb output");
			} else if(command_info.fileName() == "konsole") {
				proc_args << "--hide-menubar" << "--title" << tr("edb output") << "--nofork" << "-hold";
			} else {
				proc_args << "-title" << tr("edb output") << "-hold";
			}

			proc_args << "-e" << "sh" << "-c" << QString("%1").arg(shell_script);

			qDebug() << "Running Terminal: " << tty_command;
			qDebug() << "Terminal Args: " << proc_args;

			// make the tty process object and connect it's death signal to our cleanup
			connect(tty_proc_, SIGNAL(finished(int, QProcess::ExitStatus)), SLOT(tty_proc_finished(int, QProcess::ExitStatus)));

			tty_proc_->start(tty_command, proc_args);

			if(tty_proc_->waitForStarted(3000)) {


				// try to read from the pipe, but with a 2 second timeout
				int fd = open(qPrintable(temp_pipe), O_RDWR);
				if(fd != -1) {
					fd_set set;
					FD_ZERO(&set);    // clear the set
					FD_SET(fd, &set); // add our file descriptor to the set

					struct timeval timeout;
					timeout.tv_sec  = 2;
					timeout.tv_usec = 0;

					char buf[256] = {};
					const int rv = select(fd + 1, &set, nullptr, nullptr, &timeout);
					switch(rv) {
					case -1:
						qDebug() << "An error occured while attempting to get the TTY of the terminal sub-process";
						break;
					case 0:
						qDebug() << "A Timeout occured while attempting to get the TTY of the terminal sub-process";
						break;
					default:
						if(read(fd, buf, sizeof(buf)) != -1) {
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
void Debugger::tty_proc_finished(int exit_code, QProcess::ExitStatus exit_status) {
	Q_UNUSED(exit_code)
	Q_UNUSED(exit_status)

	tty_file_.clear();
}

//------------------------------------------------------------------------------
// Name: current_tab
// Desc:
//------------------------------------------------------------------------------
int Debugger::current_tab() const {
	return ui.tabWidget->currentIndex();
}

//------------------------------------------------------------------------------
// Name: current_data_view_info
// Desc:
//------------------------------------------------------------------------------
std::shared_ptr<DataViewInfo> Debugger::current_data_view_info() const {
	return data_regions_[current_tab()];
}

//------------------------------------------------------------------------------
// Name: set_debugger_caption
// Desc: sets the caption part to also show the application name and pid
//------------------------------------------------------------------------------
void Debugger::set_debugger_caption(const QString &appname) {
	setWindowTitle(tr("edb - %1 [%2]").arg(appname).arg(edb::v1::debugger_core->process()->pid()));
}

//------------------------------------------------------------------------------
// Name: delete_data_tab
// Desc:
//------------------------------------------------------------------------------
void Debugger::delete_data_tab() {
	const int current = current_tab();

	// get a pointer to the info we need (before removing it from the list!)
	// this seems redundant to current_data_view_info(), but we need the
	// index too, so may as well waste one line to avoid duplicate work
	std::shared_ptr<DataViewInfo> info = data_regions_[current];

	// remove it from the list
	data_regions_.remove(current);

	// remove the tab associated with it
	ui.tabWidget->removeTab(current);
}

//------------------------------------------------------------------------------
// Name: create_data_tab
// Desc:
//------------------------------------------------------------------------------
void Debugger::create_data_tab() {
	const int current = current_tab();

	// duplicate the current region
	auto new_data_view = std::make_shared<DataViewInfo>((current != -1) ? data_regions_[current]->region : nullptr);

	auto hexview = std::make_shared<QHexView>();

	new_data_view->view = hexview;

	// setup the context menu
	hexview->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(hexview.get(), &QHexView::customContextMenuRequested, this, &Debugger::mnuDumpContextMenu);

	// show the initial data for this new view
	if(new_data_view->region) {
		hexview->setAddressOffset(new_data_view->region->start());
	} else {
		hexview->setAddressOffset(0);
	}

    // NOTE(eteran): for issue #522, allow comments in data view when single word width
	hexview->setCommentServer(comment_server_.get());

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
	if(edb::v1::debuggeeIs64Bit()) {
		hexview->setAddressSize(QHexView::Address64);
	} else {
		hexview->setAddressSize(QHexView::Address32);
	}

	// set the default font
	QFont dump_font;
	dump_font.fromString(config.data_font);
	hexview->setFont(dump_font);

	data_regions_.push_back(new_data_view);

	// create the tab!
	if(new_data_view->region) {
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
void Debugger::finish_plugin_setup() {


	// call the init function for each plugin, this is done after
	// ALL plugins are loaded in case there are inter-plugin dependencies
	for(QObject *plugin: edb::v1::plugin_list()) {
		if(auto p = qobject_cast<IPlugin *>(plugin)) {
			p->init();
		}
	}

	// setup the menu for all plugins that which to do so
	QPointer<DialogOptions> options = qobject_cast<DialogOptions *>(edb::v1::dialog_options());
	for(QObject *plugin: edb::v1::plugin_list()) {
		if(auto p = qobject_cast<IPlugin *>(plugin)) {
			if(QMenu *const menu = p->menu(this)) {
				ui.menu_Plugins->addMenu(menu);
			}

			if(QWidget *const options_page = p->options_page()) {
				if(options) {
					options->addOptionsPage(options_page);
				}
			}

			// setup the shortcuts for these actions
			const QList<QAction *> register_actions = p->register_context_menu();
			const QList<QAction *> cpu_actions      = p->cpu_context_menu();
			const QList<QAction *> stack_actions    = p->stack_context_menu();
			const QList<QAction *> data_actions     = p->data_context_menu();
			const QList<QAction *> actions = register_actions + cpu_actions + stack_actions + data_actions;

			for(QAction *action : actions) {
				QKeySequence shortcut = action->shortcut();
				if(!shortcut.isEmpty()) {
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
Result<edb::address_t, QString> Debugger::get_goto_expression() {

	boost::optional<edb::address_t> address = edb::v2::get_expression_from_user(tr("Goto Expression"), tr("Expression:"));
	if(address) {
		return *address;
	} else {
		return make_unexpected(tr("No Address"));
	}
}

//------------------------------------------------------------------------------
// Name: get_follow_register
// Desc:
//------------------------------------------------------------------------------
Result<edb::reg_t, QString> Debugger::get_follow_register() const {

	const Register reg = active_register();
	if(!reg || reg.bitSize() > 8 * sizeof(edb::address_t)) {
		return make_unexpected(tr("No Value"));
	}

	return reg.valueAsAddress();
}


//------------------------------------------------------------------------------
// Name: goto_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::goto_triggered() {
	QWidget *const widget = QApplication::focusWidget();
	if(auto hexview = qobject_cast<QHexView*>(widget)) {
		if(hexview == stack_view_.get()) {
			mnuStackGotoAddress();
		} else {
			mnuDumpGotoAddress();
		}
	} else if(qobject_cast<QDisassemblyView*>(widget)) {
		mnuCPUJumpToAddress();
	}
}

//------------------------------------------------------------------------------
// Name: setup_ui
// Desc: creates the UI
//------------------------------------------------------------------------------
void Debugger::setup_ui() {
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
	ui.menu_View->addAction(ui.dataDock     ->toggleViewAction());
	ui.menu_View->addAction(ui.stackDock    ->toggleViewAction());
	ui.menu_View->addAction(ui.toolBar      ->toggleViewAction());

	ui.action_Restart->setEnabled(recent_file_manager_->entry_count()>0);

	// make sure our widgets use custom context menus
	ui.cpuView     ->setContextMenuPolicy(Qt::CustomContextMenu);

	// set the listbox to about 4 lines
	const QFontMetrics &fm = ui.listView->fontMetrics();
	ui.listView->setFixedHeight(fm.height() * 4);

	setup_stack_view();
	setup_tab_buttons();

	// remove the one in the designer and put in our built one.
	// it's a bit ugly to do it this way, but the designer wont let me
	// make a tabless entry..and the ones i create look slightly diff
	// (probably lack of layout manager in mine...
	ui.tabWidget->clear();
	mnuDumpCreateTab();

	// apply any fonts we may have stored
	apply_default_fonts();

	// apply the default setting for showing address separators
	apply_default_show_separator();
}

//------------------------------------------------------------------------------
// Name: setup_stack_view
// Desc:
//------------------------------------------------------------------------------
void Debugger::setup_stack_view() {

	stack_view_ = std::make_shared<QHexView>();

	stack_view_->setUserConfigRowWidth(false);
	stack_view_->setUserConfigWordWidth(false);

	ui.stackDock->setWidget(stack_view_.get());

	// setup the context menu
	stack_view_->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(stack_view_.get(), &QHexView::customContextMenuRequested, this, &Debugger::mnuStackContextMenu);

	// we placed a view in the designer, so just set it here
	// this may get transitioned to heap allocated, we'll see
	stack_view_info_.view = stack_view_;

	// setup the comment server for the stack viewer
	stack_view_->setCommentServer(comment_server_.get());
}

//------------------------------------------------------------------------------
// Name: closeEvent
// Desc: triggered on main window close, saves window state
//------------------------------------------------------------------------------
void Debugger::closeEvent(QCloseEvent *event) {

	// make sure sessions still get recorded even if they just close us
	const QString filename = session_filename();
	if(!filename.isEmpty()) {
		SessionManager::instance().save_session(filename);
	}

	if(IDebugger *core = edb::v1::debugger_core) {
		core->end_debug_session();
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
	settings.setValue("window.stack.show_address.enabled", stack_view_->showAddress());
	settings.setValue("window.stack.show_hex.enabled", stack_view_->showHexDump());
	settings.setValue("window.stack.show_ascii.enabled", stack_view_->showAsciiDump());
	settings.setValue("window.stack.show_comments.enabled", stack_view_->showComments());

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

	if(width != -1) {
		resize(width, size().height());
	}

	if(height != -1) {
		resize(size().width(), height);
	}

	const Configuration &config = edb::v1::config();
	switch(config.startup_window_location) {
	case Configuration::SystemDefault:
		break;
	case Configuration::Centered:
		{
			QDesktopWidget desktop;
			QRect sg = desktop.screenGeometry();
			int x = (sg.width() - this->width()) / 2;
			int y = (sg.height() - this->height()) / 2;
			move(x, y);
		}
		break;
	case Configuration::Restore:
		if(x != -1 && y != -1) {
			move(x, y);
		}
		break;
	}


	stack_view_->setShowAddress(settings.value("window.stack.show_address.enabled", true).toBool());
	stack_view_->setShowHexDump(settings.value("window.stack.show_hex.enabled", true).toBool());
	stack_view_->setShowAsciiDump(settings.value("window.stack.show_ascii.enabled", true).toBool());
	stack_view_->setShowComments(settings.value("window.stack.show_comments.enabled", true).toBool());

	int row_width  = 1;
	int word_width = edb::v1::pointer_size();

	stack_view_->setRowWidth(row_width);
	stack_view_->setWordWidth(word_width);


	QByteArray disassemblyState = settings.value("window.disassembly.state").toByteArray();
	ui.cpuView->restoreState(disassemblyState);

	settings.endGroup();
	restoreState(state);
}

//------------------------------------------------------------------------------
// Name: dragEnterEvent
// Desc: triggered when dragging data onto the main window
//------------------------------------------------------------------------------
void Debugger::dragEnterEvent(QDragEnterEvent* event) {
	const QMimeData* mimeData = event->mimeData();

	// check for our needed mime type (file)
	// make sure it's only one file
	if(mimeData->hasUrls() && mimeData->urls().size() == 1) {
		// extract the local path of the file
		QList<QUrl> urls = mimeData->urls();
		QUrl url = urls[0].toLocalFile();
		if(!url.isEmpty()) {
			event->accept();
		}
	}
}

//------------------------------------------------------------------------------
// Name: dropEvent
// Desc: triggered when data was dropped onto the main window
//------------------------------------------------------------------------------
void Debugger::dropEvent(QDropEvent* event) {
	const QMimeData* mimeData = event->mimeData();

	if(mimeData->hasUrls() && mimeData->urls().size() == 1) {
		QList<QUrl> urls = mimeData->urls();
		const QString s = urls[0].toLocalFile();
		if(!s.isEmpty()) {
			Q_ASSERT(edb::v1::debugger_core);

			common_open(s, QList<QByteArray>());
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
void Debugger::apply_default_fonts() {

	QFont font;
	const Configuration &config = edb::v1::config();

	// set some default fonts
	if(font.fromString(config.stack_font)) {
		stack_view_->setFont(font);
	}

	if(font.fromString(config.registers_font)) {
#if 0
		ui.registerList->setFont(font);
#endif
	}

	if(font.fromString(config.disassembly_font)) {
		ui.cpuView->setFont(font);
	}

	if(font.fromString(config.data_font)) {
		Q_FOREACH(const std::shared_ptr<DataViewInfo> &data_view, data_regions_) {
			data_view->view->setFont(font);
		}
	}
}

//------------------------------------------------------------------------------
// Name: setup_tab_buttons
// Desc: creates the add/remove tab buttons in the data view
//------------------------------------------------------------------------------
void Debugger::setup_tab_buttons() {
	// add the corner widgets to the data view
	add_tab_ = new QToolButton(ui.tabWidget);
	del_tab_ = new QToolButton(ui.tabWidget);

	add_tab_->setToolButtonStyle(Qt::ToolButtonIconOnly);
	del_tab_->setToolButtonStyle(Qt::ToolButtonIconOnly);
	add_tab_->setIcon(QIcon(":/debugger/images/edb16-addtab.png"));
	del_tab_->setIcon(QIcon(":/debugger/images/edb16-deltab.png"));
	add_tab_->setAutoRaise(true);
	del_tab_->setAutoRaise(true);
	add_tab_->setEnabled(false);
	del_tab_->setEnabled(false);

	ui.tabWidget->setCornerWidget(add_tab_, Qt::TopLeftCorner);
	ui.tabWidget->setCornerWidget(del_tab_, Qt::TopRightCorner);

	connect(add_tab_, &QToolButton::clicked, this, &Debugger::mnuDumpCreateTab);
	connect(del_tab_, &QToolButton::clicked, this, &Debugger::mnuDumpDeleteTab);
}

//------------------------------------------------------------------------------
// Name: active_register
// Desc:
//------------------------------------------------------------------------------
Register Debugger::active_register() const {
	const auto& model = edb::v1::arch_processor().get_register_view_model();
	const auto index = model.activeIndex();
	if(!index.data(RegisterViewModelBase::Model::IsNormalRegisterRole).toBool()) {
		return {};
	}

	const auto regName = index.sibling(index.row(), RegisterViewModelBase::Model::NAME_COLUMN).data().toString();
	if(regName.isEmpty()) {
		return {};
	}

	if(IDebugger *core = edb::v1::debugger_core) {
		if(IProcess *process = core->process()) {
			State state;
			process->current_thread()->get_state(&state);
			return state[regName];
		}
	}
	return {};
}

//------------------------------------------------------------------------------
// Name: on_registerList_customContextMenuRequested
// Desc: context menu handler for register view
//------------------------------------------------------------------------------
QList<QAction*> Debugger::getCurrentRegisterContextMenuItems() const {
	QList<QAction*> allActions;
	const auto reg=active_register();
	if(reg.type() & (Register::TYPE_GPR | Register::TYPE_IP)) {

		QList<QAction*> actions;
		actions << registerFollowInDumpAction_;
		actions << registerFollowInDumpTabAction_;
		actions << registerFollowInStackAction_;

		allActions.append(actions);
	}
	allActions.append(get_plugin_context_menu_items(&IPlugin::register_context_menu));
	return allActions;
}

// Flag-toggling functions.  Not sure if this is the best solution, but it works.

//------------------------------------------------------------------------------
// Name: toggle_flag
// Desc: toggles flag register at bit position pos
// Param: pos The position of the flag bit to toggle
//------------------------------------------------------------------------------
void Debugger::toggle_flag(int pos)
{
	// TODO Maybe this should just return w/o action if no process is loaded.

	// Get the state and get the flag register
	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(std::shared_ptr<IThread> thread = process->current_thread()) {
			State state;
			thread->get_state(&state);
			edb::reg_t flags = state.flags();

			// Toggle the flag
			flags ^= (1 << pos);
			state.set_flags(flags);
			thread->set_state(state);

			update_gui();
			refresh_gui();
		}
	}
}

void Debugger::toggle_flag_carry()     { toggle_flag(0); }
void Debugger::toggle_flag_parity()    { toggle_flag(2); }
void Debugger::toggle_flag_auxiliary() { toggle_flag(4); }
void Debugger::toggle_flag_zero()      { toggle_flag(6); }
void Debugger::toggle_flag_sign()      { toggle_flag(7); }
void Debugger::toggle_flag_direction() { toggle_flag(10); }
void Debugger::toggle_flag_overflow()  { toggle_flag(11); }

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
void Debugger::apply_default_show_separator() {
	const bool show = edb::v1::config().show_address_separator;

	ui.cpuView->setShowAddressSeparator(show);
	stack_view_->setShowAddressSeparator(show);
	Q_FOREACH(const std::shared_ptr<DataViewInfo> &data_view, data_regions_) {
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
	apply_default_fonts();

	// apply changes to the GUI options
	apply_default_show_separator();

	// show changes
	refresh_gui();
}

//----------------------------------------------------------------------
// Name: step_over
//----------------------------------------------------------------------
template <class F1, class F2>
void Debugger::step_over(F1 run_func, F2 step_func) {

	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(std::shared_ptr<IThread> thread = process->current_thread()) {
			State state;
			thread->get_state(&state);

			const edb::address_t ip = state.instruction_pointer();
			quint8 buffer[edb::Instruction::MAX_SIZE];
			if(const int sz = edb::v1::get_instruction_bytes(ip, buffer)) {
				edb::Instruction inst(buffer, buffer + sz, 0);
				if(inst && edb::v1::arch_processor().can_step_over(inst)) {

					// add a temporary breakpoint at the instruction just
					// after the call
					if(std::shared_ptr<IBreakpoint> bp = edb::v1::debugger_core->add_breakpoint(ip + inst.byte_size())) {
						bp->set_internal(true);
						bp->set_one_time(true);
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
void Debugger::follow_memory(edb::address_t address, F follow_func) {
	if(!follow_func(address)) {
		QMessageBox::critical(this,
			tr("No Memory Found"),
			tr("There appears to be no memory at that location (<strong>%1</strong>)").arg(edb::v1::format_pointer(address)));
	}
}

//------------------------------------------------------------------------------
// Name: follow_register_in_dump
// Desc:
//------------------------------------------------------------------------------
void Debugger::follow_register_in_dump(bool tabbed) {

	if(const Result<edb::address_t, QString> address = get_follow_register()) {
		if(!edb::v1::dump_data(*address, tabbed)) {
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
	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(std::shared_ptr<IThread> thread = process->current_thread()) {
			State state;
			thread->get_state(&state);
			follow_memory(state.stack_pointer(), [](edb::address_t address) {
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
	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(std::shared_ptr<IThread> thread = process->current_thread()) {
			State state;
			thread->get_state(&state);
			follow_memory(state.frame_pointer(), [](edb::address_t address) {
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
	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(std::shared_ptr<IThread> thread = process->current_thread()) {
			State state;
			thread->get_state(&state);
			follow_memory(state.instruction_pointer(), [](edb::address_t address) {
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

	if(const Result<edb::address_t, QString> address = get_goto_expression()) {
		follow_memory(*address, [](edb::address_t address) {
			return edb::v1::jump_to_address(address);
		});
	}
}

//------------------------------------------------------------------------------
// Name: mnuDumpGotoAddress
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuDumpGotoAddress() {
	if(const Result<edb::address_t, QString> address = get_goto_expression()) {
		follow_memory(*address, [](edb::address_t address) {
			return edb::v1::dump_data(address);
		});
	}
}

//------------------------------------------------------------------------------
// Name: mnuStackGotoAddress
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuStackGotoAddress() {
	if(const Result<edb::address_t, QString> address = get_goto_expression()) {
		follow_memory(*address, [](edb::address_t address) {
			return edb::v1::dump_stack(address);
		});
	}
}

//------------------------------------------------------------------------------
// Name: mnuRegisterFollowInStack
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuRegisterFollowInStack() {

	if(const Result<edb::address_t, QString> address = get_follow_register()) {
		follow_memory(*address, [](edb::address_t address) {
			return edb::v1::dump_stack(address);
		});
	}
}


//------------------------------------------------------------------------------
// Name: get_follow_address
// Desc:
//------------------------------------------------------------------------------
template <class T>
Result<edb::address_t, QString> Debugger::get_follow_address(const T &hexview) {

	Q_ASSERT(hexview);

	const size_t pointer_size = edb::v1::pointer_size();

	if(hexview->hasSelectedText()) {
		const QByteArray data = hexview->selectedBytes();

		if(data.size() == static_cast<int>(pointer_size)) {
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
// Name: follow_in_stack
// Desc:
//------------------------------------------------------------------------------
template <class T>
void Debugger::follow_in_stack(const T &hexview) {

	if(const Result<edb::address_t, QString> address = get_follow_address(hexview)) {
		follow_memory(*address, [](edb::address_t address) {
			return edb::v1::dump_stack(address);
		});
	}
}

//------------------------------------------------------------------------------
// Name: follow_in_dump
// Desc:
//------------------------------------------------------------------------------
template <class T>
void Debugger::follow_in_dump(const T &hexview) {

	if(const Result<edb::address_t, QString> address = get_follow_address(hexview)) {
		follow_memory(*address, [](edb::address_t address) {
			return edb::v1::dump_data(address);
		});
	}
}

//------------------------------------------------------------------------------
// Name: follow_in_cpu
// Desc:
//------------------------------------------------------------------------------
template <class T>
void Debugger::follow_in_cpu(const T &hexview) {

	if(const Result<edb::address_t, QString> address = get_follow_address(hexview)) {
		follow_memory(*address, [](edb::address_t address) {
			return edb::v1::jump_to_address(address);
		});
	}
}

//------------------------------------------------------------------------------
// Name: mnuDumpFollowInCPU
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuDumpFollowInCPU() {
	follow_in_cpu(qobject_cast<QHexView *>(ui.tabWidget->currentWidget()));
}

//------------------------------------------------------------------------------
// Name: mnuDumpFollowInDump
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuDumpFollowInDump() {
	follow_in_dump(qobject_cast<QHexView *>(ui.tabWidget->currentWidget()));
}

//------------------------------------------------------------------------------
// Name: mnuDumpFollowInStack
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuDumpFollowInStack() {
	follow_in_stack(qobject_cast<QHexView *>(ui.tabWidget->currentWidget()));
}

//------------------------------------------------------------------------------
// Name: mnuStackFollowInDump
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuStackFollowInDump() {
	follow_in_dump(stack_view_);
}

//------------------------------------------------------------------------------
// Name: mnuStackFollowInCPU
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuStackFollowInCPU() {
	follow_in_cpu(stack_view_);
}

//------------------------------------------------------------------------------
// Name: mnuStackFollowInStack
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuStackFollowInStack() {
	follow_in_stack(stack_view_);
}

//------------------------------------------------------------------------------
// Name: on_actionApplication_Arguments_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_actionApplication_Arguments_triggered() {
	arguments_dialog_->exec();
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

	if(!new_dir.isEmpty()) {
		working_directory_ = new_dir;
	}
}

//------------------------------------------------------------------------------
// Name: mnuStackPush
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuStackPush() {
	Register value(edb::v1::debuggeeIs32Bit()?
					   make_Register("",edb::value32(0),Register::TYPE_GPR):
					   make_Register("",edb::value64(0),Register::TYPE_GPR));

	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(std::shared_ptr<IThread> thread = process->current_thread()) {
			State state;
			thread->get_state(&state);

			// ask for a replacement
			if(edb::v1::get_value_from_user(value, tr("Enter value to push"))) {

				// if they said ok, do the push, just like the hardware would do
				edb::v1::push_value(&state, value.valueAsInteger());

				// update the state
				thread->set_state(state);
				update_gui();
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: mnuStackPop
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuStackPop() {
	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(std::shared_ptr<IThread> thread = process->current_thread()) {
			State state;
			thread->get_state(&state);
			edb::v1::pop_value(&state);
			thread->set_state(state);
			update_gui();
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_cpuView_customContextMenuRequested
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_cpuView_customContextMenuRequested(const QPoint &pos) {
	QMenu menu;

	auto displayMenu = new QMenu(tr("Display"));
	displayMenu->addAction(QIcon::fromTheme(QString::fromUtf8("view-restore")), tr("Restore Column Defaults"), ui.cpuView, SLOT(resetColumns()));

	auto editMenu = new QMenu(tr("&Edit"));
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


	if(IProcess *process = edb::v1::debugger_core->process()) {

		Q_UNUSED(process)

		quint8 buffer[edb::Instruction::MAX_SIZE + 1];
		if(edb::v1::get_instruction_bytes(address, buffer, &size)) {
			edb::Instruction inst(buffer, buffer + size, address);
			if(inst) {


				if(is_call(inst) || is_jump(inst)) {
					if(is_immediate(inst[0])) {
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
					for(std::size_t i = 0; i < inst.operand_count(); ++i) {
						if(is_immediate(inst[i])) {
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

	add_plugin_context_menu(&menu, &IPlugin::cpu_context_menu);

	menu.exec(ui.cpuView->viewport()->mapToGlobal(pos));
}

//------------------------------------------------------------------------------
// Name: mnuCPUFollow
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuCPUFollow() {

	const edb::address_t address = ui.cpuView->selectedAddress();
	int size                     = ui.cpuView->selectedSize();
	quint8 buffer[edb::Instruction::MAX_SIZE + 1];
	if(!edb::v1::get_instruction_bytes(address, buffer, &size))
		return;

	const edb::Instruction inst(buffer, buffer + size, address);
	if(!is_call(inst) && !is_jump(inst))
		return;
	if(!is_immediate(inst[0]))
		return;

	const edb::address_t addressToFollow=util::to_unsigned(inst[0]->imm);
	if(auto action = qobject_cast<QAction *>(sender())) {
		Q_UNUSED(action)
		follow_memory(addressToFollow, edb::v1::jump_to_address);
	}
}

//------------------------------------------------------------------------------
// Name: mnuCPUFollowInDump
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuCPUFollowInDump() {
	if(auto action = qobject_cast<QAction *>(sender())) {
		const edb::address_t address = action->data().toULongLong();
		follow_memory(address, [](edb::address_t address) {
			return edb::v1::dump_data(address);
		});
	}
}

//------------------------------------------------------------------------------
// Name: mnuCPUFollowInStack
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuCPUFollowInStack() {
	if(auto action = qobject_cast<QAction *>(sender())) {
		const edb::address_t address = action->data().toULongLong();
		follow_memory(address, [](edb::address_t address) {
			return edb::v1::dump_stack(address);
		});
	}
}

//------------------------------------------------------------------------------
// Name: mnuStackToggleLock
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuStackToggleLock(bool locked) {
	stack_view_locked_ = locked;
}

//------------------------------------------------------------------------------
// Name: mnuStackContextMenu
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuStackContextMenu(const QPoint &pos) {

	QMenu *const menu = stack_view_->createStandardContextMenu();

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
    action->setChecked(stack_view_locked_);
	menu->addAction(action);
	connect(action, &QAction::toggled, this, &Debugger::mnuStackToggleLock);

	add_plugin_context_menu(menu, &IPlugin::stack_context_menu);

	menu->exec(stack_view_->mapToGlobal(pos));
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

	add_plugin_context_menu(menu, &IPlugin::data_context_menu);

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
		last_open_directory_);

	if(!filename.isEmpty()) {
		QFile file(filename);
		file.open(QIODevice::WriteOnly);
		if(file.isOpen()) {
			file.write(s->allBytes());
		}
	}
}

//------------------------------------------------------------------------------
// Name: cpu_fill
// Desc:
//------------------------------------------------------------------------------
void Debugger::cpu_fill(quint8 byte) {
	const edb::address_t address = ui.cpuView->selectedAddress();
	const unsigned int size      = ui.cpuView->selectedSize();

	if(size != 0) {
		if(IProcess *process = edb::v1::debugger_core->process()) {
			if(edb::v1::overwrite_check(address, size)) {
				QByteArray bytes(size, byte);

				process->write_bytes(address, bytes.data(), size);

				// do a refresh, not full update
				refresh_gui();
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
				ui.cpuView->get_comment(address),
				&got_text);

	//If we got a comment, add it.
	if (got_text && !comment.isEmpty()) {
		ui.cpuView->add_comment(address, comment);
	}
	else if (got_text && comment.isEmpty()) {
		//If the user backspaced the comment, remove the comment since
		//there's no need for a null string to take space in the hash.
		ui.cpuView->remove_comment(address);
	}
	else {
		//The only other real case is that we didn't got_text.  No change.
		return;
	}

	refresh_gui();
}

//------------------------------------------------------------------------------
// Name: mnuCPURemoveComment
// Desc: Removes a comment at the selected address.
//------------------------------------------------------------------------------
void Debugger::mnuCPURemoveComment() {
	const edb::address_t address = ui.cpuView->selectedAddress();
	ui.cpuView->remove_comment(address);
	refresh_gui();
}

//------------------------------------------------------------------------------
// Name: run_to_this_line
// Desc:
//------------------------------------------------------------------------------
void Debugger::run_to_this_line(EXCEPTION_RESUME pass_signal) {
	const edb::address_t address = ui.cpuView->selectedAddress();
	std::shared_ptr<IBreakpoint> bp = edb::v1::find_breakpoint(address);
	if(!bp) {
		bp = edb::v1::create_breakpoint(address);
		if(!bp) return;
		bp->set_one_time(true);
		bp->set_internal(true);
		bp->tag = run_to_cursor_tag;
	}

	resume_execution(pass_signal, MODE_RUN, ResumeFlag::None);
}

//------------------------------------------------------------------------------
// Name: mnuCPURunToThisLinePassSignal
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuCPURunToThisLinePassSignal() {
	run_to_this_line(PASS_EXCEPTION);
}

//------------------------------------------------------------------------------
// Name: mnuCPURunToThisLine
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuCPURunToThisLine() {
	run_to_this_line(IGNORE_EXCEPTION);
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
	const QString condition = QInputDialog::getText(
	                              this,
	                              tr("Set Breakpoint Condition"),
	                              tr("Expression:"),
	                              QLineEdit::Normal,
	                              QString(),
	                              &ok);
	if(ok) {
		if(std::shared_ptr<IBreakpoint> bp = edb::v1::create_breakpoint(address)) {
			if(!condition.isEmpty()) {
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
	cpu_fill(0x00);
}

//------------------------------------------------------------------------------
// Name: mnuCPUFillNop
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuCPUFillNop() {
	if(IDebugger *core = edb::v1::debugger_core) {
		cpu_fill(core->nopFillByte());
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
		edb::v1::symbol_manager().find_address_name(address),
		&ok);

	if(ok) {
		edb::v1::symbol_manager().set_label(address, text);
		refresh_gui();
	}
}

//------------------------------------------------------------------------------
// Name: mnuCPUSetEIP
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuCPUSetEIP() {
	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(std::shared_ptr<IThread> thread = process->current_thread()) {
			const edb::address_t address = ui.cpuView->selectedAddress();
			State state;
			thread->get_state(&state);
			state.set_instruction_pointer(address);
			thread->set_state(state);
			update_gui();
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

	quint8 buf[edb::Instruction::MAX_SIZE];

	Q_ASSERT(size <= sizeof(buf));

	if(IProcess *process = edb::v1::debugger_core->process()) {
		const bool ok = process->read_bytes(address, buf, size);
		if(ok) {
			QByteArray bytes = QByteArray::fromRawData(reinterpret_cast<const char *>(buf), size);
			if(edb::v1::get_binary_string_from_user(bytes, tr("Edit Binary String"))) {
				edb::v1::modify_bytes(address, bytes.size(), bytes, 0x00);
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: modify_bytes
// Desc:
//------------------------------------------------------------------------------
template <class T>
void Debugger::modify_bytes(const T &hexview) {
	if(hexview) {
		const edb::address_t address = hexview->selectedBytesAddress();
		QByteArray bytes             = hexview->selectedBytes();

		if(edb::v1::get_binary_string_from_user(bytes, tr("Edit Binary String"))) {
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
	if(focusedWidget == ui.cpuView) {
		mnuCPUModify();
	} else if(focusedWidget == stack_view_.get()) {
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
	modify_bytes(qobject_cast<QHexView *>(ui.tabWidget->currentWidget()));
}

//------------------------------------------------------------------------------
// Name: mnuStackModify
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuStackModify() {
	modify_bytes(stack_view_);
}

//------------------------------------------------------------------------------
// Name: breakpoint_condition_true
// Desc:
//------------------------------------------------------------------------------
bool Debugger::breakpoint_condition_true(const QString &condition) {

	if(boost::optional<edb::address_t> condition_value = edb::v2::eval_expression(condition)) {
		return *condition_value;
	}
	return true;
}

//------------------------------------------------------------------------------
// Name: handle_trap
// Desc: returns true if we should resume as if this trap never happened
//------------------------------------------------------------------------------
edb::EVENT_STATUS Debugger::handle_trap(const std::shared_ptr<IDebugEvent> &event) {

	// we just got a trap event, there are a few possible causes
	// #1: we hit a 0xcc breakpoint, if so, then we want to stop
	// #2: we did a step
	// #3: we hit a 0xcc naturally in the program
	// #4: we hit a hardware breakpoint!
	IProcess *process = edb::v1::debugger_core->process();
	Q_ASSERT(process);

	State state;
	process->current_thread()->get_state(&state);

	// look it up in our breakpoint list, make sure it is one of OUR int3s!
	// if it is, we need to backup EIP and pause ourselves
	const std::shared_ptr<IBreakpoint> bp = event->trap_reason()==IDebugEvent::TRAP_STEPPING ?
												nullptr :
												edb::v1::find_triggered_breakpoint(state.instruction_pointer());

	if(bp && bp->enabled()) {

		const edb::address_t previous_ip = bp->address();

		// TODO: check if the breakpoint was corrupted?
		bp->hit();

		// back up eip the size of a breakpoint, since we executed a breakpoint
		// instead of the real code that belongs there
		state.set_instruction_pointer(previous_ip);
		process->current_thread()->set_state(state);

#if defined(Q_OS_LINUX)
		// test if we have hit our internal LD hook BP. If so, read in the r_debug
		// struct so we can get the state, then we can just resume
		// TODO(eteran): add an option to let the user stop of debug events
		if(bp->internal() && bp->tag == ld_loader_tag) {

			if(dynamic_info_bp_set_) {
				if(debug_pointer_) {
					if(edb::v1::debuggeeIs32Bit()) {
						handle_library_event<uint32_t>(process, debug_pointer_);
					} else {
						handle_library_event<uint64_t>(process, debug_pointer_);
					}
				}
			}

			if(edb::v1::config().break_on_library_load) {
				return edb::DEBUG_STOP;
			} else {
				return edb::DEBUG_CONTINUE_BP;
			}
		}
#endif

		const QString condition = bp->condition;

		// handle conditional breakpoints
		if(!condition.isEmpty()) {
			if(!breakpoint_condition_true(condition)) {
				return edb::DEBUG_CONTINUE_BP;
			}
		}

		// if it's a one time breakpoint then we should remove it upon
		// triggering, this is mainly used for situations like step over

		if(bp->one_time()) {
			edb::v1::debugger_core->remove_breakpoint(bp->address());
		}
	}

	return edb::DEBUG_STOP;
}

//------------------------------------------------------------------------------
// Name: handle_event_stopped
// Desc:
//------------------------------------------------------------------------------
edb::EVENT_STATUS Debugger::handle_event_stopped(const std::shared_ptr<IDebugEvent> &event) {

	// ok we just came in from a stop, we need to test some things,
	// generally, we will want to check if it was a step, if it was, was it
	// because we just hit a break point or because we wanted to run, but had
	// to step first in case were were on a breakpoint already...

	edb::v1::clear_status();

	if(event->is_kill()) {
		QMessageBox::information(
			this,
			tr("Application Killed"),
			tr("The debugged application was killed."));

		on_action_Detach_triggered();
		return edb::DEBUG_STOP;
	}

	if(event->is_error()) {
		const IDebugEvent::Message message = event->error_description();
		edb::v1::set_status(message.statusMessage,0);
		if(edb::v1::config().enable_signals_message_box)
			QMessageBox::information(this, message.caption, message.message);
		return edb::DEBUG_STOP;
	}

	if(event->is_trap()) {
		return handle_trap(event);
	}

	if(event->is_stop()) {
		// user asked to pause the debugged process
		return edb::DEBUG_STOP;
	}

	Q_ASSERT(edb::v1::debugger_core);
	QMap<qlonglong, QString> known_exceptions = edb::v1::debugger_core->exceptions();
	auto it = known_exceptions.find(event->code());

	if(it != known_exceptions.end()) {

		const Configuration &config = edb::v1::config();

		QString exception_name = it.value();

		edb::v1::set_status(tr("%1 signal received. Shift+Step/Run to pass to program, Step/Run to ignore").arg(exception_name),0);
    	if(config.enable_signals_message_box) {
			QMessageBox::information(this, tr("Debug Event"),
				tr(
				"<p>The debugged application has received a debug event-> <strong>%1 (%2)</strong></p>"
				"<p>If you would like to pass this event to the application press Shift+[F7/F8/F9]</p>"
				"<p>If you would like to ignore this event, press [F7/F8/F9]</p>").arg(event->code()).arg(exception_name));
		}
	} else {
		edb::v1::set_status(tr("Signal received: %1. Shift+Step/Run to pass to program, Step/Run to ignore").arg(event->code()),0);
		if(edb::v1::config().enable_signals_message_box) {
			QMessageBox::information(this, tr("Debug Event"),
				tr(
				"<p>The debugged application has received a debug event-> <strong>%1</strong></p>"
				"<p>If you would like to pass this event to the application press Shift+[F7/F8/F9]</p>"
				"<p>If you would like to ignore this event, press [F7/F8/F9]</p>").arg(event->code()));
		}
	}

	return edb::DEBUG_STOP;
}

//------------------------------------------------------------------------------
// Name: handle_event_terminated
// Desc:
//------------------------------------------------------------------------------
edb::EVENT_STATUS Debugger::handle_event_terminated(const std::shared_ptr<IDebugEvent> &event) {
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
edb::EVENT_STATUS Debugger::handle_event_exited(const std::shared_ptr<IDebugEvent> &event) {
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
edb::EVENT_STATUS Debugger::handle_event(const std::shared_ptr<IDebugEvent> &event) {

	Q_ASSERT(edb::v1::debugger_core);

	edb::EVENT_STATUS status;
	switch(event->reason()) {
	// either a syncronous event (STOPPED)
	// or an asyncronous event (SIGNALED)
	case IDebugEvent::EVENT_STOPPED:
		status = handle_event_stopped(event);
		break;

	case IDebugEvent::EVENT_TERMINATED:
		status = handle_event_terminated(event);
		break;

	// normal exit
	case IDebugEvent::EVENT_EXITED:
		status = handle_event_exited(event);
		break;

	default:
		Q_ASSERT(false);
		return edb::DEBUG_EXCEPTION_NOT_HANDLED;
	}

	Q_ASSERT(!(reenable_breakpoint_step_ && reenable_breakpoint_run_));

	// re-enable any breakpoints we previously disabled
	if(reenable_breakpoint_step_) {
		reenable_breakpoint_step_->enable();
		reenable_breakpoint_step_ = nullptr;
	} else if(reenable_breakpoint_run_) {
		reenable_breakpoint_run_->enable();
		reenable_breakpoint_run_ = nullptr;
		status = edb::DEBUG_CONTINUE;
	}

	return status;
}

//------------------------------------------------------------------------------
// Name: update_tab_caption
// Desc:
//------------------------------------------------------------------------------
void Debugger::update_tab_caption(const std::shared_ptr<QHexView> &view, edb::address_t start, edb::address_t end) {
	const int index = ui.tabWidget->indexOf(view.get());
	const QString caption = ui.tabWidget->data(index).toString();

	if(caption.isEmpty()) {
		ui.tabWidget->setTabText(index, tr("%1-%2").arg(edb::v1::format_pointer(start), edb::v1::format_pointer(end)));
	} else {
		ui.tabWidget->setTabText(index, tr("[%1] %2-%3").arg(caption, edb::v1::format_pointer(start), edb::v1::format_pointer(end)));
	}
}

//------------------------------------------------------------------------------
// Name: update_data
// Desc:
//------------------------------------------------------------------------------
void Debugger::update_data(const std::shared_ptr<DataViewInfo> &v) {

	Q_ASSERT(v);

	const std::shared_ptr<QHexView> &view = v->view;

	Q_ASSERT(view);

	v->update();

	update_tab_caption(view, v->region->start(), v->region->end());
}

//------------------------------------------------------------------------------
// Name: clear_data
// Desc:
//------------------------------------------------------------------------------
void Debugger::clear_data(const std::shared_ptr<DataViewInfo> &v) {

	Q_ASSERT(v);

	const std::shared_ptr<QHexView> &view = v->view;

	Q_ASSERT(view);

	view->clear();
	view->scrollTo(0);

	update_tab_caption(view, 0, 0);
}

//------------------------------------------------------------------------------
// Name: do_jump_to_address
// Desc:
//------------------------------------------------------------------------------
void Debugger::do_jump_to_address(edb::address_t address, const std::shared_ptr<IRegion> &r, bool scrollTo) {

	ui.cpuView->setRegion(r);
	if(scrollTo && !ui.cpuView->addressShown(address)) {
		ui.cpuView->scrollTo(address);
	}
	ui.cpuView->setSelectedAddress(address);
}

//------------------------------------------------------------------------------
// Name: update_disassembly
// Desc:
//------------------------------------------------------------------------------
void Debugger::update_disassembly(edb::address_t address, const std::shared_ptr<IRegion> &r) {
	ui.cpuView->setCurrentAddress(address);
	do_jump_to_address(address, r, true);
	list_model_->setStringList(edb::v1::arch_processor().update_instruction_info(address));
}

//------------------------------------------------------------------------------
// Name: update_stack_view
// Desc:
//------------------------------------------------------------------------------
void Debugger::update_stack_view(const State &state) {
	if(!edb::v1::dump_stack(state.stack_pointer(), !stack_view_locked_)) {
		stack_view_->clear();
		stack_view_->scrollTo(0);
	}
}

//------------------------------------------------------------------------------
// Name: update_cpu_view
// Desc:
//------------------------------------------------------------------------------
std::shared_ptr<IRegion> Debugger::update_cpu_view(const State &state) {
	const edb::address_t address = state.instruction_pointer();

	if(std::shared_ptr<IRegion> region = edb::v1::memory_regions().find_region(address)) {
		update_disassembly(address, region);
		return region;
	} else {
		ui.cpuView->clear();
		ui.cpuView->scrollTo(0);
		list_model_->setStringList(QStringList());
		return nullptr;
	}
}

//------------------------------------------------------------------------------
// Name: update_data_views
// Desc:
//------------------------------------------------------------------------------
void Debugger::update_data_views() {

	// update all data views with the current region data
	Q_FOREACH(const std::shared_ptr<DataViewInfo> &info, data_regions_) {

		// make sure the regions are still valid..
		if(info->region && edb::v1::memory_regions().find_region(info->region->start())) {
			update_data(info);
		} else {
			clear_data(info);
		}
	}
}

//------------------------------------------------------------------------------
// Name: refresh_gui
// Desc: refreshes all the different displays
//------------------------------------------------------------------------------
void Debugger::refresh_gui() {

	ui.cpuView->update();
	stack_view_->update();

	Q_FOREACH(const std::shared_ptr<DataViewInfo> &info, data_regions_) {
		info->view->update();
	}

	if(edb::v1::debugger_core) {
		State state;

		if(IProcess *process = edb::v1::debugger_core->process()) {
			if(std::shared_ptr<IThread> thread = process->current_thread()) {
				thread->get_state(&state);
			}
		}

		list_model_->setStringList(edb::v1::arch_processor().update_instruction_info(state.instruction_pointer()));
	}
}

//------------------------------------------------------------------------------
// Name: update_gui
// Desc: updates all the different displays
//------------------------------------------------------------------------------
void Debugger::update_gui() {

	if(edb::v1::debugger_core) {

		State state;
		if(IProcess *process = edb::v1::debugger_core->process()) {
			if(std::shared_ptr<IThread> thread = process->current_thread()) {
				thread->get_state(&state);
			}
		}

		update_data_views();
		update_stack_view(state);

		if(const std::shared_ptr<IRegion> region = update_cpu_view(state)) {
			edb::v1::arch_processor().update_register_view(region->name(), state);
		} else {
			edb::v1::arch_processor().update_register_view(QString(), state);
		}
	}

	//Signal all connected slots that the GUI has been updated.
	//Useful for plugins with windows that should updated after
	//hitting breakpoints, Step Over, etc.
	Q_EMIT gui_updated();
}

//------------------------------------------------------------------------------
// Name: resume_status
// Desc:
//------------------------------------------------------------------------------
edb::EVENT_STATUS Debugger::resume_status(bool pass_exception) {

	if(pass_exception && last_event_ && last_event_->stopped() && !last_event_->is_trap()) {
		return edb::DEBUG_EXCEPTION_NOT_HANDLED;
	} else {
		return edb::DEBUG_CONTINUE;
	}
}

//------------------------------------------------------------------------------
// Name: resume_execution
// Desc: resumes execution, handles the situation of being on a breakpoint as well
//------------------------------------------------------------------------------
void Debugger::resume_execution(EXCEPTION_RESUME pass_exception, DEBUG_MODE mode, ResumeFlags flags) {

	edb::v1::clear_status();
	Q_ASSERT(edb::v1::debugger_core);

	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(std::shared_ptr<IThread> thread = process->current_thread()) {

			// if necessary pass the trap to the application, otherwise just resume
			// as normal
			const edb::EVENT_STATUS status = resume_status(pass_exception == PASS_EXCEPTION);

			// if we are on a breakpoint, disable it
			std::shared_ptr<IBreakpoint> bp;
			if(!(flags & ResumeFlag::Forced)) {
				State state;
				thread->get_state(&state);
				bp = edb::v1::debugger_core->find_breakpoint(state.instruction_pointer());
				if(bp) {
					bp->disable();
				}
			}

			edb::v1::arch_processor().about_to_resume();

			if(mode == MODE_STEP) {
				reenable_breakpoint_step_ = bp;
				const auto stepStatus = thread->step(status);
				if(!stepStatus) {
					QMessageBox::critical(this, tr("Error"), tr("Failed to step thread: %1").arg(stepStatus.error()));
					return;
				}
			} else if(mode == MODE_RUN) {
				reenable_breakpoint_run_ = bp;
				if(bp) {
					const auto stepStatus = thread->step(status);
					if(!stepStatus) {
						QMessageBox::critical(this, tr("Error"), tr("Failed to step thread: %1").arg(stepStatus.error()));
						return;
					}
				} else {
					const auto resumeStatus = process->resume(status);
					if(!resumeStatus) {
						QMessageBox::critical(this, tr("Error"), tr("Failed to resume process: %1").arg(resumeStatus.error()));
						return;
					}

				}
			}

			// set the state to 'running'
			update_menu_state(RUNNING);
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_action_Run_Pass_Signal_To_Application_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Run_Pass_Signal_To_Application_triggered() {
	resume_execution(PASS_EXCEPTION, MODE_RUN, ResumeFlag::None);
}

//------------------------------------------------------------------------------
// Name: on_action_Step_Into_Pass_Signal_To_Application_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Step_Into_Pass_Signal_To_Application_triggered() {
	resume_execution(PASS_EXCEPTION, MODE_STEP, ResumeFlag::None);
}

//------------------------------------------------------------------------------
// Name: on_action_Run_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Run_triggered() {
	resume_execution(IGNORE_EXCEPTION, MODE_RUN, ResumeFlag::None);
}

//------------------------------------------------------------------------------
// Name: on_action_Step_Into_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Step_Into_triggered() {
	resume_execution(IGNORE_EXCEPTION, MODE_STEP, ResumeFlag::None);
}

//------------------------------------------------------------------------------
// Name: on_action_Detach_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Detach_triggered() {
	detach_from_process(NO_KILL_ON_DETACH);
}

//------------------------------------------------------------------------------
// Name: on_action_Kill_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Kill_triggered() {
	detach_from_process(KILL_ON_DETACH);
}

//------------------------------------------------------------------------------
// Name: on_action_Step_Over_Pass_Signal_To_Application_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Step_Over_Pass_Signal_To_Application_triggered() {
	step_over([this]() { on_action_Run_Pass_Signal_To_Application_triggered(); },
	          [this]() { on_action_Step_Into_Pass_Signal_To_Application_triggered(); });
}

//------------------------------------------------------------------------------
// Name: on_action_Step_Over_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Step_Over_triggered() {
	step_over([this]() { on_action_Run_triggered(); },
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
	step_over([this]() { on_action_Run_triggered(); },
	          [this]() { on_action_Step_Into_triggered(); });
}

//------------------------------------------------------------------------------
// Name: on_action_Pause_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Pause_triggered() {
	Q_ASSERT(edb::v1::debugger_core);
	if(IProcess *process = edb::v1::debugger_core->process()) {
		process->pause();
	}
}

//------------------------------------------------------------------------------
// Name: cleanup_debugger
// Desc:
//------------------------------------------------------------------------------
void Debugger::cleanup_debugger() {

	timer_->stop();

	ui.cpuView->clear_comments();
	edb::v1::memory_regions().clear();
	edb::v1::symbol_manager().clear();
	edb::v1::arch_processor().reset();

	// clear up the data view
	while(ui.tabWidget->count() > 1) {
		mnuDumpDeleteTab();
	}

	ui.tabWidget->setData(0, QString());

	Q_ASSERT(!data_regions_.isEmpty());
	data_regions_.first()->region = nullptr;

	Q_EMIT detachEvent();

	setWindowTitle(tr("edb"));

	update_gui();
}

//------------------------------------------------------------------------------
// Name: session_filename
// Desc:
//------------------------------------------------------------------------------
QString Debugger::session_filename() const {

	static bool show_path_notice = true;

	QString session_path = edb::v1::config().session_path;
	if(session_path.isEmpty()) {
		if(show_path_notice) {
			qDebug() << "No session path specified. Please set it in the preferences to enable sessions.";
			show_path_notice = false;
		}
		return QString();
	}

	if(!program_executable_.isEmpty()) {
		QFileInfo info(program_executable_);

		if(info.isRelative()) {
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
void Debugger::detach_from_process(DETACH_ACTION kill) {

	const QString filename = session_filename();
	if(!filename.isEmpty()) {
		SessionManager::instance().save_session(filename);
	}

	program_executable_.clear();

	if(edb::v1::debugger_core) {
		if(kill == KILL_ON_DETACH) edb::v1::debugger_core->kill();
		else                       edb::v1::debugger_core->detach();
	}

	last_event_ = nullptr;

	cleanup_debugger();
	update_menu_state(TERMINATED);
}

//------------------------------------------------------------------------------
// Name: set_initial_debugger_state
// Desc: resets all of the basic data to sane defaults
//------------------------------------------------------------------------------
void Debugger::set_initial_debugger_state() {

	update_menu_state(PAUSED);
	timer_->start(0);

	edb::v1::symbol_manager().clear();
	edb::v1::memory_regions().sync();

	Q_ASSERT(data_regions_.size() > 0);

	data_regions_.first()->region = edb::v1::primary_data_region();

	if(IAnalyzer *const analyzer = edb::v1::analyzer()) {
		analyzer->invalidate_analysis();
	}

	reenable_breakpoint_run_  = nullptr;
	reenable_breakpoint_step_ = nullptr;

#ifdef Q_OS_LINUX
	debug_pointer_ = 0;
	dynamic_info_bp_set_ = false;
#endif

	IProcess *process = edb::v1::debugger_core->process();

	const QString executable = process ? process->executable() : QString();

	set_debugger_caption(executable);

	program_executable_.clear();
	if(!executable.isEmpty()) {
		program_executable_ = executable;
	}

	const QString filename = session_filename();
	if(!filename.isEmpty()) {

		SessionManager &session_manager = SessionManager::instance();

		if(Result<void, SessionError> session_error = session_manager.load_session(filename)) {
			QVariantList comments_data;
			session_manager.get_comments(comments_data);
			ui.cpuView->restoreComments(comments_data);
		} else {
			QMessageBox::warning(
				this,
				tr("Error Loading Session"),
			    session_error.error().message
			);
		}
	}

	// create our binary info object for the primary code module
	binary_info_ = edb::v1::get_binary_info(edb::v1::primary_code_region());

    comment_server_->clear();
	comment_server_->set_comment(process->entry_point(), "<entry point>");
}

//------------------------------------------------------------------------------
// Name: test_native_binary
// Desc:
//------------------------------------------------------------------------------
void Debugger::test_native_binary() {
	if(EDB_IS_32_BIT && binary_info_ && !binary_info_->native()) {
		QMessageBox::warning(
			this,
			tr("Not A Native Binary"),
			tr("The program you just attached to was built for a different architecture than the one that edb was built for. "
			"For example a AMD64 binary on EDB built for IA32. "
			"This is not fully supported yet, so you may need to use a version of edb that was compiled for the same architecture as your target program")
			);
	}
}

//------------------------------------------------------------------------------
// Name: set_initial_breakpoint
// Desc: sets the initial breakpoint so we can stop at the entry point of the
//       application
//------------------------------------------------------------------------------
void Debugger::set_initial_breakpoint(const QString &s) {

	edb::address_t entryPoint = 0;

	if(edb::v1::config().initial_breakpoint == Configuration::MainSymbol) {
		const QString mainSymbol = QFileInfo(s).fileName() + "!main";
		const std::shared_ptr<Symbol> sym = edb::v1::symbol_manager().find(mainSymbol);

		if(sym) {
			entryPoint = sym->address;
		} else if(edb::v1::config().find_main) {
			entryPoint = edb::v1::locate_main_function();
		}
	}

	if(entryPoint == 0 || edb::v1::config().initial_breakpoint == Configuration::EntryPoint) {
		entryPoint = edb::v1::debugger_core->process()->entry_point();
	}

	if(entryPoint != 0) {
		if(std::shared_ptr<IBreakpoint> bp = edb::v1::debugger_core->add_breakpoint(entryPoint)) {
			bp->set_one_time(true);
			bp->set_internal(true);
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
	if(edb::v1::debugger_core->process()) {

		working_directory_     = edb::v1::debugger_core->process()->current_working_directory();
		QList<QByteArray> args = edb::v1::debugger_core->process()->arguments();
		const QString s        = edb::v1::debugger_core->process()->executable();

		if(!args.empty()) {
			args.removeFirst();
		}

		if(!s.isEmpty()) {
			detach_from_process(KILL_ON_DETACH);
			common_open(s, args);
		}
	} else {
		const auto file = recent_file_manager_->most_recent();
		if(common_open(file.first, file.second))
			arguments_dialog_->set_arguments(file.second);
	}
}

//------------------------------------------------------------------------------
// Name: setup_data_views
// Desc:
//------------------------------------------------------------------------------
void Debugger::setup_data_views() {

	// Setup data views according to debuggee bitness
	if(edb::v1::debuggeeIs64Bit()) {
		stack_view_->setAddressSize(QHexView::Address64);
		Q_FOREACH(const std::shared_ptr<DataViewInfo> &data_view, data_regions_) {
			data_view->view->setAddressSize(QHexView::Address64);
		}
	} else {
		stack_view_->setAddressSize(QHexView::Address32);
		Q_FOREACH(const std::shared_ptr<DataViewInfo> &data_view, data_regions_) {
			data_view->view->setAddressSize(QHexView::Address32);
		}
	}

	// Update stack word width
	stack_view_->setWordWidth(edb::v1::pointer_size());
}

//------------------------------------------------------------------------------
// Name: common_open
// Desc:
//------------------------------------------------------------------------------
bool Debugger::common_open(const QString &s, const QList<QByteArray> &args) {

	// ensure that the previous running process (if any) is dealth with...
	detach_from_process(KILL_ON_DETACH);

	bool ret = false;
	tty_file_ = create_tty();

	if(const Status status = edb::v1::debugger_core->open(s, working_directory_, args, tty_file_)) {
		attachComplete();
		set_initial_breakpoint(s);
		ret = true;
	} else {
		QMessageBox::critical(
			this,
			tr("Could Not Open"),
		    tr("Failed to open and attach to process:\n%1.").arg(status.error()));
	}

	update_gui();
	return ret;
}

//------------------------------------------------------------------------------
// Name: execute
// Desc:
//------------------------------------------------------------------------------
void Debugger::execute(const QString &program, const QList<QByteArray> &args) {
	if(common_open(program, args)) {
		recent_file_manager_->add_file(program,args);
		arguments_dialog_->set_arguments(args);
	}
}

//------------------------------------------------------------------------------
// Name: open_file
// Desc:
//------------------------------------------------------------------------------
void Debugger::open_file(const QString &s, const QList<QByteArray> &a) {
	if(!s.isEmpty()) {
		last_open_directory_ = QFileInfo(s).canonicalFilePath();

		execute(s, a);
	}
}

//------------------------------------------------------------------------------
// Name: attach
// Desc:
//------------------------------------------------------------------------------
void Debugger::attach(edb::pid_t pid) {
#if defined(Q_OS_UNIX)
	edb::pid_t current_pid = getpid();
	while(current_pid != 0) {
		if(current_pid == pid) {

            int ret = QMessageBox::question(this,
                                            tr("Attaching to parent"),
                                            tr("You are attempting to attach to a process which is a parent of edb, sometimes, this can lead to deadlocks. Do you want to proceed?"),
                                            QMessageBox::Yes | QMessageBox::No);
            if(ret != QMessageBox::Yes) {
                return;
            }
		}
		current_pid = edb::v1::debugger_core->parent_pid(current_pid);
	}
#endif

	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(pid == process->pid()) {
			QMessageBox::critical(
				this,
				tr("Attach"),
				tr("You are already debugging that process!"));
			return;
		}
	}

	if(const auto status = edb::v1::debugger_core->attach(pid)) {

		working_directory_ = edb::v1::debugger_core->process()->current_working_directory();

		QList<QByteArray> args = edb::v1::debugger_core->process()->arguments();

		if(!args.empty()) {
			args.removeFirst();
		}

		arguments_dialog_->set_arguments(args);
		attachComplete();
	} else {
		QMessageBox::critical(this, tr("Attach"), tr("Failed to attach to process: %1").arg(status.error()));
	}

	update_gui();
}

//------------------------------------------------------------------------------
// Name: attachComplete
// Desc:
//------------------------------------------------------------------------------
void Debugger::attachComplete() {
	set_initial_debugger_state();

	test_native_binary();

	setup_data_views();

	QString ip   = edb::v1::debugger_core->instruction_pointer().toUpper();
	QString sp   = edb::v1::debugger_core->stack_pointer().toUpper();
	QString bp   = edb::v1::debugger_core->frame_pointer().toUpper();
	QString word = edb::v1::debuggeeIs64Bit() ? "QWORD" : "DWORD";

	setRIPAction_      ->setText(tr("&Set %1 to this Instruction").arg(ip));
	gotoRIPAction_     ->setText(tr("&Goto %1").arg(ip));
	stackGotoRSPAction_->setText(tr("Goto %1").arg(sp));
	stackGotoRBPAction_->setText(tr("Goto %1").arg(bp));
	stackPushAction_   ->setText(tr("&Push %1").arg(word));
	stackPopAction_    ->setText(tr("P&op %1").arg(word));

	Q_EMIT attachEvent();
}

//------------------------------------------------------------------------------
// Name: on_action_Open_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Open_triggered() {

	static auto dialog = new DialogOpenProgram(this, tr("Choose a file"), last_open_directory_);

	// Set a sensible default dir
	if(recent_file_manager_->entry_count() > 0) {
		const RecentFileManager::RecentFile file = recent_file_manager_->most_recent();
		const QDir dir = QFileInfo(file.first).dir();
		if(dir.exists()) {
			dialog->setDirectory(dir);
		}
	}

	if(dialog->exec() == QDialog::Accepted) {

		arguments_dialog_->set_arguments(dialog->arguments());
		QStringList files = dialog->selectedFiles();
		const QString filename = files.front();
		working_directory_ = dialog->workingDirectory();
		open_file(filename, dialog->arguments());
	}
}

//------------------------------------------------------------------------------
// Name: on_action_Attach_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Attach_triggered() {

	QPointer<DialogAttach> dlg = new DialogAttach(this);

	if(dlg->exec() == QDialog::Accepted) {
		if(dlg) {
			if(const Result<edb::pid_t, QString> pid = dlg->selected_pid()) {
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
	create_data_tab();
	del_tab_->setEnabled(ui.tabWidget->count() > 1);
}

//------------------------------------------------------------------------------
// Name: mnuDumpDeleteTab
// Desc: handles removing of a memory view tab
//------------------------------------------------------------------------------
void Debugger::mnuDumpDeleteTab() {
	delete_data_tab();
	del_tab_->setEnabled(ui.tabWidget->count() > 1);
}

//------------------------------------------------------------------------------
// Name: get_plugin_context_menu_items
// Desc: Returns context menu items using supplied function to call for each plugin.
//       NULL pointer items mean "create separator here".
//------------------------------------------------------------------------------
template <class F>
QList<QAction*> Debugger::get_plugin_context_menu_items(const F &f) const {
	QList<QAction*> actions;

	for(QObject *plugin: edb::v1::plugin_list()) {
		if(auto p = qobject_cast<IPlugin *>(plugin)) {
			const QList<QAction *> acts = (p->*f)();
			if(!acts.isEmpty()) {
				actions.push_back(nullptr);
				actions.append(acts);
			}
		}
	}
	return actions;
}

//------------------------------------------------------------------------------
// Name: add_plugin_context_menu
// Desc:
//------------------------------------------------------------------------------
template <class F, class T>
void Debugger::add_plugin_context_menu(const T &menu, const F &f) {
	for(QObject *plugin: edb::v1::plugin_list()) {
		if(auto p = qobject_cast<IPlugin *>(plugin)) {
			const QList<QAction *> acts = (p->*f)();
			if(!acts.isEmpty()) {
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
bool Debugger::jump_to_address(edb::address_t address) {

	if(std::shared_ptr<IRegion> region = edb::v1::memory_regions().find_region(address)) {
		do_jump_to_address(address, region, true);
		return true;
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: dump_data_range
// Desc:
//------------------------------------------------------------------------------
bool Debugger::dump_data_range(edb::address_t address, edb::address_t end_address, bool new_tab) {

	if(std::shared_ptr<IRegion> region = edb::v1::memory_regions().find_region(address)) {
		if(new_tab) {
			mnuDumpCreateTab();
		}

		if(std::shared_ptr<DataViewInfo> info = current_data_view_info()) {
			info->region = std::shared_ptr<IRegion>(region->clone());

			if(info->region->contains(end_address)) {
				info->region->set_end(end_address);
			}

			if(info->region->contains(address)) {
				info->region->set_start(address);
			}

			update_data(info);
			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: dump_data
// Desc:
//------------------------------------------------------------------------------
bool Debugger::dump_data(edb::address_t address, bool new_tab) {

	if(std::shared_ptr<IRegion> region = edb::v1::memory_regions().find_region(address)) {
		if(new_tab) {
			mnuDumpCreateTab();
		}

		std::shared_ptr<DataViewInfo> info = current_data_view_info();

		if(info) {
			info->region = region;
			update_data(info);
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
bool Debugger::dump_stack(edb::address_t address, bool scroll_to) {
	const std::shared_ptr<IRegion> last_region = stack_view_info_.region;
	stack_view_info_.region = edb::v1::memory_regions().find_region(address);

	if(stack_view_info_.region) {
		stack_view_info_.update();

		if(IProcess *process = edb::v1::debugger_core->process()) {
			if(std::shared_ptr<IThread> thread = process->current_thread()) {

				State state;
				thread->get_state(&state);
				stack_view_->setColdZoneEnd(state.stack_pointer());

				if(scroll_to || stack_view_info_.region->equals(last_region)) {
					stack_view_->scrollTo(address - stack_view_info_.region->start());
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
void Debugger::tab_context_menu(int index, const QPoint &pos) {
	QMenu menu;
	QAction *const actionAdd   = menu.addAction(tr("&Set Label"));
	QAction *const actionClear = menu.addAction(tr("&Clear Label"));
	QAction *const chosen      = menu.exec(ui.tabWidget->mapToGlobal(pos));

	if(chosen == actionAdd) {
		bool ok;
		const QString text = QInputDialog::getText(
			this,
			tr("Set Caption"),
			tr("Caption:"),
			QLineEdit::Normal,
			ui.tabWidget->data(index).toString(),
			&ok);

		if(ok && !text.isEmpty()) {
			ui.tabWidget->setData(index, text);
		}
	} else if(chosen == actionClear) {
		ui.tabWidget->setData(index, QString());
	}

	update_gui();
}

//------------------------------------------------------------------------------
// Name: next_debug_event
// Desc:
//------------------------------------------------------------------------------
void Debugger::next_debug_event() {


	// TODO(eteran): come up with a nice system to inject a debug event
	//               into the system, for example on windows, we want
	//               to deliver real "memory map updated" events, but
	//               on linux, (at least for now), I would want to send
	//               a fake one before every event so it is always up to
	//               date.

	Q_ASSERT(edb::v1::debugger_core);

	if(std::shared_ptr<IDebugEvent> e = edb::v1::debugger_core->wait_debug_event(10)) {

		last_event_ = e;

		// TODO(eteran): disable this in favor of only doing it on library load events
		//               once we are confident. We should be able to just enclose it inside
		//               an "if(!dynamic_info_bp_set_) {" test (since we still want to
		//               do this when the hook isn't set.
		edb::v1::memory_regions().sync();

#if defined(Q_OS_LINUX)
		if(!dynamic_info_bp_set_) {
			if(IProcess *process = edb::v1::debugger_core->process()) {
				if(debug_pointer_ == 0) {
					if((debug_pointer_ = process->debug_pointer()) != 0) {
						edb::address_t r_brk = edb::v1::debuggeeIs32Bit() ?
							find_linker_hook_address<uint32_t>(process, debug_pointer_) :
							find_linker_hook_address<uint64_t>(process, debug_pointer_);

						if(r_brk) {
							// TODO(eteran): this is equivalent to ld-2.23.so!_dl_debug_state
							// maybe we should prefer setting this by symbol if possible?
							if(std::shared_ptr<IBreakpoint> bp = edb::v1::debugger_core->add_breakpoint(r_brk)) {
								bp->set_internal(true);
								bp->tag = ld_loader_tag;
								dynamic_info_bp_set_ = true;
							}
						}
					}
				}
			}
		}
#endif

		Q_EMIT debugEvent();

		const edb::EVENT_STATUS status = edb::v1::execute_debug_event_handlers(e);
		switch(status) {
		case edb::DEBUG_STOP:
			update_gui();
			update_menu_state(edb::v1::debugger_core->process() ? PAUSED : TERMINATED);
			break;
		case edb::DEBUG_CONTINUE:
			resume_execution(IGNORE_EXCEPTION, MODE_RUN, ResumeFlag::Forced);
			break;
		case edb::DEBUG_CONTINUE_BP:
			resume_execution(IGNORE_EXCEPTION, MODE_RUN, ResumeFlag::None);
			break;
		case edb::DEBUG_CONTINUE_STEP:
			resume_execution(IGNORE_EXCEPTION, MODE_STEP, ResumeFlag::Forced);
			break;
		case edb::DEBUG_EXCEPTION_NOT_HANDLED:
			resume_execution(PASS_EXCEPTION, MODE_RUN, ResumeFlag::Forced);
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

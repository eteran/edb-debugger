/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "Debugger.h"
#include "ArchProcessor.h"
#include "CommentServer.h"
#include "Configuration.h"
#include "DebuggerInternal.h"
#include "DialogAbout.h"
#include "DialogArguments.h"
#include "DialogAttach.h"
#include "DialogBreakpoints.h"
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
#include "Module.h"
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
#include <QSplitter>
#include <QStringListModel>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVector>
#include <QtDebug>

#include <clocale>
#include <cstring>
#include <memory>
#include <random>

#if defined(Q_OS_UNIX)
#include <csignal>
#endif

#include <sys/stat.h>
#include <sys/types.h>
#if defined(Q_OS_LINUX) || defined(Q_OS_OPENBSD) || defined(Q_OS_FREEBSD)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

QPlainTextEdit *logger_instance = nullptr;

constexpr quint64 initial_bp_tag    = Q_UINT64_C(0x494e4954494e5433); // "INITINT3" in hex
constexpr quint64 stepover_bp_tag   = Q_UINT64_C(0x535445504f564552); // "STEPOVER" in hex
constexpr quint64 run_to_cursor_tag = Q_UINT64_C(0x474f544f48455245); // "GOTOHERE" in hex
#ifdef Q_OS_LINUX
constexpr quint64 ld_loader_tag = Q_UINT64_C(0x4c49424556454e54); // "LIBEVENT" in hex
#endif

/**
 * @brief Checks if the instruction at the given address is a return instruction.
 *
 * @param address The address of the instruction to check.
 * @return True if the instruction is a return, false otherwise.
 */
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
	/**
	 * @brief Constructs a new RunUntilRet object.
	 */
	RunUntilRet() {
		edb::v1::add_debug_event_handler(this);
	}

	/**
	 * @brief Destroys the RunUntilRet object.
	 */
	~RunUntilRet() override {
		edb::v1::remove_debug_event_handler(this);

		try {
			for (const auto &[address, breakpoint] : ownBreakpoints_) {
				if (!breakpoint.expired()) {
					edb::v1::debugger_core->removeBreakpoint(address);
				}
			}
		} catch (...) {
			EDB_PRINT_AND_DIE("unexpected exception occurred");
		}
	}

	/**
	 * @brief Passes control back to the debugger and deletes this object.
	 * @return The event status.
	 */
	virtual edb::EventStatus pass_back_to_debugger() {
		delete this;
		return edb::DEBUG_NEXT_HANDLER;
	}

	/**
	 * @brief Handles the debug event.
	 * @param event The debug event to handle.
	 * @return The event status.
	 */
	edb::EventStatus handleEvent(const std::shared_ptr<IDebugEvent> &event) override {

		// TODO: Need to handle stop/pause button

		if (!event->isTrap()) {
			return pass_back_to_debugger();
		}

		if (IProcess *process = edb::v1::debugger_core->process()) {

			std::shared_ptr<IThread> thread = process->currentThread();
			if (!thread) {
				return pass_back_to_debugger();
			}

			State state;
			thread->getState(&state);

			edb::address_t address               = state.instructionPointer();
			IDebugEvent::TRAP_REASON trap_reason = event->trapReason();
			IDebugEvent::REASON reason           = event->reason();

			qDebug() << QStringLiteral("Event at address 0x%1").arg(address, 0, 16);

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
				qDebug("Trap breakpoint");

				// Take care of exit/terminated conditions; address == 0 may suffice to catch all, but not 100% sure.
				if (reason == IDebugEvent::EVENT_EXITED || reason == IDebugEvent::EVENT_TERMINATED || address == 0) {
					qDebug("The process is no longer running.");
					return pass_back_to_debugger();
				}

				// Check the previous byte for 0xcc to see if it was an actual breakpoint
				std::shared_ptr<IBreakpoint> bp = edb::v1::find_triggered_breakpoint(address);

				// If there was a bp there, then we hit a block terminator as part of our RunUntilRet
				// algorithm, or it is a user-set breakpoint.
				if (bp && bp->enabled()) { // Isn't it always enabled if trap_reason is breakpoint, anyway?

					const edb::address_t prev_address = bp->address();

					bp->hit();

					// Adjust RIP since 1st byte was replaced with 0xcc and we are now 1 byte after it.
					state.setInstructionPointer(prev_address);
					thread->setState(state);
					address = prev_address;

					// If it wasn't internal, it was a user breakpoint. Pass back to Debugger.
					if (!bp->internal()) {
						qDebug("Previous was not an internal breakpoint.");
						return pass_back_to_debugger();
					}
					qDebug("Previous was an internal breakpoint.");
					bp->disable();
					edb::v1::debugger_core->removeBreakpoint(bp->address());
				} else {
					// No breakpoint if it was a syscall; continue.
					return edb::DEBUG_CONTINUE;
				}
			}

			// If we are on our ret (or the instr after?), then ret.
			if (address == returnAddress_) {
				qDebug() << "On our terminator at " << address.toHexString();
				if (is_instruction_ret(address)) {
					qDebug("Found ret; passing back to debugger");
					return pass_back_to_debugger();
				}

				// If not a ret, then step so we can find the next block terminator.
				qDebug("Not ret. Single-stepping");
				return edb::DEBUG_CONTINUE_STEP;
			}

			// If we stepped (either because it was the first event or because we hit a jmp/jcc),
			// then find the next block terminator and edb::DEBUG_CONTINUE.
			// TODO: What if we started on a ret? Set bp, then the proc runs away?
			uint8_t buffer[edb::Instruction::MaxSize];
			while (const int size = edb::v1::get_instruction_bytes(address, buffer)) {

				// Get the instruction
				edb::Instruction inst(buffer, buffer + size, 0);
				qDebug() << QStringLiteral("Scanning for terminator at 0x%1: found %2").arg(address, 0, 16).arg(QString::fromStdString(inst.mnemonic()));

				// Check if it's a proper block terminator (ret/jmp/jcc/hlt)
				if (inst) {
					if (is_terminator(inst)) {
						qDebug() << QStringLiteral("Found terminator %1 at 0x%2").arg(QString::fromStdString(inst.mnemonic())).arg(address, 0, 16);
						// If we already had a breakpoint there, then just continue.
						if (std::shared_ptr<IBreakpoint> bp = edb::v1::debugger_core->findBreakpoint(address)) {
							qDebug() << "Already a breakpoint at terminator: " << address.toHexString();
							return edb::DEBUG_CONTINUE;
						}

						// Otherwise, attempt to set a breakpoint there and continue.
						if (std::shared_ptr<IBreakpoint> bp = edb::v1::debugger_core->addBreakpoint(address)) {
							ownBreakpoints_.emplace_back(address, bp);
							qDebug() << "Setting breakpoint at terminator: " << address.toHexString();
							bp->setInternal(true);
							bp->setOneTime(true); // If the 0xcc get's rm'd on next event, then
												  // don't set it one time; we'll handle it manually
							returnAddress_ = address;
							return edb::DEBUG_CONTINUE;
						}

						QMessageBox::critical(edb::v1::debugger_ui,
											  tr("Error running until return"),
											  tr("Failed to set breakpoint on a block terminator at address %1.").arg(address.toPointerString()));
						return pass_back_to_debugger();
					}
				} else {
					// Invalid instruction or some other problem. Pass it back to the debugger.
					QMessageBox::critical(edb::v1::debugger_ui,
										  tr("Error running until return"),
										  tr("Failed to disassemble instruction at address %1.").arg(address.toPointerString()));
					return pass_back_to_debugger();
				}

				address += inst.byteSize();
			}

			// If we end up out here, we've got bigger problems. Pass it back to the debugger.
			QMessageBox::critical(edb::v1::debugger_ui,
								  tr("Error running until return"),
								  tr("Stepped outside the loop, address=%1.").arg(address.toPointerString()));
			return pass_back_to_debugger();
		}

		qDebug("The process is no longer running.");
		return pass_back_to_debugger();
	}

private:
	std::vector<std::pair<edb::address_t, std::weak_ptr<IBreakpoint>>> ownBreakpoints_;
	edb::address_t lastCallReturn_ = 0;
	edb::address_t returnAddress_  = 0;
};
}

/**
 * @brief Constructs a new Debugger instance.
 *
 * @param parent The parent widget.
 */
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
	connect(tabWidget_, &TabWidget::customContextMenuRequested, this, &Debugger::tabContextMenu);

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
			auto event = new QKeyEvent(QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier);
			QCoreApplication::postEvent(widget, event);
		}
	});

	// these get updated when we attach/run a new process, so it's OK to hard code them here
#if defined(EDB_X86_64)
	setRIPAction_  = createAction(tr("&Set %1 to this Instruction").arg(QLatin1String("RIP")), QKeySequence(tr("Ctrl+*")), &Debugger::mnuCPUSetEIP);
	gotoRIPAction_ = createAction(tr("&Goto %1").arg(QLatin1String("RIP")), QKeySequence(tr("*")), &Debugger::mnuCPUJumpToEIP);
#elif defined(EDB_X86)
	setRIPAction_  = createAction(tr("&Set %1 to this Instruction").arg(QLatin1String("EIP")), QKeySequence(tr("Ctrl+*")), &Debugger::mnuCPUSetEIP);
	gotoRIPAction_ = createAction(tr("&Goto %1").arg(QLatin1String("EIP")), QKeySequence(tr("*")), &Debugger::mnuCPUJumpToEIP);
#elif defined(EDB_ARM32) || defined(EDB_ARM64)
	setRIPAction_  = createAction(tr("&Set %1 to this Instruction").arg(QLatin1String("PC")), QKeySequence(tr("Ctrl+*")), &Debugger::mnuCPUSetEIP);
	gotoRIPAction_ = createAction(tr("&Goto %1").arg(QLatin1String("PC")), QKeySequence(tr("*")), &Debugger::mnuCPUJumpToEIP);
#else
#error "This doesn't initialize actions and will lead to crash"
#endif

	// Data Dump Shortcuts
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
	stackGotoRSPAction_ = createAction(tr("Goto %1").arg(QLatin1String("RSP")), QKeySequence(), &Debugger::mnuStackGotoESP);
	stackGotoRBPAction_ = createAction(tr("Goto %1").arg(QLatin1String("RBP")), QKeySequence(), &Debugger::mnuStackGotoEBP);
	stackPushAction_    = createAction(tr("&Push %1").arg(QLatin1String("QWORD")), QKeySequence(), &Debugger::mnuStackPush);
	stackPopAction_     = createAction(tr("P&op %1").arg(QLatin1String("QWORD")), QKeySequence(), &Debugger::mnuStackPop);
#elif defined(EDB_X86)
	stackGotoRSPAction_ = createAction(tr("Goto %1").arg(QLatin1String("ESP")), QKeySequence(), &Debugger::mnuStackGotoESP);
	stackGotoRBPAction_ = createAction(tr("Goto %1").arg(QLatin1String("EBP")), QKeySequence(), &Debugger::mnuStackGotoEBP);
	stackPushAction_    = createAction(tr("&Push %1").arg(QLatin1String("DWORD")), QKeySequence(), &Debugger::mnuStackPush);
	stackPopAction_     = createAction(tr("P&op %1").arg(QLatin1String("DWORD")), QKeySequence(), &Debugger::mnuStackPop);
#elif defined(EDB_ARM32)
	stackGotoRSPAction_ = createAction(tr("Goto %1").arg(QLatin1String("SP")), QKeySequence(), &Debugger::mnuStackGotoESP);
	stackGotoRBPAction_ = createAction(tr("Goto %1").arg(QLatin1String("FP")), QKeySequence(), &Debugger::mnuStackGotoEBP);
	stackGotoRBPAction_->setDisabled(true); // FIXME(ARM): this just stubs it out since it likely won't really work
	stackPushAction_ = createAction(tr("&Push %1").arg(QLatin1String("DWORD")), QKeySequence(), &Debugger::mnuStackPush);
	stackPushAction_->setDisabled(true); // FIXME(ARM): this just stubs it out since it likely won't really work
	stackPopAction_ = createAction(tr("P&op %1").arg(QLatin1String("DWORD")), QKeySequence(), &Debugger::mnuStackPop);
	stackPopAction_->setDisabled(true); // FIXME(ARM): this just stubs it out since it likely won't really work
#elif defined(EDB_ARM64)
	stackGotoRSPAction_ = createAction(tr("Goto %1").arg(QLatin1String("SP")), QKeySequence(), &Debugger::mnuStackGotoESP);
	stackGotoRSPAction_->setDisabled(true); // FIXME(ARM): this just stubs it out since it likely won't really work
	stackGotoRBPAction_ = createAction(tr("Goto %1").arg(QLatin1String("FP")), QKeySequence(), &Debugger::mnuStackGotoEBP);
	stackGotoRBPAction_->setDisabled(true); // FIXME(ARM): this just stubs it out since it likely won't really work
	stackPushAction_ = createAction(tr("&Push %1").arg(QLatin1String("QWORD")), QKeySequence(), &Debugger::mnuStackPush);
	stackPushAction_->setDisabled(true); // FIXME(ARM): this just stubs it out since it likely won't really work
	stackPopAction_ = createAction(tr("P&op %1").arg(QLatin1String("QWORD")), QKeySequence(), &Debugger::mnuStackPop);
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
	listView_->setModel(listModel_);

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

/**
 * @brief
 */
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

/**
 * @brief
 */
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

/**
 * @brief Creates a TTY for command line I/O.
 *
 * @return The path to the created TTY.
 */
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
			std::random_device rd;
			std::mt19937 mt(rd());
			const auto temp_pipe = QStringLiteral("%1/edb_temp_file_%2_%3").arg(QDir::tempPath()).arg(mt()).arg(getpid());

			// make sure it isn't already there, and then make the pipe
			::unlink(qPrintable(temp_pipe));
			::mkfifo(qPrintable(temp_pipe), S_IRUSR | S_IWUSR);

			// this is a basic shell script which will output the tty to a file (the pipe),
			// ignore kill sigs, close all standard IO, and then just hang
			const auto shell_script = QStringLiteral(
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

			if (command_info.fileName() == QLatin1String("gnome-terminal")) {
				// NOTE(eteran): gnome-terminal at some point dropped support for -e
				// in favor of using "everything after --"
				// See issue: https://github.com/eteran/edb-debugger/issues/774
				proc_args << QStringLiteral("--hide-menubar")
						  << QStringLiteral("--title") << tr("edb output")
						  << QStringLiteral("--");
			} else if (command_info.fileName() == QLatin1String("xfce4-terminal")) {
				proc_args << QStringLiteral("--hide-menubar")
						  << QStringLiteral("--title") << tr("edb output")
						  << QStringLiteral("--hold")
						  << QStringLiteral("-x");
			} else if (command_info.fileName() == QLatin1String("konsole")) {
				proc_args << QStringLiteral("--hide-menubar")
						  << QStringLiteral("--title") << tr("edb output")
						  << QStringLiteral("--nofork")
						  << QStringLiteral("--hold")
						  << QStringLiteral("-e");
			} else {
				proc_args << QStringLiteral("-title") << tr("edb output")
						  << QStringLiteral("-hold")
						  << QStringLiteral("-e");
			}

			proc_args << QStringLiteral("sh")
					  << QStringLiteral("-c") << shell_script;

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
						qDebug("An error occurred while attempting to get the TTY of the terminal sub-process");
						break;
					case 0:
						qDebug("A Timeout occurred while attempting to get the TTY of the terminal sub-process");
						break;
					default:
						if (read(fd, buf, sizeof(buf)) != -1) {
							result_tty = QString::fromLatin1(buf).trimmed();
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

/**
 * @brief Called when the TTY process finishes.
 *
 * @param exit_code The exit code of the process.
 * @param exit_status The exit status of the process.
 */
void Debugger::ttyProcFinished(int exit_code, QProcess::ExitStatus exit_status) {
	Q_UNUSED(exit_code)
	Q_UNUSED(exit_status)

	ttyFile_.clear();
}

/**
 * @brief Returns the index of the currently selected tab.
 *
 * @return The index of the current tab.
 */
int Debugger::currentTab() const {
	return tabWidget_->currentIndex();
}

/**
 * @brief Returns the data view info for the currently selected tab.
 *
 * @return A shared pointer to the data view info.
 */
std::shared_ptr<DataViewInfo> Debugger::currentDataViewInfo() const {
	return dataRegions_[currentTab()];
}

/**
 * @brief Sets the caption of the debugger window.
 *
 * @param appname The name of the application.
 */
void Debugger::setDebuggerCaption(const QString &appname) {
	if (IProcess *process = edb::v1::debugger_core->process()) {
		setWindowTitle(tr("edb - %1 [%2]").arg(appname).arg(process->pid()));
	} else {
		setWindowTitle(tr("edb"));
	}
}

/**
 * @brief Deletes the currently selected data tab.
 */
void Debugger::deleteDataTab() {
	const int current = currentTab();

	// get a pointer to the info we need (before removing it from the list!)
	// this seems redundant to current_data_view_info(), but we need the
	// index too, so may as well waste one line to avoid duplicate work
	std::shared_ptr<DataViewInfo> info = dataRegions_[current];

	// remove it from the list
	dataRegions_.remove(current);

	// remove the tab associated with it
	tabWidget_->removeTab(current);
}

/**
 * @brief Creates a new data tab for the debugger.
 */
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

	// QColor coldZoneColor_         = Qt::gray;
	// QColor lineColor_             = Qt::black;

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
		tabWidget_->addTab(hexview.get(), tr("%1-%2").arg(
											  edb::v1::format_pointer(new_data_view->region->start()),
											  edb::v1::format_pointer(new_data_view->region->end())));
	} else {
		tabWidget_->addTab(hexview.get(), tr("%1-%2").arg(
											  edb::v1::format_pointer(edb::address_t(0)),
											  edb::v1::format_pointer(edb::address_t(0))));
	}

	tabWidget_->setCurrentIndex(tabWidget_->count() - 1);
}

/**
 * @brief Finalizes the plugin setup by adding each plugin to the menu and setting up its context menu.
 */
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

/**
 * @brief Gets the address expression from the user.
 *
 * @return A result containing the address or an error message.
 */
Result<edb::address_t, QString> Debugger::getGotoExpression() {

	std::optional<edb::address_t> address = edb::v2::get_expression_from_user(tr("Goto Expression"), tr("Expression:"));
	if (address) {
		return *address;
	}

	return make_unexpected(tr("No Address"));
}

/**
 * @brief Gets the register value to follow.
 *
 * @return A result containing the register value or an error message.
 */
Result<edb::reg_t, QString> Debugger::getFollowRegister() const {

	const Register reg = activeRegister();
	if (!reg || reg.bitSize() > 8 * sizeof(edb::address_t)) {
		return make_unexpected(tr("No Value"));
	}

	return reg.valueAsAddress();
}

/**
 * @brief Triggers the goto action based on the currently focused widget.
 */
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

/**
 * @brief Sets up the user interface for the debugger.
 */
void Debugger::setupUi() {
	// setup the global pointers as early as possible.
	// NOTE:  this should never be changed after this point
	// NOTE:  this is important that this happens BEFORE any components which
	// read settings as it could end up being a memory leak (and therefore never
	// calling it's destructor which writes the settings to disk!).
	edb::v1::debugger_ui = this;

	ui.setupUi(this);

	splitter_ = new QSplitter(this);
	splitter_->setObjectName(QLatin1String("mainSplitter"));
	splitter_->setOrientation(Qt::Vertical);

	{
		logger_ = new QPlainTextEdit(this);
		logger_->setObjectName(QLatin1String("logView"));
		logger_->setReadOnly(true);
		QFont font(QStringLiteral("monospace"));
		font.setStyleHint(QFont::TypeWriter);
		logger_->setFont(font);
		logger_->setWordWrapMode(QTextOption::WrapMode::NoWrap);
		logger_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		logger_instance = logger_;

		qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &, const QString &message) {
			const QString text = [type, &message]() {
				switch (type) {
				case QtDebugMsg:
					return tr("DEBUG %1").arg(message);
				case QtInfoMsg:
					return tr("INFO  %1").arg(message);
				case QtWarningMsg:
					return tr("WARN  %1").arg(message);
				case QtCriticalMsg:
					return tr("ERROR %1").arg(message);
				case QtFatalMsg:
					return tr("FATAL %1").arg(message);
				default:
					Q_UNREACHABLE();
				}
			}();

			logger_instance->appendPlainText(text);
			std::cerr << message.toUtf8().constData() << "\n"; // this may be useful as a crash log
		});

		auto toolButton = static_cast<QToolButton *>(ui.toolBar->widgetForAction(ui.action_Debug_Logger));

		toolButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		connect(ui.action_Debug_Logger, &QAction::triggered, this, [this](bool checked) {
			logger_->setVisible(checked);
		});
	}

	mainWindow_ = new QMainWindow(this);
	mainWindow_->setObjectName(QLatin1String("subMainWindow"));
	mainWindow_->setWindowFlags(Qt::Widget);
	mainWindow_->layout()->setContentsMargins(3, 3, 3, 3);
	mainWindow_->setObjectName(QLatin1String("dockingRoot"));

	cpuView_ = new QDisassemblyView;
	cpuView_->setObjectName(QLatin1String("cpuView"));

	listView_ = new QListView;
	listView_->setObjectName(QLatin1String("listView"));

	QFont font;
	font.setFamily(QStringLiteral("Monospace"));
	font.setPointSize(10);
	listView_->setFont(font);
	listView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
	listView_->setFixedHeight(listView_->fontMetrics().height() * 4);

	auto cpu_splitter = new QSplitter(this);
	cpu_splitter->setObjectName(QLatin1String("cpuSplitter"));
	cpu_splitter->setOrientation(Qt::Vertical);

	cpu_splitter->addWidget(cpuView_);
	cpu_splitter->addWidget(listView_);
	mainWindow_->setCentralWidget(cpu_splitter);

	splitter_->addWidget(mainWindow_);
	splitter_->addWidget(logger_);
	splitter_->setChildrenCollapsible(false);

	// data dock
	dataDock_ = new QDockWidget(this);
	dataDock_->setObjectName(QLatin1String("dataDock"));
	tabWidget_ = new TabWidget(this);
	tabWidget_->setObjectName(QLatin1String("tabWidget"));

	dataDock_->setWidget(tabWidget_);
	mainWindow_->addDockWidget(Qt::BottomDockWidgetArea, dataDock_);
	dataDock_->setWindowTitle(tr("Data Dump"));

	// stack dock
	stackDock_ = new QDockWidget(this);
	stackDock_->setObjectName(QLatin1String("stackDock"));
	mainWindow_->addDockWidget(Qt::BottomDockWidgetArea, stackDock_);
	stackDock_->setWindowTitle(tr("Stack"));

	setCentralWidget(splitter_);

	status_ = new QLabel(this);
	status_->setObjectName(QLatin1String("statusLabel"));
	ui.statusbar->insertPermanentWidget(0, status_);

	// add toggles for the dock windows
	ui.menu_View->addAction(dataDock_->toggleViewAction());
	ui.menu_View->addAction(stackDock_->toggleViewAction());
	ui.menu_View->addAction(ui.toolBar->toggleViewAction());

	ui.action_Restart->setEnabled(recentFileManager_->entryCount() > 0);

	// make sure our widgets use custom context menus
	cpuView_->setContextMenuPolicy(Qt::CustomContextMenu);

	setupStackView();
	setupTabButtons();

	mnuDumpCreateTab();

	// apply any fonts we may have stored
	applyDefaultFonts();

	// apply the default setting for showing address separators
	applyDefaultShowSeparator();

	connect(cpuView_, &QDisassemblyView::breakPointToggled, this, &Debugger::breakPointToggled_triggered);
	connect(cpuView_, &QDisassemblyView::customContextMenuRequested, this, &Debugger::customContextMenuRequested_triggered);
}

/**
 * @brief
 */
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

	stackDock_->setWidget(stackView_.get());

	// setup the context menu
	stackView_->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(stackView_.get(), &QHexView::customContextMenuRequested, this, &Debugger::mnuStackContextMenu);

	// we placed a view in the designer, so just set it here
	// this may get transitioned to heap allocated, we'll see
	stackViewInfo_.view = stackView_;

	// setup the comment server for the stack viewer
	stackView_->setCommentServer(commentServer_.get());
}

/**
 * @brief Handles the close event for the main window.
 *
 * @param event The close event.
 */
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

	if (!ui_reset_) {
		QSettings settings;
		const QByteArray state = saveState();
		settings.beginGroup(QStringLiteral("Window"));
		settings.setValue(QStringLiteral("window.logger.visible"), logger_->isVisible());
		settings.setValue(QStringLiteral("window.state"), state);
		settings.setValue(QStringLiteral("window.view.state"), mainWindow_->saveState());
		settings.setValue(QStringLiteral("window.width"), width());
		settings.setValue(QStringLiteral("window.height"), height());
		settings.setValue(QStringLiteral("window.x"), x());
		settings.setValue(QStringLiteral("window.y"), y());
		settings.setValue(QStringLiteral("window.stack.show_address.enabled"), stackView_->showAddress());
		settings.setValue(QStringLiteral("window.stack.show_hex.enabled"), stackView_->showHexDump());
		settings.setValue(QStringLiteral("window.stack.show_ascii.enabled"), stackView_->showAsciiDump());
		settings.setValue(QStringLiteral("window.stack.show_comments.enabled"), stackView_->showComments());

		QByteArray disassemblyState = cpuView_->saveState();
		settings.setValue(QStringLiteral("window.disassembly.state"), disassemblyState);

		QByteArray splitterState = splitter_->saveState();
		settings.setValue(QStringLiteral("window.splitter.state"), splitterState);

		settings.endGroup();
	}

	event->accept();
}

/**
 * @brief Handles the show event for the main window.
 *
 * @param event The show event.
 */
void Debugger::showEvent(QShowEvent *) {

	QSettings settings;
	settings.beginGroup(QStringLiteral("Window"));
	const QByteArray splitterState = settings.value(QStringLiteral("window.splitter.state")).toByteArray();
	const QByteArray viewState     = settings.value(QStringLiteral("window.view.state")).toByteArray();
	const QByteArray state         = settings.value(QStringLiteral("window.state")).toByteArray();
	const int width                = settings.value(QStringLiteral("window.width"), -1).toInt();
	const int height               = settings.value(QStringLiteral("window.height"), -1).toInt();
	const int x                    = settings.value(QStringLiteral("window.x"), -1).toInt();
	const int y                    = settings.value(QStringLiteral("window.y"), -1).toInt();

	if (width != -1) {
		resize(width, size().height());
	}

	if (height != -1) {
		resize(size().width(), height);
	}

	const bool loggerVisible = settings.value(QStringLiteral("window.logger.visible")).toBool();

	const Configuration &config = edb::v1::config();
	switch (config.startup_window_location) {
	case Configuration::SystemDefault:
		break;
	case Configuration::Centered: {
		QScreen *screen = QGuiApplication::primaryScreen();
		QRect sg        = screen->geometry();
		int x           = (sg.width() - this->width()) / 2;
		int y           = (sg.height() - this->height()) / 2;
		move(x, y);
	} break;
	case Configuration::Restore:
		if (x != -1 && y != -1) {
			move(x, y);
		}
		break;
	}

	stackView_->setShowAddress(settings.value(QStringLiteral("window.stack.show_address.enabled"), true).toBool());
	stackView_->setShowHexDump(settings.value(QStringLiteral("window.stack.show_hex.enabled"), true).toBool());
	stackView_->setShowAsciiDump(settings.value(QStringLiteral("window.stack.show_ascii.enabled"), true).toBool());
	stackView_->setShowComments(settings.value(QStringLiteral("window.stack.show_comments.enabled"), true).toBool());

	int row_width   = 1;
	auto word_width = static_cast<int>(edb::v1::pointer_size());

	stackView_->setRowWidth(row_width);
	stackView_->setWordWidth(word_width);

	QByteArray disassemblyState = settings.value(QStringLiteral("window.disassembly.state")).toByteArray();
	cpuView_->restoreState(disassemblyState);

	settings.endGroup();
	restoreState(state);
	mainWindow_->restoreState(viewState);
	splitter_->restoreState(splitterState);

	logger_->setVisible(loggerVisible);
	ui.action_Debug_Logger->setChecked(loggerVisible);
}

/**
 * @brief Handles the drag enter event for the main window.
 *
 * @param event The drag enter event.
 */
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

/**
 * @brief Handles the drop event for the main window. If a file is dropped, it will be opened.
 *
 * @param event The drop event.
 */
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

/**
 * @brief Handles the about Qt action. Displays an About Qt dialog box.
 */
void Debugger::on_actionAbout_QT_triggered() {
	QMessageBox::aboutQt(this, tr("About Qt"));
}

/**
 * @brief Applies the default fonts to the necessary widgets.
 */
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
		cpuView_->setFont(font);
	}

	if (font.fromString(config.data_font)) {
		for (const std::shared_ptr<DataViewInfo> &data_view : dataRegions_) {
			data_view->view->setFont(font);
		}
	}
}

/**
 * @brief Creates the add/remove tab buttons in the data view
 */
void Debugger::setupTabButtons() {
	// add the corner widgets to the data view
	tabCreate_ = new QToolButton(tabWidget_);
	tabDelete_ = new QToolButton(tabWidget_);

	tabCreate_->setToolButtonStyle(Qt::ToolButtonIconOnly);
	tabDelete_->setToolButtonStyle(Qt::ToolButtonIconOnly);
	tabCreate_->setIcon(QIcon::fromTheme(QStringLiteral("tab-new")));
	tabDelete_->setIcon(QIcon::fromTheme(QStringLiteral("tab-close")));
	tabCreate_->setAutoRaise(true);
	tabDelete_->setAutoRaise(true);
	tabCreate_->setEnabled(false);
	tabDelete_->setEnabled(false);

	tabWidget_->setCornerWidget(tabCreate_, Qt::TopLeftCorner);
	tabWidget_->setCornerWidget(tabDelete_, Qt::TopRightCorner);

	connect(tabCreate_, &QToolButton::clicked, this, &Debugger::mnuDumpCreateTab);
	connect(tabDelete_, &QToolButton::clicked, this, &Debugger::mnuDumpDeleteTab);
}

/**
 * @brief Returns the active register based on the current selection in the register view.
 *
 * @return The active register, or an empty Register if no valid register is selected.
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
			if (std::shared_ptr<IThread> thread = process->currentThread()) {
				State state;
				thread->getState(&state);
				return state[regName];
			}
		}
	}
	return {};
}

/**
 * @brief Returns the context menu items for the active register.
 *
 * @return A list of QAction pointers representing the context menu items.
 */
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

/**
 * @brief Toggles the flag register at the specified bit position.
 *
 * @param pos The position of the flag bit to toggle.
 */
void Debugger::toggleFlag(int pos) {
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

/**
 * @brief Handles the toggle breakpoint event.
 *
 * @param address The address of the breakpoint to toggle.
 */
void Debugger::breakPointToggled_triggered(edb::address_t address) {
	edb::v1::toggle_breakpoint(address);
}

/**
 * @brief Handles the about action. Displays an About dialog box.
 */
void Debugger::on_action_About_triggered() {

	QPointer<DialogAbout> dlg = new DialogAbout(this);
	dlg->exec();
	delete dlg;
}

/**
 * @brief Applies the default show separator setting to the necessary widgets.
 */
void Debugger::applyDefaultShowSeparator() {
	const bool show = edb::v1::config().show_address_separator;

	cpuView_->setShowAddressSeparator(show);
	stackView_->setShowAddressSeparator(show);
	for (const std::shared_ptr<DataViewInfo> &data_view : dataRegions_) {
		data_view->view->setShowAddressSeparator(show);
	}
}

/**
 * @brief Handles the configure debugger action. Applies the configuration changes.
 */
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

/**
 * @brief Steps over the current instruction.
 *
 * @param run_func The function to run the process.
 * @param step_func The function to step into the process.
 */
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

/**
 * @brief Follows the memory at the specified address.
 *
 * @param address The address of the memory to follow.
 * @param follow_func The function to follow the memory.
 */
template <class F>
void Debugger::followMemory(edb::address_t address, F follow_func) {
	if (!follow_func(address)) {
		QMessageBox::critical(this,
							  tr("No Memory Found"),
							  tr("There appears to be no memory at that location (<strong>%1</strong>)").arg(edb::v1::format_pointer(address)));
	}
}

/**
 * @brief Follows the register in the dump view.
 *
 * @param tabbed Whether to open the dump in a new tab.
 */
void Debugger::followRegisterInDump(bool tabbed) {

	if (const Result<edb::address_t, QString> address = getFollowRegister()) {
		if (!edb::v1::dump_data(*address, tabbed)) {
			QMessageBox::critical(this,
								  tr("No Memory Found"),
								  tr("There appears to be no memory at that location (<strong>%1</strong>)").arg(edb::v1::format_pointer(address.value())));
		}
	}
}

/**
 * @brief Jumps to the ESP register in the stack view.
 */
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

/**
 * @brief Jumps to the EBP register in the stack view.
 */
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

/**
 * @brief Jumps to the EIP register in the CPU view.
 */
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

/**
 * @brief Jumps to the specified address in the CPU view.
 */
void Debugger::mnuCPUJumpToAddress() {

	if (const Result<edb::address_t, QString> address = getGotoExpression()) {
		followMemory(*address, [](edb::address_t address) {
			return edb::v1::jump_to_address(address);
		});
	}
}

/**
 * @brief Jumps to the specified address in the dump view.
 */
void Debugger::mnuDumpGotoAddress() {
	if (const Result<edb::address_t, QString> address = getGotoExpression()) {
		followMemory(*address, [](edb::address_t address) {
			return edb::v1::dump_data(address);
		});
	}
}

/**
 * @brief Jumps to the specified address in the stack view.
 */
void Debugger::mnuStackGotoAddress() {
	if (const Result<edb::address_t, QString> address = getGotoExpression()) {
		followMemory(*address, [](edb::address_t address) {
			return edb::v1::dump_stack(address);
		});
	}
}

/**
 * @brief Follows the register in the stack view.
 */
void Debugger::mnuRegisterFollowInStack() {

	if (const Result<edb::address_t, QString> address = getFollowRegister()) {
		followMemory(*address, [](edb::address_t address) {
			return edb::v1::dump_stack(address);
		});
	}
}

/**
 * @brief Gets the address to follow from the selected text in the hex view.
 *
 * @param hexview The hex view to get the address from.
 * @return The address to follow or an error message.
 */
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

/**
 * @brief Follows the address in the stack view.
 *
 * @param hexview The hex view to get the address from.
 */
template <class Ptr>
void Debugger::followInStack(const Ptr &hexview) {

	if (const Result<edb::address_t, QString> address = getFollowAddress(hexview)) {
		followMemory(*address, [](edb::address_t address) {
			return edb::v1::dump_stack(address);
		});
	}
}

/**
 * @brief Follows the address in the dump view.
 *
 * @param hexview The hex view to get the address from.
 */
template <class Ptr>
void Debugger::followInDump(const Ptr &hexview) {

	if (const Result<edb::address_t, QString> address = getFollowAddress(hexview)) {
		followMemory(*address, [](edb::address_t address) {
			return edb::v1::dump_data(address);
		});
	}
}

/**
 * @brief Follows the address in the CPU view.
 *
 * @param hexview The hex view to get the address from.
 */
template <class Ptr>
void Debugger::followInCpu(const Ptr &hexview) {

	if (const Result<edb::address_t, QString> address = getFollowAddress(hexview)) {
		followMemory(*address, [](edb::address_t address) {
			return edb::v1::jump_to_address(address);
		});
	}
}

/**
 * @brief Follows the address in the CPU view.
 */
void Debugger::mnuDumpFollowInCPU() {
	followInCpu(qobject_cast<QHexView *>(tabWidget_->currentWidget()));
}

/**
 * @brief Follows the address in the dump view.
 */
void Debugger::mnuDumpFollowInDump() {
	followInDump(qobject_cast<QHexView *>(tabWidget_->currentWidget()));
}

/**
 * @brief Follows the address in the stack view.
 */
void Debugger::mnuDumpFollowInStack() {
	followInStack(qobject_cast<QHexView *>(tabWidget_->currentWidget()));
}

/**
 * @brief Follows the address in the dump view.
 */
void Debugger::mnuStackFollowInDump() {
	followInDump(stackView_);
}

/**
 * @brief
 */
void Debugger::mnuStackFollowInCPU() {
	followInCpu(stackView_);
}

/**
 * @brief
 */
void Debugger::mnuStackFollowInStack() {
	followInStack(stackView_);
}

/**
 * @brief
 */
void Debugger::on_actionApplication_Arguments_triggered() {
	argumentsDialog_->exec();
}

/**
 * @brief
 */
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

/**
 * @brief
 */
void Debugger::mnuStackPush() {
	Register value(edb::v1::debuggeeIs32Bit()
					   ? make_Register(QString(), edb::value32(0), Register::TYPE_GPR)
					   : make_Register(QString(), edb::value64(0), Register::TYPE_GPR));

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

/**
 * @brief
 */
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

/**
 * @brief
 */
void Debugger::customContextMenuRequested_triggered(const QPoint &pos) {
	QMenu menu;

	auto displayMenu = new QMenu(tr("Display"), &menu);
	displayMenu->addAction(QIcon::fromTheme(QString::fromUtf8("view-restore")), tr("Restore Column Defaults"), cpuView_, SLOT(resetColumns()));

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

	const edb::address_t address = cpuView_->selectedAddress();
	int size                     = cpuView_->selectedSize();

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

	menu.exec(cpuView_->viewport()->mapToGlobal(pos));
}

/**
 * @brief
 */
void Debugger::mnuCPUFollow() {

	const edb::address_t address = cpuView_->selectedAddress();
	int size                     = cpuView_->selectedSize();
	uint8_t buffer[edb::Instruction::MaxSize + 1];
	if (!edb::v1::get_instruction_bytes(address, buffer, &size)) {
		return;
	}

	const edb::Instruction inst(buffer, buffer + size, address);
	if (!is_call(inst) && !is_jump(inst)) {
		return;
	}
	if (!is_immediate(inst[0])) {
		return;
	}

	const edb::address_t addressToFollow = util::to_unsigned(inst[0]->imm);
	if (auto action = qobject_cast<QAction *>(sender())) {
		Q_UNUSED(action)
		followMemory(addressToFollow, edb::v1::jump_to_address);
	}
}

/**
 * @brief
 */
void Debugger::mnuCPUFollowInDump() {
	if (auto action = qobject_cast<QAction *>(sender())) {
		const edb::address_t address = action->data().toULongLong();
		followMemory(address, [](edb::address_t address) {
			return edb::v1::dump_data(address);
		});
	}
}

/**
 * @brief
 */
void Debugger::mnuCPUFollowInStack() {
	if (auto action = qobject_cast<QAction *>(sender())) {
		const edb::address_t address = action->data().toULongLong();
		followMemory(address, [](edb::address_t address) {
			return edb::v1::dump_stack(address);
		});
	}
}

/**
 * @brief
 */
void Debugger::mnuStackToggleLock(bool locked) {
	stackViewLocked_ = locked;
}

/**
 * @brief
 */
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

/**
 * @brief
 */
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

/**
 * @brief
 */
void Debugger::mnuDumpSaveToFile() {
	auto s = qobject_cast<QHexView *>(tabWidget_->currentWidget());

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

/**
 * @brief Fills the selected area in the CPU view with the specified byte.
 *
 * @param byte The byte to fill with.
 */
void Debugger::cpuFill(uint8_t byte) {
	const edb::address_t address = cpuView_->selectedAddress();
	const unsigned int size      = cpuView_->selectedSize();

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

/**
 * @brief Adds or edits a comment at the selected address in the CPU view.
 */
void Debugger::mnuCPUEditComment() {
	const edb::address_t address = cpuView_->selectedAddress();

	bool got_text;
	const QString comment = QInputDialog::getText(
		this,
		tr("Edit Comment"),
		tr("Comment:"),
		QLineEdit::Normal,
		cpuView_->getComment(address),
		&got_text);

	// If we got a comment, add it.
	if (got_text && !comment.isEmpty()) {
		cpuView_->addComment(address, comment);
	} else if (got_text && comment.isEmpty()) {
		// If the user backspaced the comment, remove the comment since
		// there's no need for a null string to take space in the hash.
		cpuView_->removeComment(address);
	} else {
		// The only other real case is that we didn't got_text.  No change.
		return;
	}

	refreshUi();
}

/**
 * @brief Removes a comment at the selected address in the CPU view.
 *
 * @param address The address of the comment to remove.
 */
void Debugger::mnuCPURemoveComment() {
	const edb::address_t address = cpuView_->selectedAddress();
	cpuView_->removeComment(address);
	refreshUi();
}

/**
 * @brief Runs the debugger to the selected line in the CPU view.
 *
 * @param pass_signal Whether to pass the exception or ignore it.
 */
void Debugger::runToThisLine(ExceptionResume pass_signal) {
	const edb::address_t address    = cpuView_->selectedAddress();
	std::shared_ptr<IBreakpoint> bp = edb::v1::find_breakpoint(address);
	if (!bp) {
		bp = edb::v1::create_breakpoint(address);
		if (!bp) {
			return;
		}
		bp->setOneTime(true);
		bp->setInternal(true);
		bp->tag = run_to_cursor_tag;
	}

	resumeExecution(pass_signal, Run, ResumeFlag::None);
}

/**
 * @brief Runs the debugger to the selected line in the CPU view, passing the exception.
 */
void Debugger::mnuCPURunToThisLinePassSignal() {
	runToThisLine(PassException);
}

/**
 * @brief Runs the debugger to the selected line in the CPU view, ignoring the exception.
 */
void Debugger::mnuCPURunToThisLine() {
	runToThisLine(IgnoreException);
}

/**
 * @brief Toggles a breakpoint at the selected address in the CPU view.
 */
void Debugger::mnuCPUToggleBreakpoint() const {
	const edb::address_t address = cpuView_->selectedAddress();
	edb::v1::toggle_breakpoint(address);
}

/**
 * @brief Adds a conditional breakpoint at the selected address in the CPU view.
 */
void Debugger::mnuCPUAddConditionalBreakpoint() {
	bool ok;
	const edb::address_t address = cpuView_->selectedAddress();

	const QString condition = QInputDialog::getText(this, tr("Set Breakpoint Condition"), tr("Expression:"), QLineEdit::Normal, QString(), &ok);
	if (ok) {
		if (std::shared_ptr<IBreakpoint> bp = edb::v1::create_breakpoint(address)) {
			if (!condition.isEmpty()) {
				bp->condition = condition;
			}
		}
	}
}

/**
 * @brief Removes a breakpoint at the selected address in the CPU view.
 *
 * @param address The address of the breakpoint to remove.
 */
void Debugger::mnuCPURemoveBreakpoint() const {
	const edb::address_t address = cpuView_->selectedAddress();
	edb::v1::remove_breakpoint(address);
}

/**
 * @brief Fills the selected bytes in the CPU view with zeros.
 */
void Debugger::mnuCPUFillZero() {
	cpuFill(0x00);
}

/**
 * @brief Fills the selected bytes in the CPU view with NOP instructions.
 */
void Debugger::mnuCPUFillNop() {
	if (IDebugger *core = edb::v1::debugger_core) {
		cpuFill(core->nopFillByte());
	}
}

/**
 * @brief Labels the selected address in the CPU view.
 */
void Debugger::mnuCPULabelAddress() {

	const edb::address_t address = cpuView_->selectedAddress();

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

/**
 * @brief Sets the EIP (Instruction Pointer) to the selected address in the CPU view.
 */
void Debugger::mnuCPUSetEIP() {
	if (IProcess *process = edb::v1::debugger_core->process()) {
		if (std::shared_ptr<IThread> thread = process->currentThread()) {
			const edb::address_t address = cpuView_->selectedAddress();
			State state;
			thread->getState(&state);
			state.setInstructionPointer(address);
			thread->setState(state);
			updateUi();
		}
	}
}

/**
 * @brief Modifies the selected bytes in the CPU view.
 */
void Debugger::mnuCPUModify() const {
	const edb::address_t address = cpuView_->selectedAddress();
	const unsigned int size      = cpuView_->selectedSize();

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

/**
 * @brief Modifies the selected bytes in the given hex view.
 *
 * @param hexview The hex view to modify.
 */
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

/**
 * @brief Modifies the selected bytes in the current view.
 */
void Debugger::mnuModifyBytes() {

	QWidget *const focusedWidget = QApplication::focusWidget();
	if (focusedWidget == cpuView_) {
		mnuCPUModify();
	} else if (focusedWidget == stackView_.get()) {
		mnuStackModify();
	} else {
		// not CPU or Stack, assume one of the data views..
		mnuDumpModify();
	}
}

/**
 * @brief Modifies the selected bytes in the dump view.
 */
void Debugger::mnuDumpModify() {
	modifyBytes(qobject_cast<QHexView *>(tabWidget_->currentWidget()));
}

/**
 * @brief Modifies the selected bytes in the stack view.
 */
void Debugger::mnuStackModify() {
	modifyBytes(stackView_);
}

/**
 * @brief Checks if the breakpoint condition is true.
 *
 * @param condition The condition to evaluate.
 * @return True if the condition is true, false otherwise.
 */
bool Debugger::isBreakpointConditionTrue(const QString &condition) {

	if (std::optional<edb::address_t> condition_value = edb::v2::eval_expression(condition)) {
		return *condition_value;
	}
	return true;
}

/**
 * @brief Handles a trap event.
 *
 * @param event The trap event to handle.
 * @return True if we should resume as if this trap never happened
 */
edb::EventStatus Debugger::handleTrap(const std::shared_ptr<IDebugEvent> &event) {

	// we just got a trap event, there are a few possible causes
	// #1: we hit a 0xcc breakpoint, if so, then we want to stop
	// #2: we did a step
	// #3: we hit a 0xcc naturally in the program
	// #4: we hit a hardware breakpoint!
	IProcess *process = edb::v1::debugger_core->process();
	Q_ASSERT(process);

	std::shared_ptr<IThread> thread = process->currentThread();
	Q_ASSERT(thread);

	State state;
	thread->getState(&state);

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
		thread->setState(state);

#if defined(Q_OS_LINUX)
		// test if we have hit our internal LD hook BP. If so, read in the r_debug
		// struct (if necessary) so we can get the state, then we can just resume
		if (bp->internal() && bp->tag == ld_loader_tag) {
			if (!debugPointer_) {
				debugPointer_ = process->debugPointer();
			}

			if (debugPointer_) {
				if (edb::v1::debuggeeIs32Bit()) {
					handle_library_event<uint32_t>(process, debugPointer_);
				} else {
					handle_library_event<uint64_t>(process, debugPointer_);
				}
			}

			if (edb::v1::config().break_on_library_load) {
				return edb::DEBUG_STOP;
			}

			return edb::DEBUG_CONTINUE_BP;
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

/**
 * @brief Handles a stopped event.
 *
 * @param event The stopped event to handle.
 * @return The status of the event.
 */
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
		if (edb::v1::config().enable_signals_message_box) {
			QMessageBox::information(this, message.caption, message.message);
		}
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

/**
 * @brief Handles a terminated event.
 *
 * @param event The terminated event to handle.
 * @return The status of the event.
 */
edb::EventStatus Debugger::handleEventTerminated(const std::shared_ptr<IDebugEvent> &event) {
	on_action_Detach_triggered();
	QMessageBox::information(
		this,
		tr("Application Terminated"),
		tr("The debugged application was terminated with exit code %1.").arg(event->code()));

	return edb::DEBUG_STOP;
}

/**
 * @brief Handles a exited event.
 *
 * @param event The exited event to handle.
 * @return The status of the event.
 */
edb::EventStatus Debugger::handleEventExited(const std::shared_ptr<IDebugEvent> &event) {
	on_action_Detach_triggered();
	QMessageBox::information(
		this,
		tr("Application Exited"),
		tr("The debugged application exited normally with exit code %1.").arg(event->code()));

	return edb::DEBUG_STOP;
}

/**
 * @brief Handles a debug event.
 *
 * @param event The debug event to handle.
 * @return The status of the event.
 */
edb::EventStatus Debugger::handleEvent(const std::shared_ptr<IDebugEvent> &event) {

	Q_ASSERT(edb::v1::debugger_core);

	edb::EventStatus status;
	switch (event->reason()) {
	// either a synchonous event (STOPPED)
	// or an asynchronous event (SIGNALED)
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

/**
 * @brief Updates the caption of a tab.
 *
 * @param view The hex view to update.
 * @param start The start address of the region.
 * @param end The end address of the region.
 */
void Debugger::updateTabCaption(const std::shared_ptr<QHexView> &view, edb::address_t start, edb::address_t end) const {
	const int index       = tabWidget_->indexOf(view.get());
	const QString caption = tabWidget_->data(index).toString();

	if (caption.isEmpty()) {
		tabWidget_->setTabText(index, tr("%1-%2").arg(edb::v1::format_pointer(start), edb::v1::format_pointer(end)));
	} else {
		tabWidget_->setTabText(index, tr("[%1] %2-%3").arg(caption, edb::v1::format_pointer(start), edb::v1::format_pointer(end)));
	}
}

/**
 * @brief Updates the data in a view.
 *
 * @param v The data view info to update.
 */
void Debugger::updateData(const std::shared_ptr<DataViewInfo> &v) {

	Q_ASSERT(v);

	const std::shared_ptr<QHexView> &view = v->view;

	Q_ASSERT(view);

	v->update();

	updateTabCaption(view, v->region->start(), v->region->end());
}

/**
 * @brief Clears the data in a view.
 *
 * @param v The data view info to clear.
 */
void Debugger::clearData(const std::shared_ptr<DataViewInfo> &v) {

	Q_ASSERT(v);

	const std::shared_ptr<QHexView> &view = v->view;

	Q_ASSERT(view);

	view->clear();
	view->scrollTo(0);

	updateTabCaption(view, 0, 0);
}

/**
 * @brief Jumps to a specific address in the CPU view.
 *
 * @param address The address to jump to.
 * @param r The region to set.
 * @param scroll_to Whether to scroll to the address.
 */
void Debugger::doJumpToAddress(edb::address_t address, const std::shared_ptr<IRegion> &r, bool scroll_to) const {

	cpuView_->setRegion(r);
	if (scroll_to && !cpuView_->addressShown(address)) {
		cpuView_->scrollTo(address);
	}
	cpuView_->setSelectedAddress(address);
}

/**
 * @brief Updates the disassembly view.
 *
 * @param address The address to update.
 * @param r The region to use.
 */
void Debugger::updateDisassembly(edb::address_t address, const std::shared_ptr<IRegion> &r) {
	cpuView_->setCurrentAddress(address);
	doJumpToAddress(address, r, true);
	listModel_->setStringList(edb::v1::arch_processor().updateInstructionInfo(address));
}

/**
 * @brief Updates the stack view.
 *
 * @param state The state to use for the update.
 */
void Debugger::updateStackView(const State &state) {
	if (!edb::v1::dump_stack(state.stackPointer(), !stackViewLocked_)) {
		stackView_->clear();
		stackView_->scrollTo(0);
	}
}

/**
 * @brief Updates the CPU view.
 *
 * @param state The state to use for the update.
 * @return The updated region or nullptr if not found.
 */
std::shared_ptr<IRegion> Debugger::updateCpuView(const State &state) {
	const edb::address_t address = state.instructionPointer();

	if (std::shared_ptr<IRegion> region = edb::v1::memory_regions().findRegion(address)) {
		updateDisassembly(address, region);
		return region;
	}

	cpuView_->clear();
	cpuView_->scrollTo(0);
	listModel_->setStringList(QStringList());
	return nullptr;
}

/**
 * @brief Updates all data views.
 */
void Debugger::updateDataViews() {

	// update all data views with the current region data
	for (const std::shared_ptr<DataViewInfo> &info : dataRegions_) {

		// make sure the regions are still valid..
		if (info->region && edb::v1::memory_regions().findRegion(info->region->start())) {
			updateData(info);
		} else {
			clearData(info);
		}
	}
}

/**
 * @brief Refreshes the UI.
 */
void Debugger::refreshUi() {

	cpuView_->update();
	stackView_->update();

	for (const std::shared_ptr<DataViewInfo> &info : dataRegions_) {
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

/**
 * @brief Updates the UI.
 */
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

	// Signal all connected slots that the GUI has been updated.
	// Useful for plugins with windows that should updated after
	// hitting breakpoints, Step Over, etc.
	Q_EMIT uiUpdated();
}

/**
 * @brief Determines the status for resuming execution.
 *
 * @param pass_exception Whether to pass the exception to the application.
 * @return The event status for resuming.
 */
edb::EventStatus Debugger::resumeStatus(bool pass_exception) {

	if (pass_exception && lastEvent_ && lastEvent_->stopped() && !lastEvent_->isTrap()) {
		return edb::DEBUG_EXCEPTION_NOT_HANDLED;
	}

	return edb::DEBUG_CONTINUE;
}

/**
 * @brief Resumes execution.
 *
 * @param pass_exception Whether to pass the exception to the application.
 * @param mode The debug mode.
 * @param flags The resume flags.
 */
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

/**
 * @brief Called when the "Run Pass Signal To Application" action is triggered.
 */
void Debugger::on_action_Run_Pass_Signal_To_Application_triggered() {
	resumeExecution(PassException, Run, ResumeFlag::None);
}

/**
 * @brief
 */
void Debugger::on_action_Step_Into_Pass_Signal_To_Application_triggered() {
	resumeExecution(PassException, Step, ResumeFlag::None);
}

/**
 * @brief
 */
void Debugger::on_action_Run_triggered() {
	resumeExecution(IgnoreException, Run, ResumeFlag::None);
}

/**
 * @brief
 */
void Debugger::on_action_Step_Into_triggered() {
	resumeExecution(IgnoreException, Step, ResumeFlag::None);
}

/**
 * @brief
 */
void Debugger::on_action_Detach_triggered() {
	detachFromProcess(NoKillOnDetach);
}

/**
 * @brief
 */
void Debugger::on_action_Kill_triggered() {
	detachFromProcess(KillOnDetach);
}

/**
 * @brief Called when the "Step Over Pass Signal To Application" action is triggered.
 */
void Debugger::on_action_Step_Over_Pass_Signal_To_Application_triggered() {
	stepOver([this]() { on_action_Run_Pass_Signal_To_Application_triggered(); },
			 [this]() { on_action_Step_Into_Pass_Signal_To_Application_triggered(); });
}

/**
 * @brief Called when the "Step Over" action is triggered.
 */
void Debugger::on_action_Step_Over_triggered() {
	stepOver([this]() { on_action_Run_triggered(); },
			 [this]() { on_action_Step_Into_triggered(); });
}

/**
 * @brief Called when the "Step Out" action is triggered.
 */
void Debugger::on_actionStep_Out_triggered() {
	on_actionRun_Until_Return_triggered();
}

/**
 * @brief Called when the "Run Until Return" action is triggered.
 */
void Debugger::on_actionRun_Until_Return_triggered() {

	new RunUntilRet();

	// Step over rather than resume in MODE_STEP so that we can avoid stepping into calls.
	// TODO: If we are sitting on the call and it has a bp, it steps over for some reason...
	stepOver([this]() { on_action_Run_triggered(); },
			 [this]() { on_action_Step_Into_triggered(); });
}

/**
 * @brief Called when the "Pause" action is triggered.
 */
void Debugger::on_action_Pause_triggered() {
	Q_ASSERT(edb::v1::debugger_core);
	if (IProcess *process = edb::v1::debugger_core->process()) {
		process->pause();
	}
}

/**
 * @brief Cleans up the debugger state.
 */
void Debugger::cleanupDebugger() {

	timer_->stop();

	cpuView_->clearComments();
	edb::v1::memory_regions().clear();
	edb::v1::symbol_manager().clear();
	edb::v1::arch_processor().reset();

	// clear up the data view
	while (tabWidget_->count() > 1) {
		mnuDumpDeleteTab();
	}

	tabWidget_->setData(0, QString());

	Q_ASSERT(!dataRegions_.isEmpty());
	dataRegions_.first()->region = nullptr;

	Q_EMIT detachEvent();

	setWindowTitle(tr("edb"));

	updateUi();
}

/**
 * @brief Returns the filename for the current session.
 *
 * @return The session filename or an empty string if no session path is specified.
 */
QString Debugger::sessionFilename() const {

	static bool show_path_notice = true;

	QString session_path = edb::v1::config().session_path;
	if (session_path.isEmpty()) {
		if (show_path_notice) {
			qDebug("No session path specified. Please set it in the preferences to enable sessions.");
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

/**
 * @brief Detaches the debugger from the current process.
 *
 * @param kill Whether to kill the process or just detach.
 */
void Debugger::detachFromProcess(DetachAction kill) {

	const QString filename = sessionFilename();
	if (!filename.isEmpty()) {
		SessionManager::instance().saveSession(filename);
	}

	programExecutable_.clear();

	if (edb::v1::debugger_core) {
		if (kill == KillOnDetach) {
			edb::v1::debugger_core->kill();
		} else {
			edb::v1::debugger_core->detach();
		}
	}

	lastEvent_ = nullptr;

	cleanupDebugger();
	updateMenuState(Terminated);
}

/**
 * @brief Sets the initial debugger state.
 */
void Debugger::setInitialDebuggerState() {

	updateMenuState(Paused);
	timer_->start(0);

	edb::v1::symbol_manager().clear();
	edb::v1::memory_regions().sync();

	Q_ASSERT(!dataRegions_.empty());

	dataRegions_.first()->region = edb::v1::primary_data_region();

	if (IAnalyzer *const analyzer = edb::v1::analyzer()) {
		analyzer->invalidateAnalysis();
	}

	reenableBreakpointRun_  = nullptr;
	reenableBreakpointStep_ = nullptr;

#ifdef Q_OS_LINUX
	debugPointer_ = 0;
	loadedModules_.clear();
#endif

	IProcess *process        = edb::v1::debugger_core->process();
	const QString executable = process ? process->executable() : QString();

	setDebuggerCaption(executable);

	programExecutable_.clear();
	if (!executable.isEmpty()) {
		programExecutable_ = executable;
	}

	const QString filename = sessionFilename();
	if (!filename.isEmpty()) {

		SessionManager &session_manager = SessionManager::instance();

		Result<void, SessionError> session_error = session_manager.loadSession(filename);
		if (!session_error) {
			QMessageBox::warning(
				this,
				tr("Error Loading Session"),
				session_error.error().message);
		}
	}

	// create our binary info object for the primary code module
	binaryInfo_ = edb::v1::get_binary_info(edb::v1::primary_code_region());

	commentServer_->clear();
	commentServer_->setComment(process->entryPoint(), QStringLiteral("<entry point>"));

	setLibraryLoadHook();
}

/**
 * @brief Tests if the binary is native.
 */
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

/**
 * @brief Sets the initial breakpoint so we can stop at the entry point of the application.
 *
 * @param s The path to the executable.
 */
void Debugger::setInitialBreakpoint(const QString &s) {

	edb::address_t entryPoint = 0;

	if (edb::v1::config().initial_breakpoint == Configuration::MainSymbol) {
		const QString mainSymbol        = QFileInfo(s).fileName() + QStringLiteral("!main");
		const std::optional<Symbol> sym = edb::v1::symbol_manager().find(mainSymbol);

		if (sym) {
			entryPoint = sym->address;
		} else if (edb::v1::config().find_main) {
			entryPoint = edb::v1::locate_main_function();
		}
	}

	if (entryPoint == 0 || edb::v1::config().initial_breakpoint == Configuration::EntryPoint) {
		if (IProcess *process = edb::v1::debugger_core->process()) {
			entryPoint = process->entryPoint();
		}
	}

	if (entryPoint != 0) {
		if (std::shared_ptr<IBreakpoint> bp = edb::v1::debugger_core->addBreakpoint(entryPoint)) {
			bp->setOneTime(true);
			bp->setInternal(true);
			bp->tag = initial_bp_tag;
		}
	}
}

/**
 * @brief Restarts the current debugging session.
 */
void Debugger::on_action_Restart_triggered() {

	Q_ASSERT(edb::v1::debugger_core);
	if (IProcess *process = edb::v1::debugger_core->process()) {

		workingDirectory_      = process->currentWorkingDirectory();
		QList<QByteArray> args = process->arguments();
		const QString exe      = process->executable();
		const QString in       = process->standardInput();
		const QString out      = process->standardOutput();

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
		const auto &[path, args]                 = file;
		if (commonOpen(path, args, QString(), QString())) {
			argumentsDialog_->setArguments(args);
		}
	}
}

/**
 * @brief Sets up the data views for the debugger.
 */
void Debugger::setupDataViews() {

	// Setup data views according to debuggee bitness
	if (edb::v1::debuggeeIs64Bit()) {
		stackView_->setAddressSize(QHexView::Address64);
		for (const std::shared_ptr<DataViewInfo> &data_view : dataRegions_) {
			data_view->view->setAddressSize(QHexView::Address64);
		}
	} else {
		stackView_->setAddressSize(QHexView::Address32);
		for (const std::shared_ptr<DataViewInfo> &data_view : dataRegions_) {
			data_view->view->setAddressSize(QHexView::Address32);
		}
	}

	// Update stack word width
	stackView_->setWordWidth(static_cast<int>(edb::v1::pointer_size()));
}

/**
 * @brief Sets a library load hook to catch library load/unload events.
 */
void Debugger::setLibraryLoadHook() {
	// NOTE(eteran): this isn't really "Linux" specific, but it is the only platform that we currently support
	// that has a dynamic loader that we can hook into to get library load/unload events. If we ever support other
	// platforms with dynamic loaders, we should add support for them here as well.
#if defined(Q_OS_LINUX)
	// Find the linker magic breakpoint function so we can set a breakpoint on it to catch library load/unload events.

	// Annoyingly, we have to sync the memory regions here, because that's what causes the symbols to be loaded,
	// and we need the symbols to find the _dl_debug_state symbol. This is a bit of a hack, but it works for now.
	edb::v1::memory_regions().sync();

	std::vector<Symbol> symbols = edb::v1::symbol_manager().findAll(QStringLiteral("_dl_debug_state"));
	if (!symbols.empty()) {

		const Symbol &symbol = symbols.front();
		if (std::shared_ptr<IBreakpoint> bp = edb::v1::debugger_core->addBreakpoint(symbol.address)) {
			bp->setInternal(true);
			bp->tag = ld_loader_tag;
			qDebug() << "Set breakpoint on _dl_debug_state at" << edb::v1::format_pointer(symbol.address);
		}
#endif
	}
}

/**
 * @brief Common function to open a process for debugging.
 *
 * @param s The path to the executable.
 * @param args The arguments to pass to the executable.
 * @param input The input file for the process.
 * @param output The output file for the process.
 * @return True if the process was opened successfully, false otherwise.
 */
bool Debugger::commonOpen(const QString &s, const QList<QByteArray> &args, const QString &input, const QString &output) {

	// ensure that the previous running process (if any) is dealt with...
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

/**
 * @brief
 */
void Debugger::execute(const QString &program, const QList<QByteArray> &args, const QString &input, const QString &output) {
	if (commonOpen(program, args, input, output)) {
		recentFileManager_->addFile(program, args);
		argumentsDialog_->setArguments(args);
	}
}

/**
 * @brief
 */
void Debugger::openFile(const QString &filename, const QList<QByteArray> &args) {
	if (!filename.isEmpty()) {
		lastOpenDirectory_ = QFileInfo(filename).canonicalFilePath();

		execute(filename, args, QString(), QString());
	}
}

/**
 * @brief
 */
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
		if (IProcess *process = edb::v1::debugger_core->process()) {

			workingDirectory_      = process->currentWorkingDirectory();
			QList<QByteArray> args = process->arguments();

			if (!args.empty()) {
				args.removeFirst();
			}

			argumentsDialog_->setArguments(args);
			attachComplete();
		}
	} else {
		QMessageBox::critical(this, tr("Attach"), tr("Failed to attach to process: %1").arg(status.error()));
	}

	updateUi();
}

/**
 * @brief Called when the debugger has successfully attached to a process.
 */
void Debugger::attachComplete() {
	setInitialDebuggerState();

	testNativeBinary();

	setupDataViews();

	QString ip   = edb::v1::debugger_core->instructionPointer().toUpper();
	QString sp   = edb::v1::debugger_core->stackPointer().toUpper();
	QString bp   = edb::v1::debugger_core->framePointer().toUpper();
	QString word = edb::v1::debuggeeIs64Bit() ? QStringLiteral("QWORD") : QStringLiteral("DWORD");

	setRIPAction_->setText(tr("&Set %1 to this Instruction").arg(ip));
	gotoRIPAction_->setText(tr("&Goto %1").arg(ip));
	stackGotoRSPAction_->setText(tr("Goto %1").arg(sp));
	stackGotoRBPAction_->setText(tr("Goto %1").arg(bp));
	stackPushAction_->setText(tr("&Push %1").arg(word));
	stackPopAction_->setText(tr("P&op %1").arg(word));

	Q_EMIT attachEvent();
}

/**
 * @brief
 */
void Debugger::on_action_Open_triggered() {

	static auto dialog = new DialogOpenProgram(this, tr("Choose a file"), lastOpenDirectory_);

	// Set a sensible default dir
	if (recentFileManager_->entryCount() > 0) {
		const RecentFileManager::RecentFile file = recentFileManager_->mostRecent();
		const auto &[path, args]                 = file;
		Q_UNUSED(args)
		const QDir dir = QFileInfo(path).dir();
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

/**
 * @brief Triggered when the user wants to attach to a process.
 */
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

/**
 * @brief Triggered when the user wants to view the memory regions.
 */
void Debugger::on_action_Memory_Regions_triggered() {
	static QPointer<DialogMemoryRegions> dlg = new DialogMemoryRegions(this);
	dlg->show();
}

/**
 * @brief Triggered when the user wants to view the threads.
 */
void Debugger::on_action_Threads_triggered() {
	static QPointer<DialogThreads> dlg = new DialogThreads(this);
	dlg->show();
}

/**
 * @brief Duplicates the current tab creating a new one.
 */
void Debugger::mnuDumpCreateTab() {
	createDataTab();
	tabDelete_->setEnabled(tabWidget_->count() > 1);
}

/**
 * @brief Handles removing of a memory view tab.
 */
void Debugger::mnuDumpDeleteTab() {
	deleteDataTab();
	tabDelete_->setEnabled(tabWidget_->count() > 1);
}

/**
 * @brief Returns context menu items using supplied function to call for each plugin.
 * nullptr items mean "create separator here".
 * @param f The function to call for each plugin.
 * @return A list of context menu items for each plugin.
 */
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

/**
 * @brief Adds context menu items for each plugin to the specified menu.
 *
 * @param menu The menu to add items to.
 * @param f The function to call for each plugin.
 */
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

/**
 * @brief
 */
void Debugger::on_action_Plugins_triggered() {
	static auto dlg = new DialogPlugins(this);
	dlg->show();
}

/**
 * @brief
 */
bool Debugger::jumpToAddress(edb::address_t address) {

	if (std::shared_ptr<IRegion> region = edb::v1::memory_regions().findRegion(address)) {
		doJumpToAddress(address, region, true);
		return true;
	}

	return false;
}

/**
 * @brief
 */
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

/**
 * @brief
 */
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

/**
 * @brief
 */
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

/**
 * @brief Shows the context menu for a tab.
 *
 * @param index The index of the tab.
 * @param pos The position of the mouse click.
 */
void Debugger::tabContextMenu(int index, const QPoint &pos) {
	QMenu menu;
	QAction *const actionAdd   = menu.addAction(tr("&Set Label"));
	QAction *const actionClear = menu.addAction(tr("&Clear Label"));
	QAction *const chosen      = menu.exec(tabWidget_->mapToGlobal(pos));

	if (chosen == actionAdd) {
		bool ok;
		const QString text = QInputDialog::getText(
			this,
			tr("Set Caption"),
			tr("Caption:"),
			QLineEdit::Normal,
			tabWidget_->data(index).toString(),
			&ok);

		if (ok && !text.isEmpty()) {
			tabWidget_->setData(index, text);
		}
	} else if (chosen == actionClear) {
		tabWidget_->setData(index, QString());
	}

	updateUi();
}

/**
 * @brief Processes the next debug event.
 */
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

/**
 * @brief Opens the help documentation in the default web browser.
 */
void Debugger::on_action_Help_triggered() {
	QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/eteran/edb-debugger/wiki"), QUrl::TolerantMode));
}

/**
 * @brief Returns the status label.
 *
 * @return A pointer to the status label.
 */
QLabel *Debugger::statusLabel() const {
	return status_;
}

/**
 * @brief Shows the breakpoints dialog.
 */
void Debugger::on_action_Breakpoints_triggered() {
	if (!breakpointDialog_) {
		breakpointDialog_ = new DialogBreakpoints(edb::v1::debugger_ui);
	}

	breakpointDialog_->show();
}

void Debugger::on_action_Reset_UI_triggered() {

	int resp = QMessageBox::question(edb::v1::debugger_ui,
									 tr("Reset UI Layout?"),
									 tr("Are you sure you want to reset the UI layout to its default state? This will take effect after restarting edb."),
									 QMessageBox::Yes | QMessageBox::No,
									 QMessageBox::No);

	if (resp != QMessageBox::Yes) {
		return;
	}

	QSettings settings;
	settings.beginGroup(QStringLiteral("Window"));
	settings.remove(QString());
	settings.endGroup();
	ui_reset_ = true;
}

/**
 * @brief Handles library load/unload events.
 *
 * @param process The process that triggered the event.
 * @param debug_pointer The platform-specific pointer to the platform-specific debug structure. (r_debug on Linux)
 */
template <class Addr>
void Debugger::handle_library_event(IProcess *process, [[maybe_unused]] edb::address_t debug_pointer) {

	if (!process) {
		return;
	}

	if (!debug_pointer) {
		return;
	}

	qDebug() << "Library event triggered, reading dynamic loader info from" << edb::v1::format_pointer(debug_pointer);

#ifdef Q_OS_LINUX
	edb::linux_struct::r_debug<Addr> dynamic_info;
	const bool ok = (process->readBytes(debug_pointer, &dynamic_info, sizeof(dynamic_info)) == sizeof(dynamic_info));
	if (ok) {

		switch (dynamic_info.r_state) {
		case edb::linux_struct::r_debug<Addr>::RT_CONSISTENT:
			break;
		case edb::linux_struct::r_debug<Addr>::RT_ADD: {
			qDebug("LIBRARY LOAD EVENT");

			QSet<Module> modules       = process->loadedModules();
			QSet<Module> added_modules = modules - loadedModules_;

			qDebug("Added modules:");
			for (const Module &module : added_modules) {
				qDebug() << "  " << module.name << "@" << edb::v1::format_pointer(module.baseAddress);

				// Notify all plugins who care about library load/unload events
				for (QObject *plugin : edb::v1::plugin_list()) {
					if (auto p = qobject_cast<IPlugin *>(plugin)) {
						p->libraryEvent(module, true);
					}
				}

				// Notify the session manager about the library load event
				// NOTE(eteran): this is a bit "out of place", because we are using it to restore labels/comments,
				// but it would be better if we informed the symbol manager more directly. But that's a larger refactor,
				// so for now, this is fine.
				SessionManager::instance().libraryEvent(module, true);
			}

			loadedModules_ = modules;
			break;
		}
		case edb::linux_struct::r_debug<Addr>::RT_DELETE: {
			qDebug("LIBRARY UNLOAD EVENT");

			QSet<Module> modules         = process->loadedModules();
			QSet<Module> removed_modules = loadedModules_ - modules;

			qDebug("Removed modules:");
			for (const Module &module : removed_modules) {
				qDebug() << "  " << module.name << "@" << edb::v1::format_pointer(module.baseAddress);

				for (QObject *plugin : edb::v1::plugin_list()) {
					if (auto p = qobject_cast<IPlugin *>(plugin)) {
						p->libraryEvent(module, false);
					}
				}

				SessionManager::instance().libraryEvent(module, false);
			}

			loadedModules_ = modules;
			break;
		}
		}
	}
#endif
}

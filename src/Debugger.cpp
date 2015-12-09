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
#include "DialogOptions.h"
#include "DialogPlugins.h"
#include "DialogThreads.h"
#include "DialogOpenProgram.h"
#include "Expression.h"
#include "IAnalyzer.h"
#include "IBinary.h"
#include "IDebugEvent.h"
#include "IDebugger.h"
#include "IPlugin.h"
#include "Instruction.h"
#include "MemoryRegions.h"
#include "QHexView"
#include "RecentFileManager.h"
#include "State.h"
#include "SymbolManager.h"
#include "edb.h"

#include <QCloseEvent>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
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
#include <QDesktopServices>
#include <QLabel>

#include <memory>
#include <cstring>

#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
#include <link.h>
#endif

#if defined(Q_OS_UNIX)
#include <signal.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#if defined(Q_OS_LINUX) || defined(Q_OS_OPENBSD)
#include <unistd.h>
#endif

namespace {

const quint64 initial_bp_tag  = Q_UINT64_C(0x494e4954494e5433); // "INITINT3" in hex
const quint64 stepover_bp_tag = Q_UINT64_C(0x535445504f564552); // "STEPOVER" in hex
const quint64 run_to_cursor_tag = Q_UINT64_C(0x474f544f48455245); // "GOTOHERE" in hex

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
public:
	//--------------------------------------------------------------------------
	// Name: RunUntilRet
	//--------------------------------------------------------------------------
	RunUntilRet() :previous_handler_(0), last_call_return_(0) {
		previous_handler_ = edb::v1::set_debug_event_handler(this);
	}

	//--------------------------------------------------------------------------
	// Name: pass_back_to_debugger
	// Desc: Makes the previous handler the event handler again and deletes this.
	//--------------------------------------------------------------------------
	virtual edb::EVENT_STATUS pass_back_to_debugger(const IDebugEvent::const_pointer &event) {
		edb::EVENT_STATUS status = previous_handler_->handle_event(event);
		edb::v1::set_debug_event_handler(previous_handler_);
		delete this;
		return status;
	}

	//--------------------------------------------------------------------------
	// Name: handle_event
	//--------------------------------------------------------------------------
	//TODO: Need to handle stop/pause button
	virtual edb::EVENT_STATUS handle_event(const IDebugEvent::const_pointer &event) {


		State state;
		edb::v1::debugger_core->get_state(&state);

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
			if (reason == IDebugEvent::EVENT_EXITED || reason == IDebugEvent::EVENT_TERMINATED ||
					address == 0) {
				qDebug() << "The process is no longer running.";
				return pass_back_to_debugger(event);
			}

			//Check the previous byte for 0xcc to see if it was an actual breakpoint
			edb::address_t prev_address = address - 1;
			IBreakpoint::pointer bp = edb::v1::find_breakpoint(prev_address);

			//If there was a bp there, then we hit a block terminator as part of our RunUntilRet
			//algorithm, or it is a user-set breakpoint.
			if(bp && bp->enabled()) { //Isn't it always enabled if trap_reason is breakpoint, anyway?

				bp->hit();

				//Adjust RIP since 1st byte was replaced with 0xcc and we are now 1 byte after it.
				state.set_instruction_pointer(prev_address);
				edb::v1::debugger_core->set_state(state);
				address = prev_address;

				//If it wasn't internal, it was a user breakpoint. Pass back to Debugger.
				if (!bp->internal()) {
					qDebug() << "Previous was not an internal breakpoint.";
					return pass_back_to_debugger(event);
				}
				qDebug() << "Previous was an internal breakpoint.";
				bp->disable();
				edb::v1::debugger_core->remove_breakpoint(bp->address());
			}

			//No breakpoint if it was a syscall; continue.
			else {
				return edb::DEBUG_CONTINUE;
			}
		}

		//If we are on our ret (or the instr after?), then ret.
		if (address == ret_address_) {
			qDebug() << QString("On our terminator at 0x%1").arg(address, 0, 16);
			if (is_instruction_ret(address)) {
				qDebug() << "Found ret; passing back to debugger";
				return pass_back_to_debugger(event);
			}

			//If not a ret, then step so we can find the next block terminator.
			else {
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
					if (IBreakpoint::pointer bp = edb::v1::debugger_core->find_breakpoint(address)) {
						qDebug() << QString("Already a breakpoint at terminator 0x%1").arg(address, 0, 16);
						return edb::DEBUG_CONTINUE;
					}

					//Otherwise, attempt to set a breakpoint there and continue.
					if (IBreakpoint::pointer bp = edb::v1::debugger_core->add_breakpoint(address)) {
						qDebug() << QString("Setting breakpoint at terminator 0x%1").arg(address, 0, 16);
						bp->set_internal(true);
						bp->set_one_time(true); //If the 0xcc get's rm'd on next event, then
						                        //don't set it one time; we'll hande it manually
						ret_address_ = address;
						return edb::DEBUG_CONTINUE;
					}

					//If we get here, then we've got a problem... Coludn't set a breakpoint on
					//a block terminator for some reason.
					qDebug() << "Couldn't set a breakpoint on a block terminator...";
				}
			}

			//Invalid instruction or some other problem. Pass it back to the debugger.
			else {
				qDebug() << "Invalid instruction or something.";
				return pass_back_to_debugger(event);
			}

			address += inst.size();
		}

		//If we end up out here, we've got bigger problems. Pass it back to the debugger.
		qDebug() << "Stepped outside the loop.  Bad.";
		return pass_back_to_debugger(event);
	}

private:
	IDebugEventHandler *previous_handler_;
	edb::address_t      last_call_return_;
	edb::address_t      ret_address_;
};


}

//------------------------------------------------------------------------------
// Name: Debugger
// Desc:
//------------------------------------------------------------------------------
Debugger::Debugger(QWidget *parent) : QMainWindow(parent),
		add_tab_(0),
		del_tab_(0),
		tty_proc_(new QProcess(this)),
		gui_state_(TERMINATED),
		stack_view_info_(IRegion::pointer()),
		arguments_dialog_(new DialogArguments),
		timer_(new QTimer(this)),
		recent_file_manager_(new RecentFileManager(this)),
		stack_comment_server_(new CommentServer),
		stack_view_locked_(false),
		auto_stack_word_width_(true),
		stack_word_width_(edb::v1::pointer_size())
#ifdef Q_OS_UNIX
		,debug_pointer_(0)
#endif
{
	setup_ui();

	// connect the timer to the debug event
	connect(timer_, SIGNAL(timeout()), this, SLOT(next_debug_event()));

	// create a context menu for the tab bar as well
	connect(ui.tabWidget, SIGNAL(customContextMenuRequested(int, const QPoint &)), this, SLOT(tab_context_menu(int, const QPoint &)));

	// CPU Shortcuts
	gotoAddressAction_           = createAction(tr("&Goto Address"),                                 QKeySequence(tr("Ctrl+G")),   SLOT(goto_triggered()));
	editCommentAction_           = createAction(tr("Add &Comment"),                                  QKeySequence(tr(";")),        SLOT(mnuCPUEditComment()));
	removeCommentAction_         = createAction(tr("Remove Comment"),                                QKeySequence(),               SLOT(mnuCPURemoveComment()));
	editBytesAction_             = createAction(tr("&Edit Bytes"),                                   QKeySequence(tr("Ctrl+E")),   SLOT(mnuCPUModify()));
	toggleBreakpointAction_      = createAction(tr("&Toggle Breakpoint"),                            QKeySequence(tr("F2")),       SLOT(mnuCPUToggleBreakpoint()));
	conditionalBreakpointAction_ = createAction(tr("Add &Conditional Breakpoint"),                   QKeySequence(tr("Shift+F2")), SLOT(mnuCPUAddConditionalBreakpoint()));
	runToThisLineAction_         = createAction(tr("R&un to this Line"),                             QKeySequence(tr("F4")),       SLOT(mnuCPURunToThisLine()));
	runToLinePassAction_         = createAction(tr("Run to this Line (Pass Signal To Application)"), QKeySequence(tr("Shift+F4")), SLOT(mnuCPURunToThisLinePassSignal()));
	fillWithZerosAction_         = createAction(tr("&Fill with 00's"),                               QKeySequence(),               SLOT(mnuCPUFillZero()));
	fillWithNOPsAction_          = createAction(tr("Fill with &NOPs"),                               QKeySequence(),               SLOT(mnuCPUFillNop()));
	removeBreakpointAction_      = createAction(tr("&Remove Breakpoint"),                            QKeySequence(),               SLOT(mnuCPURemoveBreakpoint()));
	setAddressLabelAction_       = createAction(tr("Set Address &Label"),                            QKeySequence(tr(":")),        SLOT(mnuCPULabelAddress()));
	followConstantInDumpAction_  = createAction(tr("Follow Constant In &Dump"),                      QKeySequence(),               SLOT(mnuCPUFollowInDump()));
	followConstantInStackAction_ = createAction(tr("Follow Constant In &Stack"),                     QKeySequence(),               SLOT(mnuCPUFollowInStack()));
	followAction_                = createAction(tr("&Follow"),                                       QKeySequence(),               SLOT(mnuCPUFollow()));
	
	// these get updated when we attach/run a new process, so it's OK to hard code them here
#if defined(EDB_X86_64)
	setRIPAction_                = createAction(tr("&Set %1 to this Instruction").arg("RIP"),        QKeySequence(tr("Ctrl+*")),   SLOT(mnuCPUSetEIP()));
	gotoRIPAction_               = createAction(tr("&Goto %1").arg("RIP"),                           QKeySequence(tr("*")),        SLOT(mnuCPUJumpToEIP()));
#elif defined(EDB_X86)
	setRIPAction_                = createAction(tr("&Set %1 to this Instruction").arg("EIP"),        QKeySequence(tr("Ctrl+*")),   SLOT(mnuCPUSetEIP()));
	gotoRIPAction_               = createAction(tr("&Goto %1").arg("EIP"),                           QKeySequence(tr("*")),        SLOT(mnuCPUJumpToEIP()));
#endif
	
	// Data Dump Shorcuts
	dumpFollowInCPUAction_       = createAction(tr("Follow Address In &CPU"),                        QKeySequence(),               SLOT(mnuDumpFollowInCPU()));
	dumpFollowInDumpAction_      = createAction(tr("Follow Address In &Dump"),                       QKeySequence(),               SLOT(mnuDumpFollowInDump()));
	dumpFollowInStackAction_     = createAction(tr("Follow Address In &Stack"),                      QKeySequence(),               SLOT(mnuDumpFollowInStack()));
	dumpEditBytesAction_         = createAction(tr("&Edit Bytes"),                                   QKeySequence(),               SLOT(mnuDumpModify()));
	dumpSaveToFileAction_        = createAction(tr("&Save To File"),                                 QKeySequence(),               SLOT(mnuDumpSaveToFile()));
	
	// Register View Shortcuts
	registerFollowInDumpAction_    = createAction(tr("&Follow In Dump"),           QKeySequence(), SLOT(mnuRegisterFollowInDump()));
	registerFollowInDumpTabAction_ = createAction(tr("&Follow In Dump (New Tab)"), QKeySequence(), SLOT(mnuRegisterFollowInDumpNewTab()));
	registerFollowInStackAction_   = createAction(tr("&Follow In Stack"),          QKeySequence(), SLOT(mnuRegisterFollowInStack()));

	// Stack View Shortcuts
	stackFollowInCPUAction_   = createAction(tr("Follow Address In &CPU"),   QKeySequence(), SLOT(mnuStackFollowInCPU()));
	stackFollowInDumpAction_  = createAction(tr("Follow Address In &Dump"),  QKeySequence(), SLOT(mnuStackFollowInDump()));
	stackFollowInStackAction_ = createAction(tr("Follow Address In &Stack"), QKeySequence(), SLOT(mnuStackFollowInStack()));
	stackEditBytesAction_     = createAction(tr("&Edit Bytes"),              QKeySequence(), SLOT(mnuStackModify()));
	
	// these get updated when we attach/run a new process, so it's OK to hard code them here	
#if defined(EDB_X86_64)
	stackGotoRSPAction_ = createAction(tr("Goto %1").arg("RSP"),       QKeySequence(), SLOT(mnuStackGotoESP()));
	stackGotoRBPAction_ = createAction(tr("Goto %1").arg("RBP"),       QKeySequence(), SLOT(mnuStackGotoEBP()));
	stackPushAction_    = createAction(tr("&Push %1").arg("QWORD"),    QKeySequence(), SLOT(mnuStackPush()));
	stackPopAction_     = createAction(tr("P&op %1").arg("QWORD"),     QKeySequence(), SLOT(mnuStackPop()));
#elif defined(EDB_X86)
	stackGotoRSPAction_ = createAction(tr("Goto %1").arg("ESP"),       QKeySequence(), SLOT(mnuStackGotoESP()));
	stackGotoRBPAction_ = createAction(tr("Goto %1").arg("EBP"),       QKeySequence(), SLOT(mnuStackGotoEBP()));
	stackPushAction_    = createAction(tr("&Push %1").arg("DWORD"),    QKeySequence(), SLOT(mnuStackPush()));
	stackPopAction_     = createAction(tr("P&op %1").arg("DWORD"),     QKeySequence(), SLOT(mnuStackPop()));
#endif

	
	// set these to have no meaningful "data" (yet)
	followConstantInDumpAction_->setData(qlonglong(0));
	followConstantInStackAction_->setData(qlonglong(0));
	followAction_->setData(qlonglong(0));
	
	

	setAcceptDrops(true);

	// setup the list model for instruction details list
	list_model_ = new QStringListModel(this);
	ui.listView->setModel(list_model_);

	// setup the recent file manager
	ui.action_Recent_Files->setMenu(recent_file_manager_->create_menu());
	connect(recent_file_manager_, SIGNAL(file_selected(const QString &)), SLOT(open_file(const QString &)));

	// make us the default event handler
	edb::v1::set_debug_event_handler(this);

	// enable the arch processor
	edb::v1::arch_processor().setup_register_view(ui.registerList);

	// default the working directory to ours
	working_directory_ = QDir().absolutePath();

	// let the plugins setup their menus
	finish_plugin_setup();
}

//------------------------------------------------------------------------------
// Name: ~Debugger
// Desc:
//------------------------------------------------------------------------------
Debugger::~Debugger() {

	detach_from_process((edb::v1::config().close_behavior == Configuration::Terminate) ? KILL_ON_DETACH : NO_KILL_ON_DETACH);

	// kill our xterm and wait for it to die
	tty_proc_->kill();
	tty_proc_->waitForFinished(3000);
}

//------------------------------------------------------------------------------
// Name: createAction
// Desc:
//------------------------------------------------------------------------------
QAction *Debugger::createAction(const QString &text, const QKeySequence &keySequence, const char *slot) {
	auto action = new QAction(text, this);
	action->setShortcut(keySequence);
	addAction(action);
	connect(action, SIGNAL(triggered()), this, slot);
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
		break;
	case TERMINATED:
		ui.actionRun_Until_Return->setEnabled(false);
		ui.action_Restart->setEnabled(false);
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

				const Q_PID tty_pid = tty_proc_->pid();
				Q_UNUSED(tty_pid);

				// get the output, this should block until the xterm actually gets a chance to write it
				QFile file(temp_pipe);
				if(file.open(QIODevice::ReadOnly)) {
					result_tty = file.readLine().trimmed();
				}

			} else {
				qDebug().nospace() << "Could not launch the desired terminal [" << tty_command << "], please check that it exists and you have proper permissions.";
			}

			// cleanup, god i wish there was an easier way than this!
			::unlink(qPrintable(temp_pipe));
		}
	}
#endif
	return result_tty;
}

//------------------------------------------------------------------------------
// Name: tty_proc_finished
// Desc: cleans up the data associated with a TTY when the terminal dies
//------------------------------------------------------------------------------
void Debugger::tty_proc_finished(int exit_code, QProcess::ExitStatus exit_status) {
	Q_UNUSED(exit_code);
	Q_UNUSED(exit_status);

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
DataViewInfo::pointer Debugger::current_data_view_info() const {
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
	DataViewInfo::pointer info = data_regions_[current];

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
	auto new_data_view = std::make_shared<DataViewInfo>((current != -1) ? data_regions_[current]->region : IRegion::pointer());

	auto hexview = std::make_shared<QHexView>();

	new_data_view->view = hexview;

	// setup the context menu
	hexview->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(hexview.get(), SIGNAL(customContextMenuRequested(const QPoint &)), SLOT(mnuDumpContextMenu(const QPoint &)));

	// show the initial data for this new view
	if(new_data_view->region) {
		hexview->setAddressOffset(new_data_view->region->start());
	} else {
		hexview->setAddressOffset(0);
	}

	hexview->setData(new_data_view->stream);

	const Configuration &config = edb::v1::config();

	// set the default view options
	hexview->setShowAddress(config.data_show_address);
	hexview->setShowHexDump(config.data_show_hex);
	hexview->setShowAsciiDump(config.data_show_ascii);
	hexview->setShowComments(config.data_show_comments);
	hexview->setRowWidth(config.data_row_width);
	hexview->setWordWidth(config.data_word_width);

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
			edb::v1::format_pointer(0),
			edb::v1::format_pointer(0)));
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
					connect(new QShortcut(shortcut, this), SIGNAL(activated()), action, SLOT(trigger()));
				}
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: get_goto_expression
// Desc:
//------------------------------------------------------------------------------
edb::address_t Debugger::get_goto_expression(bool *ok) {

	Q_ASSERT(ok);

	edb::address_t address;
	*ok = edb::v1::get_expression_from_user(tr("Goto Address"), tr("Address:"), &address);
	return *ok ? address : edb::address_t(0);
}

//------------------------------------------------------------------------------
// Name: get_follow_register
// Desc:
//------------------------------------------------------------------------------
edb::reg_t Debugger::get_follow_register(bool *ok) const {

	Q_ASSERT(ok);

	*ok = false;
	if(const QTreeWidgetItem *const i = ui.registerList->currentItem()) {
		if(const Register reg = edb::v1::arch_processor().value_from_item(*i)) {
			if(reg.type() & (Register::TYPE_GPR | Register::TYPE_IP)) {
				*ok = true;
				return reg.valueAsAddress();
			}
		}
	}

	return 0;
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
	ui.menu_View->addAction(ui.registersDock->toggleViewAction());
	ui.menu_View->addAction(ui.dataDock     ->toggleViewAction());
	ui.menu_View->addAction(ui.stackDock    ->toggleViewAction());
	ui.menu_View->addAction(ui.toolBar      ->toggleViewAction());

	// make sure our widgets use custom context menus
	ui.registerList->setContextMenuPolicy(Qt::CustomContextMenu);
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
	ui.stackDock->setWidget(stack_view_.get());

	// setup the context menu
	stack_view_->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(stack_view_.get(), SIGNAL(customContextMenuRequested(const QPoint &)), SLOT(mnuStackContextMenu(const QPoint &)));

	// we placed a view in the designer, so just set it here
	// this may get transitioned to heap allocated, we'll see
	stack_view_info_.view = stack_view_;

	// setup the comment server for the stack viewer
	stack_view_->setCommentServer(stack_comment_server_);
}

//------------------------------------------------------------------------------
// Name: closeEvent
// Desc: triggered on main window close, saves window state
//------------------------------------------------------------------------------
void Debugger::closeEvent(QCloseEvent *event) {
	QSettings settings;
	const QByteArray state = saveState();
	settings.beginGroup("Window");
	settings.setValue("window.state", state);
	settings.setValue("window.width", width());
	settings.setValue("window.height", height());
	settings.setValue("window.stack.show_address.enabled", stack_view_->showAddress());
	settings.setValue("window.stack.show_hex.enabled", stack_view_->showHexDump());
	settings.setValue("window.stack.show_ascii.enabled", stack_view_->showAsciiDump());
	settings.setValue("window.stack.show_comments.enabled", stack_view_->showComments());
	settings.setValue("window.stack.row_width", stack_view_->rowWidth());
	if(auto_stack_word_width_)
		settings.setValue("window.stack.word_width", -1);
	else
		settings.setValue("window.stack.word_width", stack_view_->wordWidth());
		
	
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
	const QByteArray state = settings.value("window.state").value<QByteArray>();
	const int width        = settings.value("window.width", -1).value<int>();
	const int height       = settings.value("window.height", -1).value<int>();

	if(width != -1) {
		resize(width, size().height());
	}

	if(height != -1) {
		resize(size().width(), height);
	}

	stack_view_->setShowAddress(settings.value("window.stack.show_address.enabled", true).value<bool>());
	stack_view_->setShowHexDump(settings.value("window.stack.show_hex.enabled", true).value<bool>());
	stack_view_->setShowAsciiDump(settings.value("window.stack.show_ascii.enabled", true).value<bool>());
	stack_view_->setShowComments(settings.value("window.stack.show_comments.enabled", true).value<bool>());

	int row_width = settings.value("window.stack.row_width", 1).value<int>();

	{
		int word_width = settings.value("window.stack.word_width", edb::v1::pointer_size()).value<int>();
		auto_stack_word_width_=(word_width<0);
		if(auto_stack_word_width_)
			stack_word_width_=edb::v1::pointer_size();
		else
			stack_word_width_=word_width;
	}

	// normalize values
	if(stack_word_width_ != 1 && stack_word_width_ != 2 && stack_word_width_ != 4 && stack_word_width_ != 8) {
		stack_word_width_ = edb::v1::pointer_size();
	}

	if(row_width != 1 && row_width != 2 && row_width != 4 && row_width != 8 && row_width != 16) {
		row_width = 1;
	}

	stack_view_->setRowWidth(row_width);
	stack_view_->setWordWidth(stack_word_width_);
	
	
	QByteArray disassemblyState = settings.value("window.disassembly.state").value<QByteArray>();
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
		QUrl url = mimeData->urls()[0].toLocalFile();
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
		const QString s = mimeData->urls()[0].toLocalFile();
		if(!s.isEmpty()) {
			Q_ASSERT(edb::v1::debugger_core);

			detach_from_process(KILL_ON_DETACH);
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
		ui.registerList->setFont(font);
	}

	if(font.fromString(config.disassembly_font)) {
		ui.cpuView->setFont(font);
	}

	if(font.fromString(config.data_font)) {
		for(const DataViewInfo::pointer &data_view: data_regions_) {
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

	connect(add_tab_, SIGNAL(clicked()), SLOT(mnuDumpCreateTab()));
	connect(del_tab_, SIGNAL(clicked()), SLOT(mnuDumpDeleteTab()));
}

//------------------------------------------------------------------------------
// Name: on_registerList_customContextMenuRequested
// Desc: context menu handler for register view
//------------------------------------------------------------------------------
void Debugger::on_registerList_customContextMenuRequested(const QPoint &pos) {
	QTreeWidgetItem *const item = ui.registerList->itemAt(pos);
	if(item && !ui.registerList->isCategory(item)) {
		// a little bit cheesy of a solution, but should work nicely
		if(const Register reg = edb::v1::arch_processor().value_from_item(*item)) {
			if(reg.type() & (Register::TYPE_GPR | Register::TYPE_IP)) {
				QMenu menu;
				menu.addAction(registerFollowInDumpAction_);
				menu.addAction(registerFollowInDumpTabAction_);
				menu.addAction(registerFollowInStackAction_);

				add_plugin_context_menu(&menu, &IPlugin::register_context_menu);

				menu.exec(ui.registerList->mapToGlobal(pos));
			}
			else {
				// Generally it's better to ask ArchProcessor to make its arch-specific item-specific menu
				const auto menu = edb::v1::arch_processor().register_item_context_menu(reg);
				if(menu) {
					menu->exec(ui.registerList->mapToGlobal(pos));
				}
				update_gui();
				refresh_gui();
			}
		}

		// Special case for the flag values. Open a context menu with the flag names for toggle.
		else if (QTreeWidgetItem *parent = item->parent()) {
			if (const Register reg = edb::v1::arch_processor().value_from_item(*parent)) {
				if (reg.name() == edb::v1::debugger_core->flag_register()) {
					QMenu menu;
					menu.addAction(tr("&Carry"),     this, SLOT(toggle_flag_carry()));
					menu.addAction(tr("&Parity"),    this, SLOT(toggle_flag_parity()));
					menu.addAction(tr("&Auxiliary"), this, SLOT(toggle_flag_auxiliary()));
					menu.addAction(tr("&Zero"),      this, SLOT(toggle_flag_zero()));
					menu.addAction(tr("&Sign"),      this, SLOT(toggle_flag_sign()));
					menu.addAction(tr("&Direction"), this, SLOT(toggle_flag_direction()));
					menu.addAction(tr("&Overflow"),  this, SLOT(toggle_flag_overflow()));

					add_plugin_context_menu(&menu, &IPlugin::register_context_menu);

					menu.exec(ui.registerList->mapToGlobal(pos));
				}
			}
		}
	}
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
		if(IThread::pointer thread = process->current_thread()) {	
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
// Name: on_registerList_itemDoubleClicked
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_registerList_itemDoubleClicked(QTreeWidgetItem *item) {
	Q_ASSERT(item);
	edb::v1::arch_processor().edit_item(*item);
	update_gui();
	refresh_gui();
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
	for(const DataViewInfo::pointer &data_view: data_regions_) {
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
		if(IThread::pointer thread = process->current_thread()) {
			State state;
			thread->get_state(&state);

			const edb::address_t ip = state.instruction_pointer();
			quint8 buffer[edb::Instruction::MAX_SIZE];
			if(const int sz = edb::v1::get_instruction_bytes(ip, buffer)) {
				edb::Instruction inst(buffer, buffer + sz, 0);
				if(inst && edb::v1::arch_processor().can_step_over(inst)) {

					// add a temporary breakpoint at the instruction just
					// after the call
					if(IBreakpoint::pointer bp = edb::v1::debugger_core->add_breakpoint(ip + inst.size())) {
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
		QMessageBox::information(this,
			tr("No Memory Found"),
			tr("There appears to be no memory at that location (<strong>%1</strong>)").arg(edb::v1::format_pointer(address)));
	}
}

//------------------------------------------------------------------------------
// Name: follow_register_in_dump
// Desc:
//------------------------------------------------------------------------------
void Debugger::follow_register_in_dump(bool tabbed) {
	bool ok;
	const edb::address_t address = get_follow_register(&ok);
	if(ok && !edb::v1::dump_data(address, tabbed)) {
		QMessageBox::information(this,
			tr("No Memory Found"),
			tr("There appears to be no memory at that location (<strong>%1</strong>)").arg(edb::v1::format_pointer(address)));
	}
}

//------------------------------------------------------------------------------
// Name: mnuStackGotoESP
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuStackGotoESP() {
	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(IThread::pointer thread = process->current_thread()) {
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
		if(IThread::pointer thread = process->current_thread()) {
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
		if(IThread::pointer thread = process->current_thread()) {
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
	bool ok;
	const edb::address_t address = get_goto_expression(&ok);
	if(ok) {
		follow_memory(address, [](edb::address_t address) {
			return edb::v1::jump_to_address(address);
		});
	}
}

//------------------------------------------------------------------------------
// Name: mnuDumpGotoAddress
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuDumpGotoAddress() {
    bool ok;
	const edb::address_t address = get_goto_expression(&ok);
	if(ok) {
		follow_memory(address, [](edb::address_t address) {
			return edb::v1::dump_data(address);
		});
	}
}

//------------------------------------------------------------------------------
// Name: mnuStackGotoAddress
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuStackGotoAddress() {
    bool ok;
	const edb::address_t address = get_goto_expression(&ok);
	if(ok) {
		follow_memory(address, [](edb::address_t address) {
			return edb::v1::dump_stack(address);
		});
	}
}

//------------------------------------------------------------------------------
// Name: mnuRegisterFollowInStack
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuRegisterFollowInStack() {
	bool ok;
	const edb::address_t address = get_follow_register(&ok);
	if(ok) {
		follow_memory(address, [](edb::address_t address) {
			return edb::v1::dump_stack(address);
		});
	}
}


//------------------------------------------------------------------------------
// Name: get_follow_address
// Desc:
//------------------------------------------------------------------------------
template <class T>
edb::address_t Debugger::get_follow_address(const T &hexview, bool *ok) {

	Q_ASSERT(hexview);
	Q_ASSERT(ok);

	*ok = false;

	const size_t pointer_size = edb::v1::pointer_size();

	if(hexview->hasSelectedText()) {
		const QByteArray data = hexview->selectedBytes();

		if(data.size() == edb::v1::pointer_size()) {
			edb::address_t d(0);
			std::memcpy(&d, data.data(), pointer_size);

			*ok = true;
			return d;
		}
	}

	QMessageBox::information(this,
		tr("Invalid Selection"),
		tr("Please select %1 bytes to use this function.").arg(pointer_size));

	return 0;
}

//------------------------------------------------------------------------------
// Name: follow_in_stack
// Desc:
//------------------------------------------------------------------------------
template <class T>
void Debugger::follow_in_stack(const T &hexview) {
	bool ok;
	const edb::address_t address = get_follow_address(hexview, &ok);
	if(ok) {
		follow_memory(address, [](edb::address_t address) {
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
	bool ok;
	const edb::address_t address = get_follow_address(hexview, &ok);
	if(ok) {
		follow_memory(address, [](edb::address_t address) {
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
	bool ok;
	const edb::address_t address = get_follow_address(hexview, &ok);
	if(ok) {
		follow_memory(address, [](edb::address_t address) {
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
		if(IThread::pointer thread = process->current_thread()) {
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
		if(IThread::pointer thread = process->current_thread()) {
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

	menu.addAction(editCommentAction_);
	menu.addAction(removeCommentAction_);
	menu.addSeparator();

	menu.addAction(setAddressLabelAction_);
	menu.addSeparator();

	menu.addAction(gotoAddressAction_);
	menu.addAction(gotoRIPAction_);

	const edb::address_t address = ui.cpuView->selectedAddress();
	int size                     = ui.cpuView->selectedSize();


	if(IProcess *process = edb::v1::debugger_core->process()) {

		Q_UNUSED(process);

		quint8 buffer[edb::Instruction::MAX_SIZE + 1];
		if(edb::v1::get_instruction_bytes(address, buffer, &size)) {
			edb::Instruction inst(buffer, buffer + size, address);
			if(inst) {


				if(is_call(inst) || is_jump(inst)) {
					if(inst.operands()[0].general_type() == edb::Operand::TYPE_REL) {
						menu.addAction(followAction_);
						followAction_->setData(static_cast<qlonglong>(inst.operands()[0].relative_target()));
					}

					/*
					if(inst.operands()[0].general_type() == edb::Operand::TYPE_EXPRESSION) {
						if(inst.operands()[0].expression().base == edb::Operand::REG_RIP && inst.operands()[0].expression().index == edb::Operand::REG_NULL && inst.operands()[0].expression().scale == 1) {
							menu.addAction(followAction_);
							followAction_->setData(static_cast<qlonglong>(address + inst.operands()[0].displacement()));
						}
					}
					*/
				} else {
					for(std::size_t i = 0; i < inst.operand_count(); ++i) {
						if(inst.operands()[i].general_type() == edb::Operand::TYPE_IMMEDIATE) {
							menu.addAction(followConstantInDumpAction_);
							menu.addAction(followConstantInStackAction_);
							
							followConstantInDumpAction_->setData(static_cast<qlonglong>(inst.operands()[i].immediate()));
							followConstantInStackAction_->setData(static_cast<qlonglong>(inst.operands()[i].immediate()));
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
	menu.addAction(editBytesAction_);
	menu.addAction(fillWithZerosAction_);
	menu.addAction(fillWithNOPsAction_);
	menu.addSeparator();
	menu.addAction(toggleBreakpointAction_);
	menu.addAction(conditionalBreakpointAction_);
	menu.addAction(removeBreakpointAction_);

	add_plugin_context_menu(&menu, &IPlugin::cpu_context_menu);

	menu.exec(ui.cpuView->viewport()->mapToGlobal(pos));
}

//------------------------------------------------------------------------------
// Name: mnuCPUFollow
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuCPUFollow() {
	if(auto action = qobject_cast<QAction *>(sender())) {
		const edb::address_t address = action->data().toULongLong();
		follow_memory(address, edb::v1::jump_to_address);
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
	menu->addAction(stackEditBytesAction_);
	menu->addSeparator();
	menu->addAction(stackPushAction_);
	menu->addAction(stackPopAction_);

	// lockable stack feature
	menu->addSeparator();
	auto action = new QAction(tr("&Lock Stack"), this);
    action->setCheckable(true);
    action->setChecked(stack_view_locked_);
	menu->addAction(action);
	connect(action, SIGNAL(toggled(bool)), SLOT(mnuStackToggleLock(bool)));

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
	menu->addAction(dumpEditBytesAction_);
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
				QString("Edit Comment"),
				QString("Comment:"),
				QLineEdit::Normal,
				ui.cpuView->get_comment(address),
				&got_text);

	//If we got a comment, add it.
	if (got_text && !comment.isEmpty()) {
		ui.cpuView->add_comment(address, comment);
	}

	//If the user backspaced the comment, remove the comment since
	//there's no need for a null string to take space in the hash.
	else if (got_text && comment.isEmpty()) {
		ui.cpuView->remove_comment(address);
	}

	//The only other real case is that we didn't got_text.  No change.
	else {
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
void Debugger::run_to_this_line(bool pass_signal) {
	const edb::address_t address = ui.cpuView->selectedAddress();
	IBreakpoint::pointer bp = edb::v1::find_breakpoint(address);
	if(!bp) {
		bp = edb::v1::create_breakpoint(address);
		if(!bp) return;
		bp->set_one_time(true);
		bp->set_internal(true);
		bp->tag = run_to_cursor_tag;
	}
    if(pass_signal)
		resume_execution(PASS_EXCEPTION, MODE_RUN, false);
	else
		resume_execution(IGNORE_EXCEPTION, MODE_RUN, false);
}

//------------------------------------------------------------------------------
// Name: mnuCPURunToThisLinePassSignal
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuCPURunToThisLinePassSignal() {
	run_to_this_line(true);
}

//------------------------------------------------------------------------------
// Name: mnuCPURunToThisLine
// Desc:
//------------------------------------------------------------------------------
void Debugger::mnuCPURunToThisLine() {
	run_to_this_line(false);
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
	if(ok) {
		edb::v1::create_breakpoint(address);
		if(!condition.isEmpty()) {
			edb::v1::set_breakpoint_condition(address, condition);
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
	// TODO: get system independent nop-code
	cpu_fill(0x90);
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
		if(IThread::pointer thread = process->current_thread()) {
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
			if(edb::v1::get_binary_string_from_user(bytes, QT_TRANSLATE_NOOP("edb", "Edit Binary String"), size)) {
				edb::v1::modify_bytes(address, size, bytes, 0x00);
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
		const unsigned int size      = hexview->selectedBytesSize();
		QByteArray bytes             = hexview->selectedBytes();

		if(edb::v1::get_binary_string_from_user(bytes, QT_TRANSLATE_NOOP("edb", "Edit Binary String"), size)) {
			edb::v1::modify_bytes(address, size, bytes, 0x00);
		}
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

	edb::address_t condition_value;
	if(!edb::v1::eval_expression(condition, &condition_value)) {
		return true;
	}
	return condition_value;
}

//------------------------------------------------------------------------------
// Name: handle_trap
// Desc: returns true if we should resume as if this trap never happened
//------------------------------------------------------------------------------
edb::EVENT_STATUS Debugger::handle_trap() {

	// we just got a trap event, there are a few possible causes
	// #1: we hit a 0xcc breakpoint, if so, then we want to stop
	// #2: we did a step
	// #3: we hit a 0xcc naturally in the program
	// #4: we hit a hardware breakpoint!
	State state;
	edb::v1::debugger_core->get_state(&state);

	const edb::address_t previous_ip = state.instruction_pointer() - 1;

	// look it up in our breakpoint list, make sure it is one of OUR int3s!
	// if it is, we need to backup EIP and pause ourselves
	IBreakpoint::pointer bp = edb::v1::find_breakpoint(previous_ip);
	if(bp && bp->enabled()) {

		// TODO: check if the breakpoint was corrupted
		bp->hit();

		// back up eip the size of a breakpoint, since we executed a breakpoint
		// instead of the real code that belongs there
		state.set_instruction_pointer(previous_ip);
		edb::v1::debugger_core->set_state(state);

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
edb::EVENT_STATUS Debugger::handle_event_stopped(const IDebugEvent::const_pointer &event) {

	// ok we just came in from a stop, we need to test some things,
	// generally, we will want to check if it was a step, if it was, was it
	// because we just hit a break point or because we wanted to run, but had
	// to step first in case were were on a breakpoint already...


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
		QMessageBox::information(this, message.caption, message.message);
		return edb::DEBUG_STOP;
	}

	if(event->is_trap()) {
		return handle_trap();
	}

	if(event->is_stop()) {
		// user asked to pause the debugged process
		return edb::DEBUG_STOP;
	}

	switch(event->code()) {
#ifdef Q_OS_UNIX
	case SIGCHLD:
	case SIGPROF:
		return edb::DEBUG_EXCEPTION_NOT_HANDLED;
#endif
	default:
		{
			Q_ASSERT(edb::v1::debugger_core);
			QMap<long, QString> known_exceptions = edb::v1::debugger_core->exceptions();
			auto it = known_exceptions.find(event->code());
			QString exception_name;
			if(it != known_exceptions.end()) {
				exception_name = it.value();

				QMessageBox::information(this, tr("Debug Event"),
					tr(
					"<p>The debugged application has received a debug event-> <strong>%1 (%2)</strong></p>"
					"<p>If you would like to pass this event to the application press Shift+[F7/F8/F9]</p>"
					"<p>If you would like to ignore this event, press [F7/F8/F9]</p>").arg(event->code()).arg(exception_name));
			} else {
				QMessageBox::information(this, tr("Debug Event"),
					tr(
					"<p>The debugged application has received a debug event-> <strong>%1</strong></p>"
					"<p>If you would like to pass this event to the application press Shift+[F7/F8/F9]</p>"
					"<p>If you would like to ignore this event, press [F7/F8/F9]</p>").arg(event->code()));
			}

			return edb::DEBUG_STOP;
		}
	}
}

//------------------------------------------------------------------------------
// Name: handle_event_terminated
// Desc:
//------------------------------------------------------------------------------
edb::EVENT_STATUS Debugger::handle_event_terminated(const IDebugEvent::const_pointer &event) {
	QMessageBox::information(
		this,
		tr("Application Terminated"),
		tr("The debugged application was terminated with exit code %1.").arg(event->code()));

	on_action_Detach_triggered();
	return edb::DEBUG_STOP;
}

//------------------------------------------------------------------------------
// Name: handle_event_exited
// Desc:
//------------------------------------------------------------------------------
edb::EVENT_STATUS Debugger::handle_event_exited(const IDebugEvent::const_pointer &event) {
	QMessageBox::information(
		this,
		tr("Application Exited"),
		tr("The debugged application exited normally with exit code %1.").arg(event->code()));

	on_action_Detach_triggered();
	return edb::DEBUG_STOP;
}

//------------------------------------------------------------------------------
// Name: handle_event
// Desc:
//------------------------------------------------------------------------------
edb::EVENT_STATUS Debugger::handle_event(const IDebugEvent::const_pointer &event) {

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
// Name: debug_event_handler
// Desc:
//------------------------------------------------------------------------------
edb::EVENT_STATUS Debugger::debug_event_handler(const IDebugEvent::const_pointer &event) {
	IDebugEventHandler *const handler = edb::v1::debug_event_handler();
	Q_ASSERT(handler);
	return handler->handle_event(event);
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
void Debugger::update_data(const DataViewInfo::pointer &v) {

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
void Debugger::clear_data(const DataViewInfo::pointer &v) {

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
void Debugger::do_jump_to_address(edb::address_t address, const IRegion::pointer &r, bool scrollTo) {

	ui.cpuView->setAddressOffset(r->start());
	ui.cpuView->setRegion(r);
	if(scrollTo && !ui.cpuView->addressShown(address)) {
		ui.cpuView->scrollTo(address);
	}
}

//------------------------------------------------------------------------------
// Name: update_disassembly
// Desc:
//------------------------------------------------------------------------------
void Debugger::update_disassembly(edb::address_t address, const IRegion::pointer &r) {
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
IRegion::pointer Debugger::update_cpu_view(const State &state) {

	const edb::address_t address = state.instruction_pointer();

	if(IRegion::pointer region = edb::v1::memory_regions().find_region(address)) {
		update_disassembly(address, region);
		return region;
	} else {
		ui.cpuView->clear();
		ui.cpuView->scrollTo(0);
		list_model_->setStringList(QStringList());
		return IRegion::pointer();
	}
}

//------------------------------------------------------------------------------
// Name: update_data_views
// Desc:
//------------------------------------------------------------------------------
void Debugger::update_data_views() {

	// update all data views with the current region data
	for(const DataViewInfo::pointer &info: data_regions_) {

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

	for(const DataViewInfo::pointer &info: data_regions_) {
		info->view->update();
	}

	if(edb::v1::debugger_core) {
		State state;
				
		if(IProcess *process = edb::v1::debugger_core->process()) {
			if(IThread::pointer thread = process->current_thread()) {					
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
			if(IThread::pointer thread = process->current_thread()) {				
				thread->get_state(&state);
			}
		}

		update_data_views();
		update_stack_view(state);

		if(const IRegion::pointer region = update_cpu_view(state)) {
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
void Debugger::resume_execution(EXCEPTION_RESUME pass_exception, DEBUG_MODE mode, bool forced) {

	Q_ASSERT(edb::v1::debugger_core);
	
	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(IThread::pointer thread = process->current_thread()) {	

			// if necessary pass the trap to the application, otherwise just resume
			// as normal
			const edb::EVENT_STATUS status = resume_status(pass_exception == PASS_EXCEPTION);

			// if we are on a breakpoint, disable it
			IBreakpoint::pointer bp;
			if(!forced) {
				State state;
				thread->get_state(&state);			
				bp = edb::v1::debugger_core->find_breakpoint(state.instruction_pointer());
				if(bp) {
					bp->disable();
				}
			}

			if(mode == MODE_STEP) {
				reenable_breakpoint_step_ = bp;
				thread->step(status);
			} else if(mode == MODE_RUN) {
				reenable_breakpoint_run_ = bp;
				if(bp) {
					thread->step(status);
				} else {
					process->resume(status);
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
	resume_execution(PASS_EXCEPTION, MODE_RUN, false);
}

//------------------------------------------------------------------------------
// Name: on_action_Step_Into_Pass_Signal_To_Application_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Step_Into_Pass_Signal_To_Application_triggered() {
	resume_execution(PASS_EXCEPTION, MODE_STEP, false);
}

//------------------------------------------------------------------------------
// Name: on_action_Run_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Run_triggered() {
	resume_execution(IGNORE_EXCEPTION, MODE_RUN, false);
}

//------------------------------------------------------------------------------
// Name: on_action_Step_Into_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Step_Into_triggered() {
	resume_execution(IGNORE_EXCEPTION, MODE_STEP, false);
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
	step_over(
		std::bind(&Debugger::on_action_Run_Pass_Signal_To_Application_triggered, this),
		std::bind(&Debugger::on_action_Step_Into_Pass_Signal_To_Application_triggered, this));
}

//------------------------------------------------------------------------------
// Name: on_action_Step_Over_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Step_Over_triggered() {
	step_over(
		std::bind(&Debugger::on_action_Run_triggered, this),
		std::bind(&Debugger::on_action_Step_Into_triggered, this));
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
	step_over(
				std::bind(&Debugger::on_action_Run_triggered, this),
				std::bind(&Debugger::on_action_Step_Into_triggered, this));
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
	data_regions_.first()->region = IRegion::pointer();

	setWindowTitle(tr("edb"));

	update_gui();
}

//------------------------------------------------------------------------------
// Name: session_filename
// Desc:
//------------------------------------------------------------------------------
QString Debugger::session_filename() const {

	QString session_path = edb::v1::config().session_path;
	if(session_path.isEmpty()) {
		qDebug() << "No session path specified, Using current working directory.";
		session_path = QDir().absolutePath();
	}

	if(!program_executable_.isEmpty()) {
		const QFileInfo info(program_executable_);
		return QString(QLatin1String("%1/%2.edb")).arg(session_path, info.fileName());
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
		save_session(filename);
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

#ifdef Q_OS_UNIX
	debug_pointer_ = 0;
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
		load_session(filename);
	}

	// create our binary info object for the primary code module
	binary_info_ = edb::v1::get_binary_info(edb::v1::primary_code_region());

	stack_comment_server_->clear();
	if(binary_info_) {
		stack_comment_server_->set_comment(binary_info_->entry_point(), "<entry point>");
	}
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
		const Symbol::pointer sym = edb::v1::symbol_manager().find(mainSymbol);

		if(sym) {
			entryPoint = sym->address;
		} else if(edb::v1::config().find_main) {
			entryPoint = edb::v1::locate_main_function();
		}
	}

	if(entryPoint == 0 || edb::v1::config().initial_breakpoint == Configuration::EntryPoint) {
		if(binary_info_) {
			entryPoint = binary_info_->entry_point();
		}
	}

	if(entryPoint != 0) {
		if(IBreakpoint::pointer bp = edb::v1::debugger_core->add_breakpoint(entryPoint)) {
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
	Q_ASSERT(edb::v1::debugger_core->process());

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
}

//------------------------------------------------------------------------------
// Name: setup_data_views
// Desc:
//------------------------------------------------------------------------------
void Debugger::setup_data_views() {

	// Setup data views according to debuggee bitness
	if(edb::v1::debuggeeIs64Bit()) {
		stack_view_->setAddressSize(QHexView::Address64);
		for(const DataViewInfo::pointer &data_view: data_regions_) {
			data_view->view->setAddressSize(QHexView::Address64);
		}
	} else {
		stack_view_->setAddressSize(QHexView::Address32);
		for(const DataViewInfo::pointer &data_view: data_regions_) {
			data_view->view->setAddressSize(QHexView::Address32);
		}
	}

	// Update stack word width
	if(auto_stack_word_width_)
		stack_word_width_=edb::v1::pointer_size();
	stack_view_->setWordWidth(stack_word_width_);
}

//------------------------------------------------------------------------------
// Name: common_open
// Desc:
//------------------------------------------------------------------------------
bool Debugger::common_open(const QString &s, const QList<QByteArray> &args) {

	bool ret = false;
	if(!QFile(s).exists()) {
		QMessageBox::information(
			this,
			tr("Could Not Open"),
			tr("The specified file (%1) does not appear to exist, please check privileges and try again.").arg(s));
	} else {

		tty_file_ = create_tty();		

		if(edb::v1::debugger_core->open(s, working_directory_, args, tty_file_)) {			
			attachComplete();			
			set_initial_breakpoint(s);
			ret = true;
		} else {
			QMessageBox::information(
				this,
				tr("Could Not Open"),
				tr("Failed to open and attach to process, please check privileges and try again."));
		}
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
		recent_file_manager_->add_file(program);
		arguments_dialog_->set_arguments(args);
	}
}

//------------------------------------------------------------------------------
// Name: open_file
// Desc:
//------------------------------------------------------------------------------
void Debugger::open_file(const QString &s) {
	if(!s.isEmpty()) {
		last_open_directory_ = QFileInfo(s).canonicalFilePath();

		detach_from_process(NO_KILL_ON_DETACH);

		execute(s, arguments_dialog_->arguments());
	}
}

//------------------------------------------------------------------------------
// Name: attach
// Desc:
//------------------------------------------------------------------------------
void Debugger::attach(edb::pid_t pid) {

	// TODO: we need a core concept of debugger capabilities which
	// may restrict some actions

#if defined(Q_OS_UNIX)
	edb::pid_t current_pid = getpid();
	while(current_pid != 0) {
		if(current_pid == pid) {
			QMessageBox::information(
				this,
				tr("Attach"),
				tr("You may not debug a process which is a parent of the edb process."));
			return;
		}
		current_pid = edb::v1::debugger_core->parent_pid(current_pid);
	}
#endif

	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(pid == process->pid()) {
			QMessageBox::information(
				this,
				tr("Attach"),
				tr("You are already debugging that process!"));
			return;
		}
	}


	detach_from_process(NO_KILL_ON_DETACH);
	
	if(edb::v1::debugger_core->attach(pid)) {

		working_directory_ = edb::v1::debugger_core->process()->current_working_directory();

		QList<QByteArray> args = edb::v1::debugger_core->process()->arguments();

		if(!args.empty()) {
			args.removeFirst();
		}

		arguments_dialog_->set_arguments(args);
		attachComplete();
	} else {
		QMessageBox::information(this, tr("Attach"), tr("Failed to attach to process, please check privileges and try again."));
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
	
	CapstoneEDB::init(edb::v1::debuggeeIs64Bit());
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
}

//------------------------------------------------------------------------------
// Name: on_action_Open_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Open_triggered() {

	// TODO: we need a core concept of debugger capabilities which
	// may restrict some actions

	static auto* dialog = new DialogOpenProgram(this,
												tr("Choose a file"),
												last_open_directory_);
	if(dialog->exec()==QDialog::Accepted) {

		arguments_dialog_->set_arguments(dialog->arguments());
		const QString filename = dialog->selectedFiles().front();
		working_directory_ = dialog->workingDirectory();
		open_file(filename);
	}
}

//------------------------------------------------------------------------------
// Name: on_action_Attach_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Attach_triggered() {

	// TODO: we need a core concept of debugger capabilities which
	// may restrict some actions

	QPointer<DialogAttach> dlg = new DialogAttach(this);

	if(dlg->exec() == QDialog::Accepted) {
		if(dlg) {
			bool ok;
			const edb::pid_t pid = dlg->selected_pid(&ok);
			if(ok) {
				attach(pid);
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

	// TODO: we need a core concept of debugger capabilities which
	// may restrict some actions

	static QPointer<DialogMemoryRegions> dlg = new DialogMemoryRegions(this);
	dlg->show();
}


//------------------------------------------------------------------------------
// Name: on_action_Threads_triggered
// Desc:
//------------------------------------------------------------------------------
void Debugger::on_action_Threads_triggered() {

	// TODO: we need a core concept of debugger capabilities which
	// may restrict some actions
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

	if(IRegion::pointer region = edb::v1::memory_regions().find_region(address)) {
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

	if(IRegion::pointer region = edb::v1::memory_regions().find_region(address)) {
		if(new_tab) {
			mnuDumpCreateTab();
		}

		if(DataViewInfo::pointer info = current_data_view_info()) {
			info->region = IRegion::pointer(region->clone());

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

	if(IRegion::pointer region = edb::v1::memory_regions().find_region(address)) {
		if(new_tab) {
			mnuDumpCreateTab();
		}

		DataViewInfo::pointer info = current_data_view_info();

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
	const IRegion::pointer last_region = stack_view_info_.region;
	stack_view_info_.region = edb::v1::memory_regions().find_region(address);



	if(stack_view_info_.region) {
		stack_view_info_.update();

		if(IProcess *process = edb::v1::debugger_core->process()) {
			if(IThread::pointer thread = process->current_thread()) {

				State state;
				thread->get_state(&state);
				stack_view_->setColdZoneEnd(state.stack_pointer());

				if(scroll_to || stack_view_info_.region->compare(last_region)) {
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

	if(IDebugEvent::const_pointer e = edb::v1::debugger_core->wait_debug_event(10)) {

		last_event_ = e;

		// TODO(eteran): figure out a way to do this less often, if they map an obscene
		// number of regions, this really slows things down
		edb::v1::memory_regions().sync();

		// TODO(eteran): make the system use this information, this is huge! it will
		// allow us to have restorable breakpoints...even in libraries!
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
		if(IProcess *process = edb::v1::debugger_core->process()) {
			if(!debug_pointer_ && binary_info_) {
				if((debug_pointer_ = binary_info_->debug_pointer()) != 0) {
					r_debug dynamic_info;
					const bool ok = process->read_bytes(debug_pointer_, &dynamic_info, sizeof(dynamic_info));
					if(ok) {
					#if 0
						qDebug("READ DYNAMIC INFO! %p", dynamic_info.r_brk);
					#endif
					}
				}

			}
		}
	#if 0
		qDebug("DEBUG POINTER: %p", debug_pointer_);
	#endif
#endif

		const edb::EVENT_STATUS status = debug_event_handler(e);
		switch(status) {
		case edb::DEBUG_STOP:
			update_gui();
			update_menu_state(edb::v1::debugger_core->process() ? PAUSED : TERMINATED);
			break;
		case edb::DEBUG_CONTINUE:
			resume_execution(IGNORE_EXCEPTION, MODE_RUN, true);
			break;
		case edb::DEBUG_CONTINUE_BP:
			resume_execution(IGNORE_EXCEPTION, MODE_RUN, false);
			break;
		case edb::DEBUG_CONTINUE_STEP:
			resume_execution(IGNORE_EXCEPTION, MODE_STEP, true);
			break;
		case edb::DEBUG_EXCEPTION_NOT_HANDLED:
			resume_execution(PASS_EXCEPTION, MODE_RUN, true);
			break;
		}
	}
}

//------------------------------------------------------------------------------
// Name: save_session
// Desc:
//------------------------------------------------------------------------------
void Debugger::save_session(const QString &session_file) {
	Q_UNUSED(session_file);
}

//------------------------------------------------------------------------------
// Name: load_session
// Desc:
//------------------------------------------------------------------------------
void Debugger::load_session(const QString &session_file) {
	Q_UNUSED(session_file);
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

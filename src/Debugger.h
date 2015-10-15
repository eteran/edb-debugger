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

#ifndef DEBUGGERMAIN_20090811_H_
#define DEBUGGERMAIN_20090811_H_

#include "DataViewInfo.h"
#include "Debugger.h"
#include "IDebugEventHandler.h"
#include "QHexView"
#include "edb.h"

class DialogArguments;
class IBinary;
class IBreakpoint;
class IDebugEvent;
class IPlugin;
class RecentFileManager;

class QStringListModel;
class QTimer;
class QToolButton;
class QTreeWidgetItem;
class QToolButton;
class QDragEnterEvent;
class QDropEvent;
class QLabel;

#include <QMainWindow>
#include <QProcess>
#include <QVector>

#include <cstring>
#include <memory>

#include "ui_Debugger.h"

class Debugger : public QMainWindow, public IDebugEventHandler {
	Q_OBJECT
	Q_DISABLE_COPY(Debugger)

private:
	friend class RunUntilRet;

public:
	Debugger(QWidget *parent = 0);
	virtual ~Debugger();

private:
	enum DEBUG_MODE {
		MODE_STEP,
		MODE_TRACE,
		MODE_RUN
	};

	enum EXCEPTION_RESUME {
		IGNORE_EXCEPTION,
		PASS_EXCEPTION
	};

	enum DETACH_ACTION {
		NO_KILL_ON_DETACH,
		KILL_ON_DETACH
	};

	enum GUI_STATE {
		PAUSED,
		RUNNING,
		TERMINATED
	};

public:
	DataViewInfo::pointer current_data_view_info() const;
	bool dump_data(edb::address_t address, bool new_tab);
	bool dump_data_range(edb::address_t address, edb::address_t end_address, bool new_tab);
	bool dump_stack(edb::address_t address, bool scroll_to);
	bool jump_to_address(edb::address_t address);
	int current_tab() const;
	void attach(edb::pid_t pid);
	void clear_data(const DataViewInfo::pointer &v);
	void execute(const QString &s, const QList<QByteArray> &args);
	void refresh_gui();
	void update_data(const DataViewInfo::pointer &v);
	void update_gui();
	QLabel *statusLabel() const;

Q_SIGNALS:
	void gui_updated();

public Q_SLOTS:
	// the autoconnected slots
	void on_action_Help_triggered();
	void on_actionAbout_QT_triggered();
	void on_actionApplication_Arguments_triggered();
	void on_actionApplication_Working_Directory_triggered();
	void on_actionRun_Until_Return_triggered();
	void on_action_About_triggered();
	void on_action_Attach_triggered();
	void on_action_Configure_Debugger_triggered();
	void on_action_Detach_triggered();
	void on_action_Kill_triggered();
	void on_action_Memory_Regions_triggered();
	void on_action_Open_triggered();
	void on_action_Pause_triggered();
	void on_action_Plugins_triggered();
	void on_action_Restart_triggered();
	void on_action_Run_Pass_Signal_To_Application_triggered();
	void on_action_Run_triggered();
	void on_action_Step_Into_Pass_Signal_To_Application_triggered();
	void on_action_Step_Into_triggered();
	void on_action_Step_Over_Pass_Signal_To_Application_triggered();
	void on_action_Step_Over_triggered();
	void on_actionStep_Out_triggered();
	void on_action_Threads_triggered();
	void on_cpuView_breakPointToggled(edb::address_t);
	void on_cpuView_customContextMenuRequested(const QPoint &);
	void on_registerList_customContextMenuRequested(const QPoint &);
	void on_registerList_itemDoubleClicked(QTreeWidgetItem *);

//Flag-toggling slots for right-click --> toggle flag
public Q_SLOTS:
	void toggle_flag_carry();
	void toggle_flag_parity();
	void toggle_flag_auxiliary();
	void toggle_flag_zero();
	void toggle_flag_sign();
	void toggle_flag_direction();
	void toggle_flag_overflow();

private:
	void toggle_flag(int);

	void run_to_this_line(bool pass_signal);
private Q_SLOTS:
	// the manually connected CPU slots
	void mnuCPUEditComment();
	void mnuCPURemoveComment();
	void mnuCPURunToThisLine();
	void mnuCPURunToThisLinePassSignal();
	void mnuCPUToggleBreakpoint();
	void mnuCPUAddConditionalBreakpoint();
	void mnuCPUFillNop();
	void mnuCPUFillZero();
	void mnuCPUFollow();
	void mnuCPUFollowInDump();
	void mnuCPUFollowInStack();
	void mnuCPUJumpToAddress();
	void mnuCPUJumpToEIP();
	void mnuCPUModify();
	void mnuCPURemoveBreakpoint();
	void mnuCPUSetEIP();
	void mnuCPULabelAddress();

private Q_SLOTS:
	// the manually connected Register slots
	void mnuRegisterFollowInDump()       { follow_register_in_dump(false); }
	void mnuRegisterFollowInDumpNewTab() { follow_register_in_dump(true); }
	void mnuRegisterFollowInStack();

private Q_SLOTS:
	// the manually connected Dump slots
	void mnuDumpContextMenu(const QPoint &pos);
	void mnuDumpCreateTab();
	void mnuDumpDeleteTab();
	void mnuDumpFollowInCPU();
	void mnuDumpFollowInDump();
	void mnuDumpFollowInStack();
	void mnuDumpGotoAddress();
	void mnuDumpModify();
	void mnuDumpSaveToFile();

private Q_SLOTS:
	// the manually connected Stack slots
	void mnuStackContextMenu(const QPoint &);
	void mnuStackFollowInCPU();
	void mnuStackFollowInDump();
	void mnuStackFollowInStack();
	void mnuStackGotoAddress();
	void mnuStackGotoEBP();
	void mnuStackGotoESP();
	void mnuStackModify();
	void mnuStackPop();
	void mnuStackPush();
	void mnuStackToggleLock(bool locked);

private Q_SLOTS:
	void goto_triggered();
	void next_debug_event();
	void open_file(const QString &s);
	void tab_context_menu(int index, const QPoint &pos);
	void tty_proc_finished(int exit_code, QProcess::ExitStatus exit_status);

private:
	virtual void closeEvent(QCloseEvent *event);
	virtual void showEvent(QShowEvent *event);
	virtual void dragEnterEvent(QDragEnterEvent* event);
	virtual void dropEvent(QDropEvent* event);

public:
	virtual edb::EVENT_STATUS handle_event(const IDebugEvent::const_pointer &event);

private:
	IRegion::pointer update_cpu_view(const State &state);
	QString create_tty();
	QString session_filename() const;
	bool breakpoint_condition_true(const QString &condition);
	bool common_open(const QString &s, const QList<QByteArray> &args);
	edb::EVENT_STATUS debug_event_handler(const IDebugEvent::const_pointer &event);
	edb::EVENT_STATUS handle_event_exited(const IDebugEvent::const_pointer &event);
	edb::EVENT_STATUS handle_event_stopped(const IDebugEvent::const_pointer &event);
	edb::EVENT_STATUS handle_event_terminated(const IDebugEvent::const_pointer &event);
	edb::EVENT_STATUS handle_trap();
	edb::EVENT_STATUS resume_status(bool pass_exception);
	edb::address_t get_goto_expression(bool *ok);
	edb::reg_t get_follow_register(bool *ok) const;
	void apply_default_fonts();
	void apply_default_show_separator();
	void cleanup_debugger();
	void cpu_fill(quint8 byte);
	void create_data_tab();
	void delete_data_tab();
	void detach_from_process(DETACH_ACTION kill);
	void do_jump_to_address(edb::address_t address, const IRegion::pointer &r, bool scroll_to);
	void finish_plugin_setup();
	void follow_register_in_dump(bool tabbed);
	void load_session(const QString &session_file);
	void resume_execution(EXCEPTION_RESUME pass_exception, DEBUG_MODE mode, bool forced);
	void save_session(const QString &session_file);
	void set_debugger_caption(const QString &appname);
	void set_initial_breakpoint(const QString &s);
	void set_initial_debugger_state();
	void setup_stack_view();
	void setup_tab_buttons();
	void setup_ui();
	void test_native_binary();
	void setup_data_views();
	void update_data_views();
	void update_disassembly(edb::address_t address, const IRegion::pointer &r);
	void update_menu_state(GUI_STATE state);
	void update_stack_view(const State &state);
	void update_tab_caption(const std::shared_ptr<QHexView> &view, edb::address_t start, edb::address_t end);
	QAction *createAction(const QString &text, const QKeySequence &keySequence, const char *slot);
	void attachComplete();

private:
	template <class F>
	void follow_memory(edb::address_t address, F follow_func);

	template <class T>
	edb::address_t get_follow_address(const T &hexview, bool *ok);

	template <class F, class T>
	void add_plugin_context_menu(const T &menu, const F &f);

	template <class T>
	void follow_in_cpu(const T &hexview);

	template <class T>
	void follow_in_dump(const T &hexview);

	template <class T>
	void follow_in_stack(const T &hexview);

	template <class T>
	void modify_bytes(const T &hexview);

	template <class F1, class F2>
	void step_over(F1 run_func, F2 step_func);

public:
	Ui::Debugger ui;

private:
	QToolButton *                                    add_tab_;
	QToolButton *                                    del_tab_;
	QProcess *                                       tty_proc_;
	GUI_STATE                                        gui_state_;
	QString                                          tty_file_;
	QVector<DataViewInfo::pointer>                   data_regions_;
	DataViewInfo                                     stack_view_info_;
	std::shared_ptr<QHexView>                         stack_view_;
	QStringListModel *                               list_model_;
	DialogArguments *                                arguments_dialog_;
	QTimer *                                         timer_;
	RecentFileManager *                              recent_file_manager_;

	QSharedPointer<QHexView::CommentServerInterface> stack_comment_server_;
	IBreakpoint::pointer                             reenable_breakpoint_run_;
	IBreakpoint::pointer                             reenable_breakpoint_step_;
	std::unique_ptr<IBinary>                         binary_info_;

	QString                                          last_open_directory_;
	QString                                          working_directory_;
	QString                                          program_executable_;
	bool                                             stack_view_locked_;
	bool                                             auto_stack_word_width_;
	int                                              stack_word_width_;
	IDebugEvent::const_pointer                       last_event_;
	QLabel *                                         status_;
#ifdef Q_OS_UNIX
	edb::address_t                                   debug_pointer_;
#endif

private:
	QAction *gotoAddressAction_;
	QAction *editCommentAction_;
	QAction *removeCommentAction_;
	QAction *editBytesAction_;
	QAction *toggleBreakpointAction_;
	QAction *conditionalBreakpointAction_;
	QAction *runToThisLineAction_;
	QAction *runToLinePassAction_;
	QAction *fillWithZerosAction_;
	QAction *fillWithNOPsAction_;
	QAction *removeBreakpointAction_;
	QAction *setAddressLabelAction_;
	QAction *followConstantInDumpAction_;
	QAction *followConstantInStackAction_;
	QAction *followAction_;
	QAction *setRIPAction_;
	QAction *gotoRIPAction_;
	QAction *dumpFollowInCPUAction_;
	QAction *dumpFollowInDumpAction_;
	QAction *dumpFollowInStackAction_;
	QAction *dumpEditBytesAction_;
	QAction *dumpSaveToFileAction_;
	QAction *registerFollowInDumpAction_;
	QAction *registerFollowInDumpTabAction_;
	QAction *registerFollowInStackAction_;
	QAction *stackFollowInCPUAction_;
	QAction *stackFollowInDumpAction_;
	QAction *stackFollowInStackAction_;
	QAction *stackEditBytesAction_;;	
	QAction *stackGotoRSPAction_;
	QAction *stackGotoRBPAction_;
	QAction *stackPushAction_;
	QAction *stackPopAction_;
};

#endif

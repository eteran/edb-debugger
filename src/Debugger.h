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

#include "Debugger.h"
#include "DataViewInfo.h"
#include "IDebugEventHandler.h"
#include "OSTypes.h"
#include "QHexView"

template <class T, class E>
class Result;

class DialogArguments;
class IBinary;
class IBreakpoint;
class IDebugEvent;
class IPlugin;
class RecentFileManager;
class CommentServer;

class QStringListModel;
class QTimer;
class QToolButton;
class QToolButton;
class QDragEnterEvent;
class QDropEvent;
class QLabel;

#include <QMainWindow>
#include <QProcess>
#include <QVector>

#include <memory>

#include "ui_Debugger.h"

class Debugger : public QMainWindow, public IDebugEventHandler {
	Q_OBJECT
public:
    explicit Debugger(QWidget *parent = nullptr);
	Debugger(const Debugger &)            = delete;
	Debugger& operator=(const Debugger &) = delete;
	~Debugger() override;

private:

	enum ResumeFlag {
		None   = 0,
		Forced = 1,
	};
	Q_DECLARE_FLAGS(ResumeFlags, ResumeFlag)

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
	std::shared_ptr<DataViewInfo> current_data_view_info() const;
	bool dump_data(edb::address_t address, bool new_tab);
	bool dump_data_range(edb::address_t address, edb::address_t end_address, bool new_tab);
	bool dump_stack(edb::address_t address, bool scroll_to);
	bool jump_to_address(edb::address_t address);
	int current_tab() const;
	void attach(edb::pid_t pid);
	void clear_data(const std::shared_ptr<DataViewInfo> &v);
	void execute(const QString &s, const QList<QByteArray> &args);
	void refresh_gui();
	void update_data(const std::shared_ptr<DataViewInfo> &v);
	void update_gui();
	QLabel *statusLabel() const;
	Register active_register() const;

Q_SIGNALS:
	void gui_updated();

	// TODO(eteran): maybe this is better off as a single event
	//               with a type passed?
	void debugEvent();
	void detachEvent();
	void attachEvent();

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
	void run_to_this_line(EXCEPTION_RESUME pass_signal);

private Q_SLOTS:
	// the manually connected general slots
	void mnuModifyBytes();

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
	void open_file(const QString &s,const QList<QByteArray> &a);
	void tab_context_menu(int index, const QPoint &pos);
	void tty_proc_finished(int exit_code, QProcess::ExitStatus exit_status);

private:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

public:
    edb::EVENT_STATUS handle_event(const std::shared_ptr<IDebugEvent> &event) override;

private:
	std::shared_ptr<IRegion> update_cpu_view(const State &state);
	QString create_tty();
	QString session_filename() const;
	bool breakpoint_condition_true(const QString &condition);
	bool common_open(const QString &s, const QList<QByteArray> &args);
	edb::EVENT_STATUS handle_event_exited(const std::shared_ptr<IDebugEvent> &event);
	edb::EVENT_STATUS handle_event_stopped(const std::shared_ptr<IDebugEvent> &event);
	edb::EVENT_STATUS handle_event_terminated(const std::shared_ptr<IDebugEvent> &event);
	edb::EVENT_STATUS handle_trap(const std::shared_ptr<IDebugEvent> &event);
	edb::EVENT_STATUS resume_status(bool pass_exception);
	Result<edb::address_t, QString> get_goto_expression();
	Result<edb::reg_t, QString> get_follow_register() const;
	void apply_default_fonts();
	void apply_default_show_separator();
	void cleanup_debugger();
	void cpu_fill(quint8 byte);
	void create_data_tab();
	void delete_data_tab();
	void detach_from_process(DETACH_ACTION kill);
	void do_jump_to_address(edb::address_t address, const std::shared_ptr<IRegion> &r, bool scroll_to);
	void finish_plugin_setup();
	void load_session(const QString &session_file);
	void resume_execution(EXCEPTION_RESUME pass_exception, DEBUG_MODE mode, ResumeFlags flags);
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
	void update_disassembly(edb::address_t address, const std::shared_ptr<IRegion> &r);
	void update_menu_state(GUI_STATE state);
	void update_stack_view(const State &state);
	void update_tab_caption(const std::shared_ptr<QHexView> &view, edb::address_t start, edb::address_t end);
	QAction *createAction(const QString &text, const QKeySequence &keySequence, void(Debugger::*slotPtr)());
	void attachComplete();

private:
	template <class F>
	void follow_memory(edb::address_t address, F follow_func);

	template <class T>
	Result<edb::address_t, QString> get_follow_address(const T &hexview);

	template <class F>
	QList<QAction*> get_plugin_context_menu_items(const F &f) const;

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
	GUI_STATE           gui_state_           = TERMINATED;
	QProcess           *tty_proc_            = nullptr;
	DialogArguments    *arguments_dialog_    = nullptr;
	QLabel             *status_              = nullptr;
	QStringListModel   *list_model_          = nullptr;
	QTimer             *timer_               = nullptr;
	QToolButton        *add_tab_             = nullptr;
	QToolButton        *del_tab_             = nullptr;
	RecentFileManager  *recent_file_manager_ = nullptr;
	bool                stack_view_locked_   = false;

#if defined(Q_OS_LINUX)
	edb::address_t      debug_pointer_       { 0 };
	bool                dynamic_info_bp_set_ = false;
#endif

private:
	DataViewInfo                                      stack_view_info_;
	QString                                           last_open_directory_;
	QString                                           program_executable_;
	QString                                           tty_file_;
	QString                                           working_directory_;
	QVector<std::shared_ptr<DataViewInfo>>            data_regions_;
	std::shared_ptr<IBreakpoint>                      reenable_breakpoint_run_;
	std::shared_ptr<IBreakpoint>                      reenable_breakpoint_step_;
	std::shared_ptr<CommentServer>                    comment_server_;
	std::shared_ptr<QHexView>                         stack_view_;
	std::shared_ptr<const IDebugEvent>                last_event_;
	std::unique_ptr<IBinary>                          binary_info_;

private:
	QAction *gotoAddressAction_;
	QAction *editCommentAction_;
	QAction *editBytesAction_;
	QAction *toggleBreakpointAction_;
	QAction *conditionalBreakpointAction_;
	QAction *runToThisLineAction_;
	QAction *runToLinePassAction_;
	QAction *fillWithZerosAction_;
	QAction *fillWithNOPsAction_;
	QAction *setAddressLabelAction_;
	QAction *followConstantInDumpAction_;
	QAction *followConstantInStackAction_;
	QAction *followAction_;
	QAction *setRIPAction_;
	QAction *gotoRIPAction_;
	QAction *dumpFollowInCPUAction_;
	QAction *dumpFollowInDumpAction_;
	QAction *dumpFollowInStackAction_;
	QAction *dumpSaveToFileAction_;
	QAction *registerFollowInDumpAction_;
	QAction *registerFollowInDumpTabAction_;
	QAction *registerFollowInStackAction_;
	QAction *stackFollowInCPUAction_;
	QAction *stackFollowInDumpAction_;
	QAction *stackFollowInStackAction_;
	QAction *stackGotoRSPAction_;
	QAction *stackGotoRBPAction_;
	QAction *stackPushAction_;
	QAction *stackPopAction_;
};

#endif

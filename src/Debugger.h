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

#ifndef DEBUGGER_H_20090811_
#define DEBUGGER_H_20090811_

#include "DataViewInfo.h"
#include "IDebugEventHandler.h"
#include "OSTypes.h"
#include "QHexView"

#include <QMainWindow>
#include <QProcess>
#include <QVector>

#include <memory>

#include "ui_Debugger.h"

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

class Debugger : public QMainWindow, public IDebugEventHandler {
	Q_OBJECT
public:
	explicit Debugger(QWidget *parent = nullptr);
	Debugger(const Debugger &) = delete;
	Debugger &operator=(const Debugger &) = delete;
	~Debugger() override;

private:
	enum class ResumeFlag {
		None   = 0,
		Forced = 1,
	};

	enum DebugMode {
		Step,
		Trace,
		Run
	};

	enum ExceptionResume {
		IgnoreException,
		PassException
	};

	enum DetachAction {
		NoKillOnDetach,
		KillOnDetach
	};

	enum GuiState {
		Paused,
		Running,
		Terminated
	};

public:
	std::shared_ptr<DataViewInfo> currentDataViewInfo() const;
	bool dumpData(edb::address_t address, bool new_tab);
	bool dumpDataRange(edb::address_t address, edb::address_t end_address, bool new_tab);
	bool dumpStack(edb::address_t address, bool scroll_to);
	bool jumpToAddress(edb::address_t address);
	int currentTab() const;
	void attach(edb::pid_t pid);
	void clearData(const std::shared_ptr<DataViewInfo> &v);
	void execute(const QString &s, const QList<QByteArray> &args, const QString &input, const QString &output);
	void refreshUi();
	void updateData(const std::shared_ptr<DataViewInfo> &v);
	void updateUi();
	QLabel *statusLabel() const;
	Register activeRegister() const;

Q_SIGNALS:
	void uiUpdated();

	// TODO(eteran): maybe this is better off as a single event
	//               with a type passed?
	void debugEvent();
	void detachEvent();
	void attachEvent();

public Q_SLOTS:
	// the autoconnected slots
	void on_actionAbout_QT_triggered();
	void on_actionApplication_Arguments_triggered();
	void on_actionApplication_Working_Directory_triggered();
	void on_actionRun_Until_Return_triggered();
	void on_actionStep_Out_triggered();
	void on_action_About_triggered();
	void on_action_Attach_triggered();
	void on_action_Configure_Debugger_triggered();
	void on_action_Detach_triggered();
	void on_action_Help_triggered();
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
	void on_action_Threads_triggered();
	void on_cpuView_breakPointToggled(edb::address_t);
	void on_cpuView_customContextMenuRequested(const QPoint &);

private:
	void toggleFlag(int);
	void runToThisLine(ExceptionResume pass_signal);

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
	// the manually connected Register slots
	QList<QAction *> currentRegisterContextMenuItems() const;
	void mnuRegisterFollowInDump() { followRegisterInDump(false); }
	void mnuRegisterFollowInDumpNewTab() { followRegisterInDump(true); }
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
	void gotoTriggered();
	void nextDebugEvent();
	void openFile(const QString &filename, const QList<QByteArray> &args);
	void tabContextMenu(int index, const QPoint &pos);
	void ttyProcFinished(int exit_code, QProcess::ExitStatus exit_status);

private:
	void closeEvent(QCloseEvent *event) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dropEvent(QDropEvent *event) override;
	void showEvent(QShowEvent *event) override;

public:
	edb::EventStatus handleEvent(const std::shared_ptr<IDebugEvent> &event) override;

private:
	QString createTty();
	QString sessionFilename() const;
	Result<edb::address_t, QString> getGotoExpression();
	Result<edb::reg_t, QString> getFollowRegister() const;
	bool commonOpen(const QString &s, const QList<QByteArray> &args, const QString &input, const QString &output);
	bool isBreakpointConditionTrue(const QString &condition);
	edb::EventStatus handleEventExited(const std::shared_ptr<IDebugEvent> &event);
	edb::EventStatus handleEventStopped(const std::shared_ptr<IDebugEvent> &event);
	edb::EventStatus handleEventTerminated(const std::shared_ptr<IDebugEvent> &event);
	edb::EventStatus handleTrap(const std::shared_ptr<IDebugEvent> &event);
	edb::EventStatus resumeStatus(bool pass_exception);
	std::shared_ptr<IRegion> updateCpuView(const State &state);
	void applyDefaultFonts();
	void applyDefaultShowSeparator();
	void attachComplete();
	void cleanupDebugger();
	void cpuFill(uint8_t byte);
	void createDataTab();
	void deleteDataTab();
	void detachFromProcess(DetachAction kill);
	void doJumpToAddress(edb::address_t address, const std::shared_ptr<IRegion> &r, bool scroll_to);
	void finishPluginSetup();
	void followRegisterInDump(bool tabbed);
	void loadSession(const QString &session_file);
	void resumeExecution(ExceptionResume pass_exception, DebugMode mode, ResumeFlag flags);
	void saveSession(const QString &session_file);
	void setDebuggerCaption(const QString &appname);
	void setInitialBreakpoint(const QString &s);
	void setInitialDebuggerState();
	void setupDataViews();
	void setupStackView();
	void setupTabButtons();
	void setupUi();
	void testNativeBinary();
	void updateDataViews();
	void updateDisassembly(edb::address_t address, const std::shared_ptr<IRegion> &r);
	void updateMenuState(GuiState state);
	void updateStackView(const State &state);
	void updateTabCaption(const std::shared_ptr<QHexView> &view, edb::address_t start, edb::address_t end);

private:
	template <class F>
	QAction *createAction(const QString &text, const QKeySequence &keySequence, F slotPtr);

	template <class F>
	void followMemory(edb::address_t address, F follow_func);

	template <class Ptr>
	Result<edb::address_t, QString> getFollowAddress(const Ptr &hexview);

	template <class F>
	QList<QAction *> getPluginContextMenuItems(const F &f) const;

	template <class F, class T>
	void addPluginContextMenu(const T &menu, const F &f);

	template <class Ptr>
	void followInCpu(const Ptr &hexview);

	template <class Ptr>
	void followInDump(const Ptr &hexview);

	template <class Ptr>
	void followInStack(const Ptr &hexview);

	template <class Ptr>
	void modifyBytes(const Ptr &hexview);

	template <class F1, class F2>
	void stepOver(F1 run_func, F2 step_func);

public:
	Ui::Debugger ui;

private:
	GuiState guiState_                    = Terminated;
	QProcess *ttyProc_                    = nullptr;
	DialogArguments *argumentsDialog_     = nullptr;
	QLabel *status_                       = nullptr;
	QStringListModel *listModel_          = nullptr;
	QTimer *timer_                        = nullptr;
	QToolButton *tabCreate_               = nullptr;
	QToolButton *tabDelete_               = nullptr;
	RecentFileManager *recentFileManager_ = nullptr;
	bool stackViewLocked_                 = false;

#if defined(Q_OS_LINUX)
	edb::address_t debugtPointer_  = 0;
	bool dynamicInfoBreakpointSet_ = false;
#endif

private:
	DataViewInfo stackViewInfo_;
	QString lastOpenDirectory_;
	QString programExecutable_;
	QString ttyFile_;
	QString workingDirectory_;
	QVector<std::shared_ptr<DataViewInfo>> dataRegions_;
	std::shared_ptr<IBreakpoint> reenableBreakpointRun_;
	std::shared_ptr<IBreakpoint> reenableBreakpointStep_;
	std::shared_ptr<CommentServer> commentServer_;
	std::shared_ptr<QHexView> stackView_;
	std::shared_ptr<const IDebugEvent> lastEvent_;
	std::unique_ptr<IBinary> binaryInfo_;

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

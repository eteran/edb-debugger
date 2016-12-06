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

#ifndef HARDWAREBREAKPOINTS_20080228_H_
#define HARDWAREBREAKPOINTS_20080228_H_

#include "IPlugin.h"
#include "IDebugEventHandler.h"
#include "libHardwareBreakpoints.h"

class QDialog;
class QMenu;
class State;
class QCheckBox;
class QComboBox;
class QLineEdit;

namespace HardwareBreakpoints {

class HardwareBreakpoints : public QObject, public IPlugin, public IDebugEventHandler {
	Q_OBJECT
	Q_INTERFACES(IPlugin)
#if QT_VERSION >= 0x050000
	Q_PLUGIN_METADATA(IID "edb.IPlugin/1.0")
#endif
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")

private:


public:
	HardwareBreakpoints();

public:
	virtual QMenu *menu(QWidget *parent = 0);
	virtual edb::EVENT_STATUS handle_event(const IDebugEvent::const_pointer &event);
	virtual QList<QAction *> cpu_context_menu();	
	virtual QList<QAction *> stack_context_menu();
	virtual QList<QAction *> data_context_menu();

public Q_SLOTS:
	void show_menu();
	void set_exec1();
	void set_exec2();
	void set_exec3();
	void set_exec4();

	void set_write1();
	void set_write2();
	void set_write3();
	void set_write4();

	void set_access1();
	void set_access2();
	void set_access3();
	void set_access4();

private:
	void setupBreakpoints();
	void setExecuteBP(int index, bool inUse);
	void setReadWriteBP(int index, bool inUse, edb::address_t address, int size);
	void setWriteBP(int index, bool inUse, edb::address_t address, int size);

	void setDataReadWriteBP(int index, bool inUse);
	void setDataWriteBP(int index, bool inUse);
	void setStackReadWriteBP(int index, bool inUse);
	void setStackWriteBP(int index, bool inUse);
	void setCPUReadWriteBP(int index, bool inUse);
	void setCPUWriteBP(int index, bool inUse);

	void set_exec(int index);
	void set_write(int index);
	void set_access(int index);


private:
	QMenu *              menu_;
	QDialog *            dialog_;
	IDebugEventHandler * old_event_handler_;
	
	QLineEdit *addresses_[4];
	QCheckBox *enabled_[4];
	QComboBox *types_[4];
	QComboBox *sizes_[4];
};

}

#endif

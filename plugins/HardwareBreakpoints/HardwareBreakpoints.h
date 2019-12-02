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

#ifndef HARDWARE_BREAKPOINTS_H_20080228_
#define HARDWARE_BREAKPOINTS_H_20080228_

#include "IDebugEventHandler.h"
#include "IPlugin.h"
#include "libHardwareBreakpoints.h"

class QDialog;
class QMenu;
class State;
class QCheckBox;
class QComboBox;
class QLineEdit;

namespace HardwareBreakpointsPlugin {

class HardwareBreakpoints : public QObject, public IPlugin, public IDebugEventHandler {
	Q_OBJECT
	Q_INTERFACES(IPlugin)
	Q_PLUGIN_METADATA(IID "edb.IPlugin/1.0")
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")

public:
	HardwareBreakpoints(QObject *parent = nullptr);

protected:
	void privateInit() override;
	void privateFini() override;

public:
	QMenu *menu(QWidget *parent = nullptr) override;
	edb::EventStatus handleEvent(const std::shared_ptr<IDebugEvent> &event) override;
	QList<QAction *> cpuContextMenu() override;
	QList<QAction *> stackContextMenu() override;
	QList<QAction *> dataContextMenu() override;

public Q_SLOTS:
	void showMenu();

private:
	void setupBreakpoints();
	void setExecuteBP(int index, bool inUse);
	void setReadWriteBP(int index, bool inUse, edb::address_t address, size_t size);
	void setWriteBP(int index, bool inUse, edb::address_t address, size_t size);

	void setDataReadWriteBP(int index, bool inUse);
	void setDataWriteBP(int index, bool inUse);
	void setStackReadWriteBP(int index, bool inUse);
	void setStackWriteBP(int index, bool inUse);
	void setCPUReadWriteBP(int index, bool inUse);
	void setCPUWriteBP(int index, bool inUse);

	void setExec(int index);
	void setWrite(int index);
	void setAccess(int index);

private Q_SLOTS:
	void setWrite1();
	void setWrite2();
	void setWrite3();
	void setWrite4();

	void setAccess1();
	void setAccess2();
	void setAccess3();
	void setAccess4();

	void setExec1();
	void setExec2();
	void setExec3();
	void setExec4();

private:
	QMenu *menu_              = nullptr;
	QPointer<QDialog> dialog_ = nullptr;

	QLineEdit *addresses_[4] = {};
	QCheckBox *enabled_[4]   = {};
	QComboBox *types_[4]     = {};
	QComboBox *sizes_[4]     = {};
};

}

#endif

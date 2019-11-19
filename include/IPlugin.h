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

#ifndef IPLUGIN_H_20061101_
#define IPLUGIN_H_20061101_

#include <QList>
#include <QVariantMap>
#include <QtPlugin>

class QMenu;
class QAction;

class IPlugin {
public:
	virtual ~IPlugin() = default;

public:
	void init() {
		privateInit();
	}

	void fini() {
		privateFini();
	}

public:
	virtual QMenu *menu(QWidget *parent = nullptr) = 0;

public:
	// optional, overload these to have there contents added to a view's context menu
	virtual QList<QAction *> cpuContextMenu() { return {}; }
	virtual QList<QAction *> registerContextMenu() { return {}; }
	virtual QList<QAction *> stackContextMenu() { return {}; }
	virtual QList<QAction *> dataContextMenu() { return {}; }

	// optional, overload this to add a page to the options dialog
	virtual QWidget *optionsPage() { return nullptr; }

public:
	virtual QVariantMap saveState() const { return {}; }
	virtual void restoreState(const QVariantMap &) {}

public:
	enum ArgumentStatus {
		ARG_SUCCESS,
		ARG_ERROR,
		ARG_EXIT
	};

	// optional, command line argument processing
	// return a string to add to "--help"
	virtual QString extraArguments() const { return {}; }

	// take actions based on the command line arguments
	// you *may* remove arguments which are exclusively yours
	// return ARG_SUCCESS if the normal execution should continue
	// return ARG_ERROR   if we should show usage and exit
	// return ARG_EXIT    if you processed the arguments and we should terminate successfully
	virtual ArgumentStatus parseArguments(QStringList &) { return ARG_SUCCESS; }

protected:
	// optional init, overload this to have edb run it after loading the plugin
	virtual void privateInit() {
	}

	// optional fini, overload this to have edb run it before unloading the plugin
	virtual void privateFini() {
	}
};

Q_DECLARE_INTERFACE(IPlugin, "edb.IPlugin/1.0")

#endif

/*
Copyright (C) 2006 - 2011 Evan Teran
                          eteran@alum.rit.edu

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

#ifndef FUNCTIONDBPLUGIN_20070211_H_
#define FUNCTIONDBPLUGIN_20070211_H_

#include "DebuggerPluginInterface.h"
#include "FunctionDB.h"

class QMenu;
class QString;

class FunctionDBPlugin : public QObject, public DebuggerPluginInterface, public FunctionDB {
	Q_OBJECT
	Q_INTERFACES(DebuggerPluginInterface)
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")

public:
	virtual const FunctionInfo *find(const QString &name) const;

public:
	virtual QMenu *menu(QWidget *parent = 0);

private:
	virtual void private_init();
};

#endif

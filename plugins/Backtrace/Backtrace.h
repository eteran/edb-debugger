/*
Copyright (C) 2015	Armen Boursalian
					aboursalian@gmail.com

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

#ifndef BACKTRACE_H
#define BACKTRACE_H

#include "IPlugin.h"

class QMenu;
class QDialog;

namespace Backtrace {

class Backtrace : public QObject, public IPlugin
{
	Q_OBJECT
	Q_INTERFACES(IPlugin)
#if QT_VERSION >= 0x050000
	Q_PLUGIN_METADATA(IID "edb.IPlugin/1.0")
#endif
	Q_CLASSINFO("author", "Armen Boursalian")
	Q_CLASSINFO("url", "https://github.com/Northern-Lights")

public:
	Backtrace();
	virtual ~Backtrace();

public:
	virtual QMenu *menu(QWidget *parent = 0);

public Q_SLOTS:
	void show_menu();

private:
	QMenu	*menu_;
	QDialog	*dialog_;
};

}
#endif // BACKTRACE_H

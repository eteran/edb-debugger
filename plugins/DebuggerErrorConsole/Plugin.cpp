/*
Copyright (C) 2017 Ruslan Kabatsayev <b7.10110111@gmail.com>

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

#include "Plugin.h"

namespace DebuggerErrorConsolePlugin {

/**
 * @brief Plugin::Plugin
 * @param parent
 */
Plugin::Plugin(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief Plugin::menu
 * @param parent
 * @return
 */
QMenu *Plugin::menu(QWidget *) {
	return nullptr;
}

/**
 * @brief DebuggerErrorConsole::DebuggerErrorConsole
 * @param parent
 * @param f
 */
DebuggerErrorConsole::DebuggerErrorConsole(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {
}

}

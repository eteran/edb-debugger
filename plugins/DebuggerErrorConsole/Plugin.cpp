/*
 * Copyright (C) 2017 Ruslan Kabatsayev <b7.10110111@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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

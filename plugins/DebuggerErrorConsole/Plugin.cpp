/*
 * Copyright (C) 2017 Ruslan Kabatsayev <b7.10110111@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "Plugin.h"

// NOTE(eteran): This plugin has been reduced to a skeleton, as the original code was merged into the primary codebase.
// This exists solely to reserve the name and prevent conflicts with future plugins that may want to use the same name.
// The original code can be found in the git history if needed.
namespace DebuggerErrorConsolePlugin {

/**
 * @brief Constructs a Plugin object with the given parent.
 *
 * @param parent
 */
Plugin::Plugin(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief Returns a menu for the plugin, or nullptr if the plugin does not provide a menu.
 *
 * @param parent The parent widget for the menu.
 * @return a menu for the plugin, or nullptr if the plugin does not provide a menu.
 */
QMenu *Plugin::menu(QWidget *) {
	return nullptr;
}

/**
 * @brief Constructs a DebuggerErrorConsole dialog with the given parent and window flags.
 *
 * @param parent The parent widget for the dialog.
 * @param f The window flags for the dialog.
 */
DebuggerErrorConsole::DebuggerErrorConsole(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {
}

}

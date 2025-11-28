/*
 * Copyright (C) 2017 Ruslan Kabatsayev <b7.10110111@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DEBUGGER_ERROR_CONSOLE_PLUGIN_H_20170808_
#define DEBUGGER_ERROR_CONSOLE_PLUGIN_H_20170808_

#include "IPlugin.h"
#include "edb.h"
#include <QDialog>

namespace DebuggerErrorConsolePlugin {

class DebuggerErrorConsole : public QDialog {
	Q_OBJECT
public:
	explicit DebuggerErrorConsole(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
};

class Plugin : public QObject, public IPlugin {
	Q_OBJECT
	Q_INTERFACES(IPlugin)
	Q_PLUGIN_METADATA(IID "edb.IPlugin/1.0")
	Q_CLASSINFO("author", "Ruslan Kabatsayev")
	Q_CLASSINFO("email", "b7.10110111@gmail.com")

public:
	explicit Plugin(QObject *parent = nullptr);
	~Plugin() override = default;

public:
	[[nodiscard]] QMenu *menu(QWidget *parent = nullptr) override;
};

}

#endif

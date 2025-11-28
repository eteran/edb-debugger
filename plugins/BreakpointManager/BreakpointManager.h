/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef BREAKPOINT_MANAGER_H_20060430_
#define BREAKPOINT_MANAGER_H_20060430_

#include "IPlugin.h"

class QMenu;
class QDialog;

namespace BreakpointManagerPlugin {

class BreakpointManager : public QObject, public IPlugin {
	Q_OBJECT
	Q_INTERFACES(IPlugin)
	Q_PLUGIN_METADATA(IID "edb.IPlugin/1.0")
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")

public:
	explicit BreakpointManager(QObject *parent = nullptr);
	~BreakpointManager() override;

public:
	[[nodiscard]] QMenu *menu(QWidget *parent = nullptr) override;
};

}

#endif

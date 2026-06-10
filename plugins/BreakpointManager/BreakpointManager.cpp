/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "BreakpointManager.h"
#include <QMenu>

namespace BreakpointManagerPlugin {

/**
 * @brief Constructs the BreakpointManager plugin object.
 * @param parent
 */
BreakpointManager::BreakpointManager(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief Default destructor for the BreakpointManager plugin.
 */
BreakpointManager::~BreakpointManager() = default;

/**
 * @brief Returns nullptr, as the BreakpointManager plugin contributes no top-level menu.
 * @param parent
 * @return
 */
QMenu *BreakpointManager::menu(QWidget *parent) {
	Q_ASSERT(parent);
	return nullptr;
}

}

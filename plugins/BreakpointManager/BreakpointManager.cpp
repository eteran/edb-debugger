/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "BreakpointManager.h"
#include <QMenu>

namespace BreakpointManagerPlugin {

/**
 * @brief BreakpointManager::BreakpointManager
 * @param parent
 */
BreakpointManager::BreakpointManager(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief BreakpointManager::~BreakpointManager
 */
BreakpointManager::~BreakpointManager() = default;

/**
 * @brief BreakpointManager::menu
 * @param parent
 * @return
 */
QMenu *BreakpointManager::menu(QWidget *parent) {
	Q_ASSERT(parent);
	return nullptr;
}

}

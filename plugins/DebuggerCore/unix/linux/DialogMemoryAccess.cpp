/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogMemoryAccess.h"

namespace DebuggerCorePlugin {

/**
 * @brief Constructs the proc/mem access warning dialog and fixes its size.
 *
 * @param parent
 * @param f
 */
DialogMemoryAccess::DialogMemoryAccess(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);
	adjustSize();
	setFixedSize(width(), height());
}

/**
 * @brief Returns true if the warning dialog should be shown again on the next run.
 *
 * @return
 */
bool DialogMemoryAccess::warnNextTime() const {
	return !ui.checkNeverShowAgain->isChecked();
}

}

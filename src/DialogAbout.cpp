/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogAbout.h"
#include "version.h"

/**
 * @brief Constructor for the DialogAbout class.
 *
 * @param parent The parent widget.
 * @param f The window flags.
 */
DialogAbout::DialogAbout(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {
	ui.setupUi(this);
	ui.labelVersion->setText(tr("Version: %1<br>\n"
								"Compiled: %2<br>\n"
								"Git Commit: <a href=\"https://github.com/eteran/edb-debugger/commit/%3\">%3</a>")
								 .arg(EDB_VERSION_STRING, __DATE__, GIT_BRANCH));
}

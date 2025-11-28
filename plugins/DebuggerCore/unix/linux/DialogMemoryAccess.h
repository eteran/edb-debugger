/*
 * Copyright (C) 2016- 2025 Evan Teran
 *                          evan.teran@gmail.com
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_MEMORY_ACCESS_H_20160930_
#define DIALOG_MEMORY_ACCESS_H_20160930_

#include "ui_DialogMemoryAccess.h"
#include <QDialog>

namespace DebuggerCorePlugin {

class DialogMemoryAccess final : public QDialog {
	Q_OBJECT

public:
	explicit DialogMemoryAccess(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogMemoryAccess() override = default;

public:
	[[nodiscard]] bool warnNextTime() const;

private:
	Ui::DialogMemoryAccess ui;
};

}

#endif

/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_HW_BREAKPOINTS_H_20080228_
#define DIALOG_HW_BREAKPOINTS_H_20080228_

#include "ui_DialogHwBreakpoints.h"
#include <QDialog>

namespace HardwareBreakpointsPlugin {

class DialogHwBreakpoints : public QDialog {
	Q_OBJECT

private:
	friend class HardwareBreakpoints;

public:
	explicit DialogHwBreakpoints(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogHwBreakpoints() override = default;

private:
	void showEvent(QShowEvent *event) override;

private Q_SLOTS:
	void type1IndexChanged(int index);
	void type2IndexChanged(int index);
	void type3IndexChanged(int index);
	void type4IndexChanged(int index);

private:
	Ui::DialogHwBreakpoints ui;
};

}

#endif

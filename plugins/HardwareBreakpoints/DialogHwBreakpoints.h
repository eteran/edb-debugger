/*
Copyright (C) 2006 - 2023 Evan Teran
						  evan.teran@gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

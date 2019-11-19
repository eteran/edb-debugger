/*
Copyright (C) 2016 - 2016 Evan Teran
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
	bool warnNextTime() const;

private:
	Ui::DialogMemoryAccess ui;
};

}

#endif

/*
Copyright (C) 2006 - 2015 Evan Teran
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

#ifndef ASSEMBLER_OPTIONS_PAGE_20130611_H_
#define ASSEMBLER_OPTIONS_PAGE_20130611_H_

#include <QWidget>

namespace Assembler {

namespace Ui { class OptionsPage; }

class OptionsPage : public QWidget {
	Q_OBJECT

public:
	OptionsPage(QWidget *parent = 0);
	virtual ~OptionsPage();

public:
	virtual void showEvent(QShowEvent *event);

public Q_SLOTS:
	virtual void on_assemblerName_currentIndexChanged(const QString &text);
	
private:
	Ui::OptionsPage *const ui;
};

}

#endif

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

#ifndef DIALOGARGUMENTS_20090609_H_
#define DIALOGARGUMENTS_20090609_H_

#include <QDialog>
#include "ui_DialogArguments.h"

class DialogArguments : public QDialog {
	Q_OBJECT

public:
    explicit DialogArguments(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogArguments() override = default;

public:
	QList<QByteArray> arguments() const;
	void set_arguments(const QList<QByteArray> &args);

private:
	 Ui::DialogArguments ui;
};

#endif


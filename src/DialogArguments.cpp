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

#include "DialogArguments.h"
#include <QListWidgetItem>

//------------------------------------------------------------------------------
// Name: DialogArguments
// Desc:
//------------------------------------------------------------------------------
DialogArguments::DialogArguments(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {
	ui.setupUi(this);
}

//------------------------------------------------------------------------------
// Name: arguments
// Desc:
//------------------------------------------------------------------------------
QList<QByteArray> DialogArguments::arguments() const {
	QList<QByteArray> ret;
	for (int i = 0; i < ui.listWidget->count(); ++i) {
		ret << ui.listWidget->item(i)->text().toUtf8();
	}
	return ret;
}

//------------------------------------------------------------------------------
// Name: set_arguments
// Desc:
//------------------------------------------------------------------------------
void DialogArguments::setArguments(const QList<QByteArray> &args) {
	ui.listWidget->clear();

	QStringList l;
	for (const QByteArray &ba : args) {
		l << QString::fromUtf8(ba.constData());
	}

	ui.listWidget->addItems(l);
}

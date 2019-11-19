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

#ifndef DIALOG_REFERENCES_H_20061101_
#define DIALOG_REFERENCES_H_20061101_

#include "IRegion.h"
#include "Types.h"
#include "ui_DialogReferences.h"
#include <QDialog>

class QListWidgetItem;

namespace ReferencesPlugin {

class DialogReferences : public QDialog {
	Q_OBJECT

public:
	explicit DialogReferences(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogReferences() override = default;

public Q_SLOTS:
	void on_listWidget_itemDoubleClicked(QListWidgetItem *item);

Q_SIGNALS:
	void updateProgress(int);

private:
	void showEvent(QShowEvent *event) override;

private:
	void doFind();

private:
	Ui::DialogReferences ui;
	QPushButton *buttonFind_ = nullptr;
};

}

#endif

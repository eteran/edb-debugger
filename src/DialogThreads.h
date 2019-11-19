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

#ifndef DIALOG_THREADS_H_20101026_
#define DIALOG_THREADS_H_20101026_

#include <QDialog>

#include "ui_DialogThreads.h"

class ThreadsModel;
class QSortFilterProxyModel;
class QModelIndex;

class DialogThreads : public QDialog {
	Q_OBJECT
public:
	DialogThreads(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogThreads() override = default;

private Q_SLOTS:
	void on_thread_table_doubleClicked(const QModelIndex &index);
	void updateThreads();

public:
	void showEvent(QShowEvent *) override;

private:
	Ui::DialogThreads ui;
	ThreadsModel *threadsModel_           = nullptr;
	QSortFilterProxyModel *threadsFilter_ = nullptr;
};

#endif

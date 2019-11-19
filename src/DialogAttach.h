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

#ifndef DIALOG_ATTACH_H_20091218_
#define DIALOG_ATTACH_H_20091218_

#include "OSTypes.h"

#include <QDialog>
#include <QTimer>

#include "ui_DialogAttach.h"

template <class T, class E>
class Result;

class ProcessModel;
class QSortFilterProxyModel;
class QModelIndex;

class DialogAttach final : public QDialog {
	Q_OBJECT

public:
	explicit DialogAttach(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogAttach() override = default;

private:
	void showEvent(QShowEvent *event) override;

private:
	void updateList();

public Q_SLOTS:
	void on_filter_uid_clicked(bool checked);
	void on_filter_textChanged(const QString &filter);
	void on_processes_table_doubleClicked(const QModelIndex &index);

public:
	Result<edb::pid_t, QString> selectedPid() const;

private:
	Ui::DialogAttach ui;
	ProcessModel *processModel_               = nullptr;
	QSortFilterProxyModel *processNameFilter_ = nullptr;
	QSortFilterProxyModel *processPidFilter_  = nullptr;
	QTimer updateTimer_;
};

#endif

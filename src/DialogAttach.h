/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
	[[nodiscard]] Result<edb::pid_t, QString> selectedPid() const;

private:
	Ui::DialogAttach ui;
	ProcessModel *processModel_               = nullptr;
	QSortFilterProxyModel *processNameFilter_ = nullptr;
	QSortFilterProxyModel *processPidFilter_  = nullptr;
	QTimer updateTimer_;
};

#endif

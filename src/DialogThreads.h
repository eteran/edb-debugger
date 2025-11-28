/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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

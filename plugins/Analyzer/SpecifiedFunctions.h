/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_SPECIFIED_FUNCTIONS_H_20070705_
#define DIALOG_SPECIFIED_FUNCTIONS_H_20070705_

#include "ui_SpecifiedFunctions.h"
#include <QDialog>

class QStringListModel;
class QSortFilterProxyModel;
class QModelIndex;

namespace AnalyzerPlugin {

class SpecifiedFunctions : public QDialog {
	Q_OBJECT

public:
	explicit SpecifiedFunctions(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~SpecifiedFunctions() override = default;

public Q_SLOTS:
	void on_function_list_doubleClicked(const QModelIndex &index);

private:
	void showEvent(QShowEvent *event) override;

private:
	void doFind();

private:
	Ui::SpecifiedFunctions ui;
	QStringListModel *model_            = nullptr;
	QSortFilterProxyModel *filterModel_ = nullptr;
	QPushButton *buttonRefresh_         = nullptr;
};

}

#endif

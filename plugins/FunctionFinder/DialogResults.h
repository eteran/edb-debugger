/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_RESULTS_H_20190403_
#define DIALOG_RESULTS_H_20190403_

#include "Types.h"
#include "ui_DialogResults.h"
#include <QDialog>

class QSortFilterProxyModel;
class IAnalyzer;
class Function;

namespace FunctionFinderPlugin {

class ResultsModel;

class DialogResults : public QDialog {
	Q_OBJECT

public:
	explicit DialogResults(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogResults() override = default;

public:
	void addResult(const Function &function);
	[[nodiscard]] int resultCount() const;

public Q_SLOTS:
	void on_tableView_doubleClicked(const QModelIndex &index);

private:
	Ui::DialogResults ui;
	QSortFilterProxyModel *filterModel_ = nullptr;
	ResultsModel *resultsModel_         = nullptr;
	QPushButton *buttonGraph_           = nullptr;
};

}

#endif

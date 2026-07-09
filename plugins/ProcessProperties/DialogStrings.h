/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_STRINGS_H_20061101_
#define DIALOG_STRINGS_H_20061101_

#include "ResultsModel.h"
#include "Types.h"
#include "ui_DialogStrings.h"
#include <QDialog>
#include <QFutureWatcher>
#include <QTimer>

#include <atomic>
#include <vector>

class QSortFilterProxyModel;
class QListWidgetItem;
class IProcess;

namespace ProcessPropertiesPlugin {

class DialogStrings : public QDialog {
	Q_OBJECT

public:
	explicit DialogStrings(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogStrings() override = default;

private:
	void reject() override;
	void showEvent(QShowEvent *event) override;

private:
	struct RegionScan {
		edb::address_t start = 0;
		edb::address_t end   = 0;
		bool accessible      = false;
	};

	struct SearchResult {
		std::vector<ResultsModel::Result> results;
		bool cancelled        = false;
		bool allocationFailed = false;
	};

	void onFindClicked();
	void onFindFinished();
	void updateProgressBar();
	void setSearchRunning(bool running);
	SearchResult doFind(const std::vector<RegionScan> &regions, bool searchUnicode, int minStringLength);

private:
	Ui::DialogStrings ui;
	QSortFilterProxyModel *filterModel_ = nullptr;
	QPushButton *buttonFind_            = nullptr;
	QFutureWatcher<SearchResult> searchWatcher_;
	QTimer progressTimer_;
	std::atomic_bool cancelRequested_ = false;
	std::atomic_size_t progressDone_  = 0;
	std::atomic_size_t progressTotal_ = 0;
	bool searchRunning_               = false;
};

}

#endif

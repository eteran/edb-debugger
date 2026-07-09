/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_FUNCTIONS_H_20061101_
#define DIALOG_FUNCTIONS_H_20061101_

#include "Function.h"
#include "Types.h"
#include "ui_DialogFunctions.h"
#include <QDialog>
#include <QFutureWatcher>
#include <QTimer>

#include <atomic>
#include <vector>

class QSortFilterProxyModel;
class IAnalyzer;
class IRegion;

namespace FunctionFinderPlugin {

class DialogFunctions : public QDialog {
	Q_OBJECT

public:
	explicit DialogFunctions(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogFunctions() override = default;

private:
	void reject() override;
	void showEvent(QShowEvent *event) override;

private:
	struct RegionScan {
		std::shared_ptr<IRegion> region;
	};

	struct SearchResult {
		std::vector<Function> functions;
		bool cancelled = false;
	};

	void onFindClicked();
	void onFindFinished();
	void updateProgressBar();
	void setSearchRunning(bool running);
	SearchResult doFind(const std::vector<RegionScan> &regions);

private:
	Ui::DialogFunctions ui;
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

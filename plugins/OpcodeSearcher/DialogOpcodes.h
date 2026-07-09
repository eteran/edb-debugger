/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_OPCODES_H_20061101_
#define DIALOG_OPCODES_H_20061101_

#include "ResultsModel.h"
#include "ui_DialogOpcodes.h"
#include <QDialog>
#include <QFutureWatcher>
#include <QTimer>

class QSortFilterProxyModel;
class IProcess;

#include <atomic>
#include <vector>

namespace OpcodeSearcherPlugin {

class DialogResults;

class DialogOpcodes : public QDialog {
	Q_OBJECT

public:
	explicit DialogOpcodes(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogOpcodes() override = default;

private:
	void reject() override;

private:
	struct RegionScan {
		edb::address_t start = 0;
		edb::address_t end   = 0;
	};

	struct SearchResult {
		QVector<ResultsModel::Result> results;
		bool cancelled = false;
	};

	void onFindClicked();
	void onFindFinished();
	void updateProgressBar();
	void setSearchRunning(bool running);
	SearchResult doFind(int classtype, bool is32Bit, const IProcess *process, const std::vector<RegionScan> &regions);

private:
	void showEvent(QShowEvent *event) override;

private:
	Ui::DialogOpcodes ui;
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

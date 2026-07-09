/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_HEAP_H_20061101_
#define DIALOG_HEAP_H_20061101_

#include "ResultViewModel.h"
#include "Types.h"
#include "ui_DialogHeap.h"
#include <QDialog>
#include <QFutureWatcher>
#include <QTimer>

#include <atomic>
#include <vector>

class QSortFilterProxyModel;

namespace HeapAnalyzerPlugin {

struct SearchResult {
	QVector<ResultViewModel::Result> results;
	qint64 freeBlocks  = 0;
	qint64 busyBlocks  = 0;
	bool cancelled     = false;
};

class DialogHeap : public QDialog {
	Q_OBJECT

public:
	explicit DialogHeap(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogHeap() override = default;

public Q_SLOTS:
	void on_tableView_doubleClicked(const QModelIndex &index);

private:
	void reject() override;
	void showEvent(QShowEvent *event) override;

private:
	void onFindClicked();
	void onFindFinished();
	void updateProgressBar();
	void setSearchRunning(bool running);
	template <class Addr>
	SearchResult collectBlocks(edb::address_t start_address, edb::address_t end_address);

	template <class Addr>
	void doFind();

	void detectPointers();
	void processPotentialPointers(const QHash<edb::address_t, edb::address_t> &targets, const QModelIndex &index);
	[[nodiscard]] QMap<edb::address_t, const ResultViewModel::Result *> createResultMap() const;

private:
	Ui::DialogHeap ui;
	ResultViewModel *model_             = nullptr;
	QSortFilterProxyModel *filterModel_ = nullptr;
	QPushButton *buttonAnalyze_         = nullptr;
	QPushButton *buttonGraph_           = nullptr;
	QFutureWatcher<SearchResult> searchWatcher_;
	QTimer progressTimer_;
	std::atomic_bool cancelRequested_ = false;
	std::atomic_size_t progressDone_   = 0;
	std::atomic_size_t progressTotal_  = 0;
	bool searchRunning_               = false;
};

}

#endif

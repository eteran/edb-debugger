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

class QSortFilterProxyModel;

namespace HeapAnalyzerPlugin {

class DialogHeap : public QDialog {
	Q_OBJECT

public:
	explicit DialogHeap(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogHeap() override = default;

public Q_SLOTS:
	void on_tableView_doubleClicked(const QModelIndex &index);

private:
	void showEvent(QShowEvent *event) override;

private:
	void detectPointers();
	void processPotentialPointers(const QHash<edb::address_t, edb::address_t> &targets, const QModelIndex &index);
	[[nodiscard]] edb::address_t findHeapStartHeuristic(edb::address_t end_address, size_t offset) const;
	[[nodiscard]] QMap<edb::address_t, const ResultViewModel::Result *> createResultMap() const;

private:
	template <class Addr>
	void collectBlocks(edb::address_t start_address, edb::address_t end_address);

	template <class Addr>
	void doFind();

private:
	Ui::DialogHeap ui;
	ResultViewModel *model_             = nullptr;
	QSortFilterProxyModel *filterModel_ = nullptr;
	QPushButton *buttonAnalyze_         = nullptr;
	QPushButton *buttonGraph_           = nullptr;
};

}

#endif

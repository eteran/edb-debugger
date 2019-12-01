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
	edb::address_t findHeapStartHeuristic(edb::address_t end_address, size_t offset) const;
	QMap<edb::address_t, const ResultViewModel::Result *> createResultMap() const;

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

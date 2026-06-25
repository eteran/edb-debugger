/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogResults.h"
#include "edb.h"
#include <QSortFilterProxyModel>

namespace ProcessPropertiesPlugin {

/**
 * @brief Constructs a DialogResults instance with the specified parent and window flags.
 *
 * @param parent The parent widget.
 * @param f The window flags.
 */
DialogResults::DialogResults(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {
	ui.setupUi(this);
	ui.tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	model_       = new ResultsModel(this);
	filterModel_ = new QSortFilterProxyModel(this);

	filterModel_->setFilterKeyColumn(2);
	filterModel_->setSourceModel(model_);
	ui.tableView->setModel(filterModel_);

	connect(ui.textFilter, &QLineEdit::textChanged, filterModel_, &QSortFilterProxyModel::setFilterFixedString);
}

/**
 * @brief Adds a result to the DialogResults instance.
 *
 * @param result The result to be added to the results model.
 */
void DialogResults::addResult(const ResultsModel::Result &result) {
	model_->addResult(result);
}

/**
 * @brief Handles the double-click event on the table view in the DialogResults instance.
 *
 * @param index The index of the double-clicked item in the table view.
 */
void DialogResults::on_tableView_doubleClicked(const QModelIndex &index) {
	if (index.isValid()) {
		const QModelIndex realIndex = filterModel_->mapToSource(index);
		if (realIndex.isValid()) {
			if (auto item = static_cast<ResultsModel::Result *>(realIndex.internalPointer())) {
				edb::v1::dump_data(item->address, false);
			}
		}
	}
}

/**
 * @brief Gets the number of results in the DialogResults instance.
 *
 * @return The number of results in the results model.
 */
int DialogResults::resultCount() const {
	return model_->rowCount();
}

}

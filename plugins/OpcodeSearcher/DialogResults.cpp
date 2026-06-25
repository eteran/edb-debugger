/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogResults.h"
#include "edb.h"
#include <QSortFilterProxyModel>

namespace OpcodeSearcherPlugin {

/**
 * @brief Constructor for the DialogResults class.
 *
 * @param parent The parent widget for the dialog.
 * @param f The window flags for the dialog.
 */
DialogResults::DialogResults(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);
	ui.tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	model_       = new ResultsModel(this);
	filterModel_ = new QSortFilterProxyModel(this);

	filterModel_->setFilterKeyColumn(1);
	filterModel_->setSourceModel(model_);
	ui.tableView->setModel(filterModel_);

	connect(ui.textFilter, &QLineEdit::textChanged, filterModel_, &QSortFilterProxyModel::setFilterFixedString);
}

/**
 * @brief Adds a result to the results model. This function is used to populate the results table with new search results.
 *
 * @param result The result to be added to the model.
 */
void DialogResults::addResult(const ResultsModel::Result &result) {
	model_->addResult(result);
}

/**
 * @brief Handles the event when a row in the table view is double-clicked. It retrieves the address from the selected result and jumps to that address in the debugger.
 *
 * @param index The index of the double-clicked item in the table view.
 */
void DialogResults::on_tableView_doubleClicked(const QModelIndex &index) {
	if (index.isValid()) {
		const QModelIndex realIndex = filterModel_->mapToSource(index);
		if (realIndex.isValid()) {
			if (auto item = static_cast<ResultsModel::Result *>(realIndex.internalPointer())) {
				edb::v1::jump_to_address(item->address);
			}
		}
	}
}

/**
 * @brief Returns the number of results currently in the model.
 *
 * @return The number of results in the model.
 */
int DialogResults::resultCount() const {
	return model_->rowCount();
}

}

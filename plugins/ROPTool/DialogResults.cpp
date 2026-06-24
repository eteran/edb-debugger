/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogResults.h"
#include "ResultsModel.h"
#include "edb.h"
#include <QSortFilterProxyModel>

namespace ROPToolPlugin {

/**
 * @brief Constructs a DialogResults object with the specified parent widget and window flags.
 *
 * @param parent The parent widget for this dialog.
 * @param f The window flags for this dialog.
 */
DialogResults::DialogResults(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);
	ui.tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	model_       = new ResultsModel(this);
	filterModel_ = new QSortFilterProxyModel(this);

	resultFilter_ = new ResultFilterProxy(this);
	resultFilter_->setSourceModel(model_);

	filterModel_->setFilterKeyColumn(1);
	filterModel_->setSourceModel(resultFilter_);
	ui.tableView->setModel(filterModel_);

	connect(ui.textFilter, &QLineEdit::textChanged, filterModel_, &QSortFilterProxyModel::setFilterFixedString);
	connect(ui.chkShowALU, &QCheckBox::stateChanged, this, [this](int state) {
		resultFilter_->setMask(0x01, state);
	});

	connect(ui.chkShowStack, &QCheckBox::stateChanged, this, [this](int state) {
		resultFilter_->setMask(0x02, state);
	});

	connect(ui.chkShowLogic, &QCheckBox::stateChanged, this, [this](int state) {
		resultFilter_->setMask(0x04, state);
	});

	connect(ui.chkShowData, &QCheckBox::stateChanged, this, [this](int state) {
		resultFilter_->setMask(0x08, state);
	});

	connect(ui.chkShowOther, &QCheckBox::stateChanged, this, [this](int state) {
		resultFilter_->setMask(0x10, state);
	});
}

/**
 * @brief Adds a result to the results model.
 *
 * @param result The result to be added to the model.
 */
void DialogResults::addResult(const ResultsModel::Result &result) {
	model_->addResult(result);
}

/**
 * @brief Handles the double-click event for the table view.
 * When a result is double-clicked, it jumps to the corresponding address in the debugger.
 *
 * @param index The index of the clicked item.
 */
void DialogResults::on_tableView_doubleClicked(const QModelIndex &index) {
	if (index.isValid()) {
		const QModelIndex realIndex = filterModel_->mapToSource(index);
		if (realIndex.isValid()) {
			const QModelIndex realIndex2 = resultFilter_->mapToSource(realIndex);
			if (auto item = static_cast<ResultsModel::Result *>(realIndex2.internalPointer())) {
				edb::v1::jump_to_address(item->address);
			}
		}
	}
}

/**
 * @brief Returns the number of results in the model.
 *
 * @return The number of results in the model.
 */
int DialogResults::resultCount() const {
	return model_->rowCount();
}

}

/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogStrings.h"
#include "Configuration.h"
#include "DialogResults.h"
#include "IRegion.h"
#include "MemoryRegions.h"
#include "ResultsModel.h"
#include "edb.h"
#include "util/Math.h"

#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>

namespace ProcessPropertiesPlugin {

/**
 * @brief DialogStrings::DialogStrings
 * @param parent
 * @param f
 */
DialogStrings::DialogStrings(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);
	ui.tableView->verticalHeader()->hide();
	ui.tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	filterModel_ = new QSortFilterProxyModel(this);
	connect(ui.txtSearch, &QLineEdit::textChanged, filterModel_, &QSortFilterProxyModel::setFilterFixedString);

	buttonFind_ = new QPushButton(QIcon::fromTheme("edit-find"), tr("Find"));
	connect(buttonFind_, &QPushButton::clicked, this, [this]() {
		buttonFind_->setEnabled(false);
		ui.progressBar->setValue(0);
		doFind();
		ui.progressBar->setValue(100);
		buttonFind_->setEnabled(true);
	});

	ui.buttonBox->addButton(buttonFind_, QDialogButtonBox::ActionRole);
}

/**
 * @brief DialogStrings::showEvent
 */
void DialogStrings::showEvent(QShowEvent *) {
	filterModel_->setFilterKeyColumn(3);
	filterModel_->setSourceModel(&edb::v1::memory_regions());
	ui.tableView->setModel(filterModel_);
	ui.progressBar->setValue(0);
}

/**
 * @brief DialogStrings::doFind
 */
void DialogStrings::doFind() {

	const int min_string_length = edb::v1::config().min_string_length;

	const QItemSelectionModel *const selection_model = ui.tableView->selectionModel();
	const QModelIndexList sel                        = selection_model->selectedRows();

	QString str;

	if (sel.empty()) {
		QMessageBox::critical(
			this,
			tr("No Region Selected"),
			tr("You must select a region which is to be scanned for strings."));
		return;
	}

	auto resultsDialog = new DialogResults(this);

	for (const QModelIndex &selected_item : sel) {

		const QModelIndex index = filterModel_->mapToSource(selected_item);

		if (auto region = *reinterpret_cast<const std::shared_ptr<IRegion> *>(index.internalPointer())) {

			edb::address_t start_address     = region->start();
			const edb::address_t end_address = region->end();
			const edb::address_t orig_start  = start_address;

			// do the search for this region!
			while (start_address < end_address) {

				int string_length = 0;
				bool ok           = edb::v1::get_ascii_string_at_address(start_address, str, min_string_length, 256, string_length);
				if (ok) {
					resultsDialog->addResult({start_address, str, ResultsModel::Result::Ascii});
				} else if (ui.search_unicode->isChecked()) {
					string_length = 0;
					ok            = edb::v1::get_utf16_string_at_address(start_address, str, min_string_length, 256, string_length);
					if (ok) {
						resultsDialog->addResult({start_address, str, ResultsModel::Result::Utf16});
					}
				}

				ui.progressBar->setValue(util::percentage((start_address - orig_start), region->size()));

				if (ok) {
					start_address += string_length;
				} else {
					++start_address;
				}
			}
		}
	}

	if (resultsDialog->resultCount() == 0) {
		QMessageBox::information(this, tr("No Strings Found"), tr("No strings were found in the selected region"));
		delete resultsDialog;
	} else {
		resultsDialog->show();
	}
}

}

/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogRegions.h"
#include "DialogHeader.h"
#include "MemoryRegions.h"
#include "edb.h"
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTreeWidgetItem>

namespace BinaryInfoPlugin {

/**
 * @brief DialogRegions::DialogRegions
 * @param parent
 */
DialogRegions::DialogRegions(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);
	ui.tableView->verticalHeader()->hide();
	ui.tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	filterModel_ = new QSortFilterProxyModel(this);
	connect(ui.txtSearch, &QLineEdit::textChanged, filterModel_, &QSortFilterProxyModel::setFilterFixedString);

	buttonExplore_ = new QPushButton(QIcon::fromTheme("edit-find"), tr("Explore Header"));
	connect(buttonExplore_, &QPushButton::clicked, this, [this]() {
		const QItemSelectionModel *const selModel = ui.tableView->selectionModel();
		const QModelIndexList sel                 = selModel->selectedRows();

		if (sel.empty()) {
			QMessageBox::critical(
				this,
				tr("No Region Selected"),
				tr("You must select a region which is to be scanned for executable headers."));
		} else {

			for (const QModelIndex &selected_item : sel) {

				const QModelIndex index = filterModel_->mapToSource(selected_item);
				if (auto region = *reinterpret_cast<const std::shared_ptr<IRegion> *>(index.internalPointer())) {
					auto dialog = new DialogHeader(region, this);
					dialog->show();
				}
			}
		}
	});

	ui.buttonBox->addButton(buttonExplore_, QDialogButtonBox::ActionRole);
}

/**
 * @brief DialogRegions::showEvent
 */
void DialogRegions::showEvent(QShowEvent *) {
	filterModel_->setFilterKeyColumn(3);
	filterModel_->setSourceModel(&edb::v1::memory_regions());
	ui.tableView->setModel(filterModel_);
}

}

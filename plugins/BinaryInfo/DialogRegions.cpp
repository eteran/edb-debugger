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

		if (sel.size() == 0) {
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

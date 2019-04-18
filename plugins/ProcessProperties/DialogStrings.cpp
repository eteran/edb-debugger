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

#include "DialogStrings.h"
#include "Configuration.h"
#include "IRegion.h"
#include "MemoryRegions.h"
#include "DialogResults.h"
#include "Util.h"
#include "edb.h"

#include <QHeaderView>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QPushButton>

namespace ProcessPropertiesPlugin {

//------------------------------------------------------------------------------
// Name: DialogStrings
// Desc:
//------------------------------------------------------------------------------
DialogStrings::DialogStrings(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f)  {
	ui.setupUi(this);
	ui.tableView->verticalHeader()->hide();
	ui.tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	filter_model_ = new QSortFilterProxyModel(this);
	connect(ui.txtSearch, &QLineEdit::textChanged, filter_model_, &QSortFilterProxyModel::setFilterFixedString);

	btnFind_ = new QPushButton(QIcon::fromTheme("edit-find"), tr("Find"));
	connect(btnFind_, &QPushButton::clicked, this, [this]() {
		btnFind_->setEnabled(false);
		ui.progressBar->setValue(0);
		do_find();
		ui.progressBar->setValue(100);
		btnFind_->setEnabled(true);
	});

	ui.buttonBox->addButton(btnFind_, QDialogButtonBox::ActionRole);
}

//------------------------------------------------------------------------------
// Name: showEvent
// Desc:
//------------------------------------------------------------------------------
void DialogStrings::showEvent(QShowEvent *) {
	filter_model_->setFilterKeyColumn(3);
	filter_model_->setSourceModel(&edb::v1::memory_regions());
	ui.tableView->setModel(filter_model_);
	ui.progressBar->setValue(0);
}

//------------------------------------------------------------------------------
// Name: do_find
// Desc:
//------------------------------------------------------------------------------
void DialogStrings::do_find() {

	const int min_string_length = edb::v1::config().min_string_length;

	const QItemSelectionModel *const selection_model = ui.tableView->selectionModel();
	const QModelIndexList sel = selection_model->selectedRows();

	QString str;

	if(sel.size() == 0) {
		QMessageBox::critical(
			this,
			tr("No Region Selected"),
			tr("You must select a region which is to be scanned for strings."));
		return;
	}

	auto resultsDialog = new DialogResults(this);

	for(const QModelIndex &selected_item: sel) {

		const QModelIndex index = filter_model_->mapToSource(selected_item);

		if(auto region = *reinterpret_cast<const std::shared_ptr<IRegion> *>(index.internalPointer())) {

			edb::address_t start_address     = region->start();
			const edb::address_t end_address = region->end();
			const edb::address_t orig_start  = start_address;

			// do the search for this region!
			while(start_address < end_address) {

				int string_length = 0;
				bool ok = edb::v1::get_ascii_string_at_address(start_address, str, min_string_length, 256, string_length);
				if(ok) {
					resultsDialog->addResult({start_address, str, Result::ASCII});
				} else if(ui.search_unicode->isChecked()) {
					string_length = 0;
					ok = edb::v1::get_utf16_string_at_address(start_address, str, min_string_length, 256, string_length);
					if(ok) {
						resultsDialog->addResult({start_address, str, Result::UTF16});
					}
				}

				ui.progressBar->setValue(util::percentage((start_address - orig_start), region->size()));

				if(ok) {
					start_address += string_length;
				} else {
					++start_address;
				}
			}
		}
	}

	if(resultsDialog->resultCount() == 0) {
		QMessageBox::information(this, tr("No Strings Found"), tr("No strings were found in the selected region"));
		delete resultsDialog;
	} else {
		resultsDialog->show();
	}

}

}

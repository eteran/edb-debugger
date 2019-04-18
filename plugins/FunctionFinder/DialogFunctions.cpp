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

#include "DialogFunctions.h"
#include "DialogResults.h"
#include "edb.h"
#include "IAnalyzer.h"
#include "MemoryRegions.h"

#include <QDialog>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QPushButton>

namespace FunctionFinderPlugin {


//------------------------------------------------------------------------------
// Name: DialogFunctions
// Desc:
//------------------------------------------------------------------------------
DialogFunctions::DialogFunctions(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f) {
	ui.setupUi(this);
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
void DialogFunctions::showEvent(QShowEvent *) {
	filter_model_->setFilterKeyColumn(3);
	filter_model_->setSourceModel(&edb::v1::memory_regions());
	ui.tableView->setModel(filter_model_);

	ui.progressBar->setValue(0);
}

//------------------------------------------------------------------------------
// Name: do_find
// Desc:
//------------------------------------------------------------------------------
void DialogFunctions::do_find() {

	if(IAnalyzer *const analyzer = edb::v1::analyzer()) {
		const QItemSelectionModel *const selModel = ui.tableView->selectionModel();
		const QModelIndexList sel = selModel->selectedRows();

		if(sel.size() == 0) {
			QMessageBox::critical(this, tr("No Region Selected"), tr("You must select a region which is to be scanned for functions."));
			return;
		}

		auto analyzer_object = dynamic_cast<QObject *>(analyzer);
		if(analyzer_object) {
			connect(analyzer_object, SIGNAL(update_progress(int)), ui.progressBar, SLOT(setValue(int)));
		}

		auto resultsDialog = new DialogResults(this);

		for(const QModelIndex &selected_item: sel) {

			const QModelIndex index = filter_model_->mapToSource(selected_item);

			// do the search for this region!
			if(auto region = *reinterpret_cast<const std::shared_ptr<IRegion> *>(index.internalPointer())) {

				analyzer->analyze(region);

				const IAnalyzer::FunctionMap &results = analyzer->functions(region);
				for(const Function &function: results) {
					resultsDialog->addResult(function);
				}
			}
		}

		if(resultsDialog->resultCount() == 0) {
			QMessageBox::information(this, tr("No Results"), tr("No Functions Found!"));
			delete resultsDialog;
		} else {
			resultsDialog->show();
		}

		if(analyzer_object) {
			disconnect(analyzer_object, SIGNAL(update_progress(int)), ui.progressBar, SLOT(setValue(int)));
		}
	}
}


}

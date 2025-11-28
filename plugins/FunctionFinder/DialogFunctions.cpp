/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogFunctions.h"
#include "DialogResults.h"
#include "IAnalyzer.h"
#include "MemoryRegions.h"
#include "edb.h"

#include <QDialog>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>

namespace FunctionFinderPlugin {

/**
 * @brief DialogFunctions::DialogFunctions
 * @param parent
 * @param f
 */
DialogFunctions::DialogFunctions(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);
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
 * @brief DialogFunctions::showEvent
 */
void DialogFunctions::showEvent(QShowEvent *) {
	filterModel_->setFilterKeyColumn(3);
	filterModel_->setSourceModel(&edb::v1::memory_regions());
	ui.tableView->setModel(filterModel_);

	ui.progressBar->setValue(0);
}

/**
 * @brief DialogFunctions::doFind
 */
void DialogFunctions::doFind() {

	if (IAnalyzer *const analyzer = edb::v1::analyzer()) {
		const QItemSelectionModel *const selModel = ui.tableView->selectionModel();
		const QModelIndexList sel                 = selModel->selectedRows();

		if (sel.empty()) {
			QMessageBox::critical(this, tr("No Region Selected"), tr("You must select a region which is to be scanned for functions."));
			return;
		}

		auto analyzer_object = dynamic_cast<QObject *>(analyzer);
		if (analyzer_object) {
			connect(analyzer_object, SIGNAL(updateProgress(int)), ui.progressBar, SLOT(setValue(int)));
		}

		auto resultsDialog = new DialogResults(this);

		for (const QModelIndex &selected_item : sel) {

			const QModelIndex index = filterModel_->mapToSource(selected_item);

			// do the search for this region!
			if (auto region = *reinterpret_cast<const std::shared_ptr<IRegion> *>(index.internalPointer())) {

				analyzer->analyze(region);

				const IAnalyzer::FunctionMap &results = analyzer->functions(region);
				for (const Function &function : results) {
					resultsDialog->addResult(function);
				}
			}
		}

		if (resultsDialog->resultCount() == 0) {
			QMessageBox::information(this, tr("No Results"), tr("No Functions Found!"));
			delete resultsDialog;
		} else {
			resultsDialog->show();
		}

		if (analyzer_object) {
			disconnect(analyzer_object, SIGNAL(updateProgress(int)), ui.progressBar, SLOT(setValue(int)));
		}
	}
}

}

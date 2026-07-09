/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogFunctions.h"
#include "DialogResults.h"
#include "IAnalyzer.h"
#include "IProcess.h"
#include "IRegion.h"
#include "MemoryRegions.h"
#include "edb.h"
#include "util/Math.h"

#include <QDialog>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QtConcurrent/QtConcurrentRun>
#include <QVector>

#include <vector>

namespace FunctionFinderPlugin {

/**
 * @brief Constructs a DialogFunctions object with the specified parent widget and window flags.
 *
 * @param parent The parent widget for this dialog.
 * @param f The window flags for this dialog.
 */
DialogFunctions::DialogFunctions(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);
	ui.tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	filterModel_ = new QSortFilterProxyModel(this);
	connect(ui.txtSearch, &QLineEdit::textChanged, filterModel_, &QSortFilterProxyModel::setFilterFixedString);

	buttonFind_ = new QPushButton(QIcon::fromTheme("edit-find"), tr("Find"));
	connect(buttonFind_, &QPushButton::clicked, this, &DialogFunctions::onFindClicked);
	connect(&searchWatcher_, &QFutureWatcher<SearchResult>::finished, this, &DialogFunctions::onFindFinished);

	progressTimer_.setInterval(50);
	connect(&progressTimer_, &QTimer::timeout, this, &DialogFunctions::updateProgressBar);

	ui.buttonBox->addButton(buttonFind_, QDialogButtonBox::ActionRole);
}

/**
 * @brief Handles the show event for the dialog, initializing the filter model and setting up the table view.
 */
void DialogFunctions::showEvent(QShowEvent *) {
	filterModel_->setFilterKeyColumn(3);
	filterModel_->setSourceModel(&edb::v1::memory_regions());
	ui.tableView->setModel(filterModel_);

	ui.progressBar->setValue(0);
}

void DialogFunctions::reject() {
	if (searchRunning_) {
		cancelRequested_.store(true);
		buttonFind_->setEnabled(false);
		buttonFind_->setText(tr("Cancelling..."));
		return;
	}

	QDialog::reject();
}

/**
 * @brief Performs the function search operation.
 */
DialogFunctions::SearchResult DialogFunctions::doFind(const std::vector<RegionScan> &regions) {
	SearchResult result;
	progressDone_.store(0);
	progressTotal_.store(static_cast<size_t>(regions.size()));

	if (IAnalyzer *const analyzer = edb::v1::analyzer()) {
		auto analyzer_object = dynamic_cast<QObject *>(analyzer);
		if (analyzer_object) {
			connect(analyzer_object, SIGNAL(updateProgress(int)), ui.progressBar, SLOT(setValue(int)));
		}

		for (const RegionScan &entry : regions) {
			if (cancelRequested_.load()) {
				result.cancelled = true;
				break;
			}

			analyzer->analyze(entry.region);
			const IAnalyzer::FunctionMap functions = analyzer->functions(entry.region);
			for (const Function &function : functions) {
				result.functions.push_back(function);
			}

			progressDone_.fetch_add(1);
		}

		if (analyzer_object) {
			disconnect(analyzer_object, SIGNAL(updateProgress(int)), ui.progressBar, SLOT(setValue(int)));
		}
	}

	return result;
}

void DialogFunctions::onFindClicked() {
	if (searchRunning_) {
		cancelRequested_.store(true);
		buttonFind_->setEnabled(false);
		buttonFind_->setText(tr("Cancelling..."));
		return;
	}

	const QItemSelectionModel *const selModel = ui.tableView->selectionModel();
	const QModelIndexList sel                 = selModel->selectedRows();

	if (sel.empty()) {
		QMessageBox::critical(this, tr("No Region Selected"), tr("You must select a region which is to be scanned for functions."));
		return;
	}

	std::vector<RegionScan> regions;
	regions.reserve(static_cast<size_t>(sel.size()));
	for (const QModelIndex &selected_item : sel) {
		const QModelIndex index = filterModel_->mapToSource(selected_item);
		if (auto region = *reinterpret_cast<const std::shared_ptr<IRegion> *>(index.internalPointer())) {
			regions.push_back({region});
		}
	}

	if (regions.empty()) {
		return;
	}

	ui.progressBar->setValue(0);
	cancelRequested_.store(false);
	progressDone_.store(0);
	progressTotal_.store(0);
	setSearchRunning(true);
	progressTimer_.start();
	searchWatcher_.setFuture(QtConcurrent::run([this, regions]() {
		return doFind(regions);
	}));
}

void DialogFunctions::onFindFinished() {
	progressTimer_.stop();
	updateProgressBar();

	const SearchResult result = searchWatcher_.result();
	setSearchRunning(false);

	if (result.cancelled) {
		return;
	}

	auto resultsDialog = new DialogResults(this);
	for (const Function &function : result.functions) {
		resultsDialog->addResult(function);
	}

	if (resultsDialog->resultCount() == 0) {
		QMessageBox::information(this, tr("No Results"), tr("No Functions Found!"));
		delete resultsDialog;
	} else {
		resultsDialog->show();
	}
}

void DialogFunctions::updateProgressBar() {
	const size_t done  = progressDone_.load();
	const size_t total = progressTotal_.load();

	if (total == 0) {
		ui.progressBar->setValue(0);
		return;
	}

	ui.progressBar->setValue(util::percentage(done, total));
}

void DialogFunctions::setSearchRunning(bool running) {
	searchRunning_ = running;

	if (searchRunning_) {
		buttonFind_->setEnabled(true);
		buttonFind_->setIcon(QIcon::fromTheme("process-stop"));
		buttonFind_->setText(tr("Cancel"));
	} else {
		buttonFind_->setEnabled(true);
		buttonFind_->setIcon(QIcon::fromTheme("edit-find"));
		buttonFind_->setText(tr("Find"));
		ui.progressBar->setValue(100);
	}
}

}

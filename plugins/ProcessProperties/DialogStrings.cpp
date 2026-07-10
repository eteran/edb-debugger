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
#include <QVector>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <numeric>

namespace ProcessPropertiesPlugin {

/**
 * @brief Constructor for the DialogStrings class.
 *
 * @param parent The parent widget for this dialog.
 * @param f The window flags for this dialog.
 */
DialogStrings::DialogStrings(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);
	ui.tableView->verticalHeader()->hide();
	ui.tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	filterModel_ = new QSortFilterProxyModel(this);
	connect(ui.txtSearch, &QLineEdit::textChanged, filterModel_, &QSortFilterProxyModel::setFilterFixedString);

	buttonFind_ = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-find")), tr("Find"));
	connect(buttonFind_, &QPushButton::clicked, this, &DialogStrings::onFindClicked);
	connect(&searchWatcher_, &QFutureWatcher<SearchResult>::finished, this, &DialogStrings::onFindFinished);

	progressTimer_.setInterval(50);
	connect(&progressTimer_, &QTimer::timeout, this, &DialogStrings::updateProgressBar);

	ui.buttonBox->addButton(buttonFind_, QDialogButtonBox::ActionRole);
}

/**
 * @brief Handles the show event for the dialog, initializing the filter model and setting up the table view.
 */
void DialogStrings::showEvent(QShowEvent *) {
	filterModel_->setFilterKeyColumn(3);
	filterModel_->setSourceModel(&edb::v1::memory_regions());
	ui.tableView->setModel(filterModel_);
	ui.progressBar->setValue(0);
}

/**
 * @brief Performs the string search operation within the selected memory regions.
 */
void DialogStrings::reject() {
	if (searchRunning_) {
		cancelRequested_.store(true);
		buttonFind_->setEnabled(false);
		buttonFind_->setText(tr("Cancelling..."));
		return;
	}

	QDialog::reject();
}

DialogStrings::SearchResult DialogStrings::doFind(const std::vector<RegionScan> &regions, bool searchUnicode, int minStringLength) {
	SearchResult result;
	progressDone_.store(0);
	progressTotal_.store(0);

	if (edb::v1::debugger_core == nullptr) {
		return result;
	}

	const size_t totalBytes = std::accumulate(regions.begin(), regions.end(), size_t(0), [](size_t total, const RegionScan &region) {
		return total + static_cast<size_t>(region.end - region.start);
	});
	progressTotal_.store(totalBytes);

	QString str;

	for (const RegionScan &region : regions) {

		if (cancelRequested_.load()) {
			result.cancelled = true;
			break;
		}

		if (!region.accessible) {
			progressDone_.fetch_add(static_cast<size_t>(region.end - region.start));
			continue;
		}

		edb::address_t start_address     = region.start;
		const edb::address_t end_address = region.end;
		const edb::address_t orig_start  = start_address;

		while (start_address < end_address) {

			if (cancelRequested_.load()) {
				result.cancelled = true;
				break;
			}

			int string_length = 0;
			bool ok           = edb::v1::get_ascii_string_at_address(start_address, str, minStringLength, 256, string_length);
			if (ok) {
				result.results.push_back({start_address, str, ResultsModel::Result::Ascii});
			} else if (searchUnicode) {
				string_length = 0;
				ok            = edb::v1::get_utf16_string_at_address(start_address, str, minStringLength, 256, string_length);
				if (ok) {
					result.results.push_back({start_address, str, ResultsModel::Result::Utf16});
				}
			}

			const size_t advance = ok ? static_cast<size_t>(string_length) : 1;
			progressDone_.fetch_add(advance);
			start_address += advance;
		}
	}

	return result;
}

void DialogStrings::onFindClicked() {
	if (searchRunning_) {
		cancelRequested_.store(true);
		buttonFind_->setEnabled(false);
		buttonFind_->setText(tr("Cancelling..."));
		return;
	}

	const int min_string_length = edb::v1::config().min_string_length;
	const bool searchUnicode    = ui.search_unicode->isChecked();

	const QItemSelectionModel *const selection_model = ui.tableView->selectionModel();
	const QModelIndexList sel                        = selection_model->selectedRows();

	if (sel.empty()) {
		QMessageBox::critical(
			this,
			tr("No Region Selected"),
			tr("You must select a region which is to be scanned for strings."));
		return;
	}

	if (edb::v1::debugger_core == nullptr) {
		return;
	}

	edb::v1::memory_regions().sync();
	const QList<std::shared_ptr<IRegion>> currentRegions = edb::v1::memory_regions().regions();

	std::vector<RegionScan> regions;
	regions.reserve(static_cast<size_t>(currentRegions.size()));
	for (const std::shared_ptr<IRegion> &region : currentRegions) {
		regions.push_back({region->start(), region->end(), region->accessible()});
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
	searchWatcher_.setFuture(QtConcurrent::run([this, regions, searchUnicode, min_string_length]() {
		return doFind(regions, searchUnicode, min_string_length);
	}));
}

void DialogStrings::onFindFinished() {
	progressTimer_.stop();
	updateProgressBar();

	const SearchResult result = searchWatcher_.result();
	setSearchRunning(false);

	if (result.allocationFailed) {
		QMessageBox::critical(this, tr("Memory Allocation Error"), tr("Unable to satisfy memory allocation request for search string."));
		return;
	}

	if (result.cancelled) {
		return;
	}

	auto resultsDialog = new DialogResults(this);
	for (const ResultsModel::Result &r : result.results) {
		resultsDialog->addResult(r);
	}

	if (resultsDialog->resultCount() == 0) {
		QMessageBox::information(this, tr("No Strings Found"), tr("No strings were found in the selected region"));
		delete resultsDialog;
	} else {
		resultsDialog->show();
	}
}

void DialogStrings::updateProgressBar() {
	const size_t done  = progressDone_.load();
	const size_t total = progressTotal_.load();

	if (total == 0) {
		ui.progressBar->setValue(0);
		return;
	}

	ui.progressBar->setValue(util::percentage(done, total));
}

void DialogStrings::setSearchRunning(bool running) {
	searchRunning_ = running;

	if (searchRunning_) {
		buttonFind_->setEnabled(true);
		buttonFind_->setIcon(QIcon::fromTheme(QStringLiteral("process-stop")));
		buttonFind_->setText(tr("Cancel"));
	} else {
		buttonFind_->setEnabled(true);
		buttonFind_->setIcon(QIcon::fromTheme(QStringLiteral("edit-find")));
		buttonFind_->setText(tr("Find"));
		ui.progressBar->setValue(100);
	}
}

}

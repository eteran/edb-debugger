/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogBinaryString.h"
#include "DialogResults.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "IRegion.h"
#include "MemoryRegions.h"
#include "edb.h"
#include "util/Math.h"

#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVector>
#include <QtConcurrent/QtConcurrentRun>
#include <cstring>

namespace BinarySearcherPlugin {

/**
 * @brief Constructs the binary string search dialog and sets up its Find button and input widget.
 *
 * @param parent
 * @param f
 */
DialogBinaryString::DialogBinaryString(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);
	ui.progressBar->setValue(0);

	// NOTE(eteran): address issue #574
	ui.binaryString->setShowKeepSize(false);

	buttonFind_ = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-find")), tr("Find"));
	connect(buttonFind_, &QPushButton::clicked, this, &DialogBinaryString::onFindClicked);
	connect(&searchWatcher_, &QFutureWatcher<SearchResult>::finished, this, &DialogBinaryString::onFindFinished);

	progressTimer_.setInterval(50);
	connect(&progressTimer_, &QTimer::timeout, this, &DialogBinaryString::updateProgress);

	ui.buttonBox->addButton(buttonFind_, QDialogButtonBox::ActionRole);
}

/**
 * @brief Searches all mapped memory regions for occurrences of the entered binary pattern.
 *
 * @param b The binary pattern to search for.
 * @param process The process to search in.
 * @param regions The memory regions to search.
 * @param page_size The size of a memory page.
 * @param align The alignment to use when searching.
 * @param skipNoAccess Whether to skip regions that are not accessible.
 * @return SearchResult The result of the search, including matches and status flags.
 */
DialogBinaryString::SearchResult DialogBinaryString::doFind(
	const QByteArray &b,
	const IProcess *process,
	const std::vector<RegionScan> &regions,
	size_t page_size,
	edb::address_t align,
	bool skipNoAccess) {

	SearchResult result;
	progressDone_.store(0);
	progressTotal_.store(0);

	if (process == nullptr || page_size == 0) {
		return result;
	}

	constexpr size_t MaxPages = 4096; // how many pages our read chunks are.
	const auto sz             = static_cast<size_t>(b.size());

	if (sz == 0) {
		return result;
	}

	size_t total_pages = 0;
	for (const RegionScan &region : regions) {
		total_pages += region.size / page_size;
	}
	progressTotal_.store(total_pages);

	for (const RegionScan &region : regions) {

		if (cancelRequested_.load()) {
			result.cancelled = true;
			break;
		}

		const size_t region_size = region.size;
		const size_t page_count  = region_size / page_size;

		// a short circuit for speeding things up
		if (skipNoAccess && !region.accessible) {
			progressDone_.fetch_add(page_count);
			continue;
		}

		// Read out 4096 pages at a time, as some applications have huge regions
		// and we will push against memory fragmentation if we mirror those regions.
		// To prevent missing a needle in the haystack if it is split between read
		// boundaries. increment current_page by only 4095.
		for (size_t current_page = 0; current_page < page_count; current_page += MaxPages - 1) {

			if (cancelRequested_.load()) {
				result.cancelled = true;
				break;
			}

			const size_t chunk_pages = std::min(MaxPages, page_count - current_page);

			try {
				QVector<uint8_t> pages(static_cast<int>(chunk_pages * page_size));

				if (process->readPages(region.start + (current_page * page_size), pages.data(), chunk_pages)) {
					const uint8_t *p               = pages.constData();
					const uint8_t *const pages_end = p + pages.size() - static_cast<qptrdiff>(sz);

					while (p <= pages_end) {

						if (std::memcmp(p, b.constData(), sz) == 0) {
							const edb::address_t addr = (p - pages.constData()) + region.start + (current_page * page_size);
							result.matches.push_back(addr);
						}

						p += align;
					}
				}
			} catch (const std::bad_alloc &) {
				result.allocationFailed = true;
				return result;
			}

			progressDone_.fetch_add(chunk_pages);
		}

		if (result.cancelled) {
			break;
		}
	}

	return result;
}

/**
 * @brief Initiates the search for the binary string in the specified memory regions.
 */
void DialogBinaryString::onFindClicked() {
	if (searchRunning_) {
		cancelRequested_.store(true);
		buttonFind_->setEnabled(false);
		buttonFind_->setText(tr("Cancelling..."));
		return;
	}

	const QByteArray needle = ui.binaryString->value();
	const auto sz           = static_cast<size_t>(needle.size());

	constexpr size_t MaxPages = 4096;
	if (sz == 0) {
		return;
	}

	if (!edb::v1::debugger_core) {
		return;
	}

	const size_t page_size = edb::v1::debugger_core->pageSize();
	if (sz > MaxPages * page_size) {
		QMessageBox::information(nullptr, tr("Input String Too Large"), tr("The search string is too large."));
		return;
	}

	edb::v1::memory_regions().sync();
	const QList<std::shared_ptr<IRegion>> current_regions = edb::v1::memory_regions().regions();

	std::vector<RegionScan> regions;
	regions.reserve(static_cast<size_t>(current_regions.size()));
	for (const std::shared_ptr<IRegion> &region : current_regions) {
		regions.push_back({region->start(), region->size(), region->accessible()});
	}

	const IProcess *process    = edb::v1::debugger_core->process();
	const bool skipNoAccess    = ui.chkSkipNoAccess->isChecked();
	const edb::address_t align = ui.chkAlignment->isChecked() ? 1 << (ui.cmbAlignment->currentIndex() + 1) : 1;

	ui.progressBar->setValue(0);
	cancelRequested_.store(false);
	progressDone_.store(0);
	progressTotal_.store(0);
	setSearchRunning(true);

	progressTimer_.start();
	searchWatcher_.setFuture(QtConcurrent::run([this, needle, process, regions, page_size, align, skipNoAccess]() {
		return doFind(needle, process, regions, page_size, align, skipNoAccess);
	}));
}

/**
 * @brief Handles the completion of the binary string search and displays the results.
 */
void DialogBinaryString::onFindFinished() {
	progressTimer_.stop();
	updateProgress();

	const SearchResult result = searchWatcher_.result();
	setSearchRunning(false);

	if (result.allocationFailed) {
		QMessageBox::critical(
			nullptr,
			tr("Memory Allocation Error"),
			tr("Unable to satisfy memory allocation request for requested region"));
		return;
	}

	if (result.cancelled) {
		return;
	}

	auto results = new DialogResults(this);
	results->setAttribute(Qt::WA_DeleteOnClose);

	for (const edb::address_t match : result.matches) {
		results->addResult(DialogResults::RegionType::Data, match);
	}

	if (results->resultCount() == 0) {
		QMessageBox::information(nullptr, tr("No Results"), tr("No Results were found!"));
		delete results;
	} else {
		results->show();
	}
}

/**
 * @brief Updates the progress bar based on the current progress of the search.
 */
void DialogBinaryString::updateProgress() {
	const size_t done  = progressDone_.load();
	const size_t total = progressTotal_.load();

	if (total == 0) {
		ui.progressBar->setValue(0);
		return;
	}

	ui.progressBar->setValue(util::percentage(done, total));
}

/**
 * @brief Sets the search running state and updates the UI accordingly.
 *
 * @param running True if the search is running, false otherwise.
 */
void DialogBinaryString::setSearchRunning(bool running) {
	searchRunning_ = running;

	if (searchRunning_) {
		buttonFind_->setEnabled(true);
		buttonFind_->setIcon(QIcon::fromTheme(QStringLiteral("dialog-close")));
		buttonFind_->setText(tr("Cancel"));
	} else {
		buttonFind_->setEnabled(true);
		buttonFind_->setIcon(QIcon::fromTheme(QStringLiteral("edit-find")));
		buttonFind_->setText(tr("Find"));
		ui.progressBar->setValue(100);
	}
}

/**
 * @brief Handles the rejection of the dialog, allowing for cancellation of an ongoing search.
 */
void DialogBinaryString::reject() {
	if (searchRunning_) {
		cancelRequested_.store(true);
		buttonFind_->setEnabled(false);
		buttonFind_->setText(tr("Cancelling..."));
		return;
	}

	QDialog::reject();
}

}

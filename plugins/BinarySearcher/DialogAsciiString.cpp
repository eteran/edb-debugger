/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogAsciiString.h"
#include "DialogResults.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "IRegion.h"
#include "IThread.h"
#include "MemoryRegions.h"
#include "State.h"
#include "edb.h"
#include "util/Math.h"

#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVector>
#include <QtConcurrent/QtConcurrentRun>
#include <QtDebug>

#include <cstring>

namespace BinarySearcherPlugin {

/**
 * @brief Constructs the ASCII string search dialog and sets up its Find button.
 *
 * @param parent The parent widget of the dialog.
 * @param f The window flags for the dialog.
 */
DialogAsciiString::DialogAsciiString(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);
	ui.progressBar->setValue(0);

	buttonFind_ = new QPushButton(QIcon::fromTheme("edit-find"), tr("Find"));
	connect(buttonFind_, &QPushButton::clicked, this, &DialogAsciiString::onFindClicked);
	connect(&searchWatcher_, &QFutureWatcher<SearchResult>::finished, this, &DialogAsciiString::onFindFinished);

	progressTimer_.setInterval(50);
	connect(&progressTimer_, &QTimer::timeout, this, &DialogAsciiString::updateProgress);

	ui.buttonBox->addButton(buttonFind_, QDialogButtonBox::ActionRole);
}

/**
 * @brief Searches stack memory for stack-aligned pointers that point to the entered ASCII string.
 *
 * @param b The ASCII string to search for.
 * @param process The process to search in.
 * @param stack_ptr The starting address of the stack region to search.
 * @param stack_end The ending address of the stack region to search.
 * @return SearchResult The result of the search, including matches and status flags.
 */
DialogAsciiString::SearchResult DialogAsciiString::doFind(
	const QByteArray &b,
	const IProcess *process,
	edb::address_t stack_ptr,
	edb::address_t stack_end) {
	SearchResult result;

	progressDone_.store(0);
	progressTotal_.store(0);

	if (process == nullptr || stack_end <= stack_ptr) {
		return result;
	}

	const auto sz = static_cast<size_t>(b.size());
	if (sz == 0) {
		return result;
	}

	edb::address_t count = (stack_end - stack_ptr) / edb::v1::pointer_size();
	progressTotal_.store(static_cast<size_t>(count));

	try {
		std::vector<uint8_t> chars(sz);

		while (stack_ptr < stack_end) {

			if (cancelRequested_.load()) {
				result.cancelled = true;
				break;
			}

			// get the value from the stack
			edb::address_t stack_address;

			if (process->readBytes(stack_ptr, &stack_address, edb::v1::pointer_size())) {
				if (process->readBytes(stack_address, chars.data(), chars.size())) {
					if (std::memcmp(chars.data(), b.constData(), chars.size()) == 0) {
						result.matches.push_back(stack_ptr);
					}
				}
			}

			progressDone_.fetch_add(1);
			stack_ptr += edb::v1::pointer_size();
		}

	} catch (const std::bad_alloc &) {
		result.allocationFailed = true;
	}

	return result;
}

/**
 * @brief Initiates the search for the ASCII string in stack memory.
 *
 * If a search is already running, it requests cancellation of the current search.
 * Otherwise, it retrieves the ASCII string from the input field, synchronizes memory regions,
 */
void DialogAsciiString::onFindClicked() {
	if (searchRunning_) {
		cancelRequested_.store(true);
		buttonFind_->setEnabled(false);
		buttonFind_->setText(tr("Cancelling..."));
		return;
	}

	ui.progressBar->setValue(0);
	cancelRequested_.store(false);
	progressDone_.store(0);
	progressTotal_.store(0);

	const QByteArray needle = ui.txtAscii->text().toLatin1();
	const IProcess *process = nullptr;
	edb::address_t stack_start;
	edb::address_t stack_end;

	if (!needle.isEmpty()) {
		edb::v1::memory_regions().sync();

		process = edb::v1::debugger_core ? edb::v1::debugger_core->process() : nullptr;
		if (process) {
			if (std::shared_ptr<IThread> thread = process->currentThread()) {
				State state;
				thread->getState(&state);

				if (std::shared_ptr<IRegion> region = edb::v1::memory_regions().findRegion(state.stackPointer())) {
					stack_start = region->start();
					stack_end   = region->end();
				}
			}
		}
	}

	setSearchRunning(true);
	progressTimer_.start();
	searchWatcher_.setFuture(QtConcurrent::run([this, needle, process, stack_start, stack_end]() {
		return doFind(needle, process, stack_start, stack_end);
	}));
}

/**
 * @brief Handles the completion of the ASCII string search and displays the results.
 */
void DialogAsciiString::onFindFinished() {
	progressTimer_.stop();
	updateProgress();

	const SearchResult result = searchWatcher_.result();
	setSearchRunning(false);

	if (result.allocationFailed) {
		QMessageBox::critical(
			nullptr,
			tr("Memory Allocation Error"),
			tr("Unable to satisfy memory allocation request for search string."));
		return;
	}

	if (result.cancelled) {
		return;
	}

	auto results = new DialogResults(this);
	for (edb::address_t match : result.matches) {
		results->addResult(DialogResults::RegionType::Stack, match);
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
void DialogAsciiString::updateProgress() {
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
void DialogAsciiString::setSearchRunning(bool running) {
	searchRunning_ = running;

	if (searchRunning_) {
		buttonFind_->setEnabled(true);
		buttonFind_->setIcon(QIcon::fromTheme("dialog-close"));
		buttonFind_->setText(tr("Cancel"));
	} else {
		buttonFind_->setEnabled(true);
		buttonFind_->setIcon(QIcon::fromTheme("edit-find"));
		buttonFind_->setText(tr("Find"));
		ui.progressBar->setValue(100);
	}
}

/**
 * @brief Handles the rejection of the dialog, allowing for cancellation of an ongoing search.
 */
void DialogAsciiString::reject() {
	if (searchRunning_) {
		cancelRequested_.store(true);
		buttonFind_->setEnabled(false);
		buttonFind_->setText(tr("Cancelling..."));
		return;
	}

	QDialog::reject();
}

/**
 * @brief Focuses the ASCII input field when the dialog becomes visible.
 *
 * @param event The show event.
 */
void DialogAsciiString::showEvent(QShowEvent *event) {
	Q_UNUSED(event)
	ui.txtAscii->setFocus();
}

}

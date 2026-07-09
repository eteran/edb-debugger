/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogReferences.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "MemoryRegions.h"
#include "edb.h"
#include "util/Math.h"

#include <QMessageBox>
#include <QPushButton>
#include <QVector>
#include <QtConcurrent/QtConcurrentRun>

namespace ReferencesPlugin {

enum Role {
	TypeRole    = Qt::UserRole + 0,
	AddressRole = Qt::UserRole + 1
};

/**
 * @brief Constructor for the DialogReferences class.
 *
 * @param parent The parent widget for the dialog.
 * @param f The window flags for the dialog.
 */
DialogReferences::DialogReferences(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);

	buttonFind_ = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-find")), tr("Find"));
	connect(buttonFind_, &QPushButton::clicked, this, &DialogReferences::onFindClicked);
	connect(&searchWatcher_, &QFutureWatcher<SearchResult>::finished, this, &DialogReferences::onFindFinished);

	progressTimer_.setInterval(50);
	connect(&progressTimer_, &QTimer::timeout, this, &DialogReferences::updateProgressBar);

	ui.buttonBox->addButton(buttonFind_, QDialogButtonBox::ActionRole);
}

/**
 * @brief Handles the show event for the dialog.
 *
 * @param event The show event.
 */
void DialogReferences::showEvent(QShowEvent *) {
	ui.listWidget->clear();
	ui.progressBar->setValue(0);
}

void DialogReferences::reject() {
	if (searchRunning_) {
		cancelRequested_.store(true);
		buttonFind_->setEnabled(false);
		buttonFind_->setText(tr("Cancelling..."));
		return;
	}

	QDialog::reject();
}

/**
 * @brief Performs the search for references based on the address entered in the dialog.
 */
DialogReferences::SearchResult DialogReferences::doFind(edb::address_t address, const IProcess *process, const std::vector<RegionScan> &regions, bool skipNoAccess) {
	SearchResult result;
	progressDone_.store(0);
	progressTotal_.store(0);

	if (process == nullptr) {
		return result;
	}

	const size_t page_size    = edb::v1::debugger_core->pageSize();
	const size_t pointer_size = edb::v1::pointer_size();

	const size_t totalBytes = std::accumulate(regions.begin(), regions.end(), size_t(0), [](size_t total, const RegionScan &region) {
		return total + static_cast<size_t>(region.end - region.start);
	});
	progressTotal_.store(totalBytes);

	for (const RegionScan &region : regions) {

		if (cancelRequested_.load()) {
			result.cancelled = true;
			break;
		}

		if (skipNoAccess && !region.accessible) {
			progressDone_.fetch_add(static_cast<size_t>(region.end - region.start));
			continue;
		}

		const size_t page_count      = static_cast<size_t>(region.end - region.start) / page_size;
		const QVector<uint8_t> pages = edb::v1::read_pages(region.start, page_count);

		if (!pages.isEmpty()) {
			const uint8_t *p               = pages.data();
			const uint8_t *const pages_end = pages.data() + (region.end - region.start);

			while (p != pages_end) {

				if (cancelRequested_.load()) {
					result.cancelled = true;
					break;
				}

				if (pages_end - p < static_cast<int>(pointer_size)) {
					break;
				}

				const edb::address_t addr = p - pages.data() + region.start;

				edb::address_t test_address(0);
				memcpy(&test_address, p, pointer_size);

				if (test_address == address) {
					result.matches.push_back({'D', addr});
				}

				edb::Instruction inst(p, pages_end, addr);

				if (inst) {
					switch (inst.operation()) {
					case X86_INS_MOV:
						Q_ASSERT(inst.operandCount() == 2);

						if (is_expression(inst[0])) {
							if (is_immediate(inst[1]) && static_cast<edb::address_t>(inst[1]->imm) == address) {
								result.matches.push_back({'C', addr});
							}
						}
						break;
					case X86_INS_PUSH:
						Q_ASSERT(inst.operandCount() == 1);

						if (is_immediate(inst[0]) && static_cast<edb::address_t>(inst[0]->imm) == address) {
							result.matches.push_back({'C', addr});
						}
						break;
					default:
						if (is_jump(inst) || is_call(inst)) {
							if (is_immediate(inst[0])) {
								if (static_cast<edb::address_t>(inst[0]->imm) == address) {
									result.matches.push_back({'C', addr});
								}
							}
						}
						break;
					}
				}

				progressDone_.fetch_add(1);
				++p;
			}
		}
	}

	return result;
}

void DialogReferences::onFindClicked() {
	if (searchRunning_) {
		cancelRequested_.store(true);
		buttonFind_->setEnabled(false);
		buttonFind_->setText(tr("Cancelling..."));
		return;
	}

	ui.listWidget->clear();
	ui.progressBar->setValue(0);
	cancelRequested_.store(false);
	progressDone_.store(0);
	progressTotal_.store(0);

	bool ok = false;
	edb::address_t address;
	const QString text = ui.txtAddress->text();
	if (!text.isEmpty()) {
		ok = edb::v1::eval_expression(text, &address);
	}

	if (!ok) {
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

	setSearchRunning(true);
	progressTimer_.start();
	const IProcess *process = edb::v1::debugger_core->process();
	const bool skipNoAccess = ui.chkSkipNoAccess->isChecked();
	searchWatcher_.setFuture(QtConcurrent::run([this, address, process, regions, skipNoAccess]() {
		return doFind(address, process, regions, skipNoAccess);
	}));
}

void DialogReferences::onFindFinished() {
	progressTimer_.stop();
	updateProgressBar();

	const SearchResult result = searchWatcher_.result();
	setSearchRunning(false);

	if (result.cancelled) {
		return;
	}

	for (const ReferenceResult &reference : result.matches) {
		auto item = new QListWidgetItem(edb::v1::format_pointer(reference.address));
		item->setData(TypeRole, reference.type);
		item->setData(AddressRole, reference.address.toQVariant());
		ui.listWidget->addItem(item);
	}

	if (ui.listWidget->count() == 0) {
		QMessageBox::information(this, tr("No Results"), tr("No References were found!"));
	}
}

void DialogReferences::updateProgressBar() {
	const size_t done  = progressDone_.load();
	const size_t total = progressTotal_.load();

	if (total == 0) {
		ui.progressBar->setValue(0);
		return;
	}

	ui.progressBar->setValue(util::percentage(done, total));
}

void DialogReferences::setSearchRunning(bool running) {
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

/**
 * @brief Handles the event when an item in the list widget is double-clicked. Depending on the type of the item, it either dumps data at the address or jumps to that address.
 *
 * @param item The list widget item that was double-clicked.
 */
void DialogReferences::on_listWidget_itemDoubleClicked(QListWidgetItem *item) {
	const edb::address_t addr = item->data(AddressRole).toULongLong();
	if (item->data(TypeRole).toChar() == 'D') {
		edb::v1::dump_data(addr, false);
	} else {
		edb::v1::jump_to_address(addr);
	}
}

}

/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogBinaryString.h"
#include "DialogResults.h"
#include "IDebugger.h"
#include "IRegion.h"
#include "MemoryRegions.h"
#include "edb.h"
#include "util/Math.h"

#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVector>
#include <cstring>

namespace BinarySearcherPlugin {

/**
 * @brief DialogBinaryString::DialogBinaryString
 * @param parent
 * @param f
 */
DialogBinaryString::DialogBinaryString(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);
	ui.progressBar->setValue(0);

	// NOTE(eteran): address issue #574
	ui.binaryString->setShowKeepSize(false);

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
 * @brief DialogBinaryString::doFind
 */
void DialogBinaryString::doFind() {
	const QByteArray b = ui.binaryString->value();

	if (!results_) {
		results_ = new DialogResults(this);
		results_->setAttribute(Qt::WA_DeleteOnClose);
	}

	const edb::address_t align = ui.chkAlignment->isChecked() ? 1 << (ui.cmbAlignment->currentIndex() + 1) : 1;
	const size_t sz            = b.size();
	edb::v1::memory_regions().sync();
	const QList<std::shared_ptr<IRegion>> regions = edb::v1::memory_regions().regions();
	const size_t page_size                        = edb::v1::debugger_core->pageSize();

	int i = 0;

	const size_t max_number_pages = 4096; // how many pages our read chunks are.

	if (sz == 0) {
		return;
	}
	if (sz > max_number_pages * page_size) {
		QMessageBox::information(nullptr, tr("Input String Too Large"), tr("The search string is too large."));
		return;
	}

	for (const std::shared_ptr<IRegion> &region : regions) {
		const size_t region_size = region->size();

		// a short circuit for speeding things up
		if (ui.chkSkipNoAccess->isChecked() && !region->accessible()) {
			ui.progressBar->setValue(util::percentage(++i, regions.size()));
			continue;
		}

		const size_t page_count = region_size / page_size;
		// Read out 4096 pages at a time, as some applications have huge regions
		// and we will push against memory fragmentation if we mirror those regions.
		// To prevent missing a needle in the haystack if it is split between read
		// boundaries. increment current_page by only 4095.
		for (size_t current_page = 0; current_page < page_count; current_page += max_number_pages - 1) {
			const QVector<uint8_t> pages = edb::v1::read_pages(
				region->start() + (current_page * page_size),
				std::min(max_number_pages, page_count - current_page));

			if (!pages.isEmpty()) {

				const uint8_t *p               = &pages.constFirst();
				const uint8_t *const pages_end = &pages.constLast() - sz;

				while (p < pages_end) {
					// compare values..
					if (std::memcmp(p, b.constData(), sz) == 0) {
						const edb::address_t addr = p - pages.data() + region->start() + (current_page * page_size);
						results_->addResult(DialogResults::RegionType::Data, addr);
					}

					// update progress bar every 64KB
					if ((reinterpret_cast<uint64_t>(p) & 0xFFFF) == 0) {
						ui.progressBar->setValue(util::percentage(i, regions.size(), p - pages.data(), region_size));
					}

					p += align;
				}
			}
		}
		++i;
	}

	if (results_->resultCount() == 0) {
		QMessageBox::information(nullptr, tr("No Results"), tr("No Results were found!"));
	} else {
		results_->show();
	}
}

}

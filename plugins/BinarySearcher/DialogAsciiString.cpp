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
#include <QtDebug>

#include <cstring>

namespace BinarySearcherPlugin {

/**
 * @brief DialogAsciiString::DialogAsciiString
 * @param parent
 * @param f
 */
DialogAsciiString::DialogAsciiString(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {
	ui.setupUi(this);
	ui.progressBar->setValue(0);

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
 * @brief DialogAsciiString::doFind
 *
 * find *stack aligned pointers* to exact string matches
 */
void DialogAsciiString::doFind() {

	const QByteArray b = ui.txtAscii->text().toLatin1();
	auto results       = new DialogResults(this);

	const auto sz = static_cast<size_t>(b.size());
	if (sz != 0) {

		edb::v1::memory_regions().sync();

		if (IProcess *process = edb::v1::debugger_core->process()) {
			if (std::shared_ptr<IThread> thread = process->currentThread()) {

				State state;
				thread->getState(&state);
				edb::address_t stack_ptr = state.stackPointer();

				if (std::shared_ptr<IRegion> region = edb::v1::memory_regions().findRegion(stack_ptr)) {

					edb::address_t count = (region->end() - stack_ptr) / edb::v1::pointer_size();
					stack_ptr            = region->start();

					try {
						std::vector<uint8_t> chars(sz);

						int i = 0;
						while (stack_ptr < region->end()) {

							// get the value from the stack
							edb::address_t stack_address;

							if (process->readBytes(stack_ptr, &stack_address, edb::v1::pointer_size())) {
								if (process->readBytes(stack_address, chars.data(), chars.size())) {
									if (std::memcmp(chars.data(), b.constData(), chars.size()) == 0) {
										results->addResult(DialogResults::RegionType::Stack, stack_ptr);
									}
								}
							}
							ui.progressBar->setValue(util::percentage(i++, count));
							stack_ptr += edb::v1::pointer_size();
						}

					} catch (const std::bad_alloc &) {
						QMessageBox::critical(
							nullptr,
							tr("Memroy Allocation Error"),
							tr("Unable to satisfy memory allocation request for search string."));
					}
				}
			}
		}
	}

	if (results->resultCount() == 0) {
		QMessageBox::information(nullptr, tr("No Results"), tr("No Results were found!"));
		delete results;
	} else {
		results->show();
	}
}

/**
 * @brief DialogAsciiString::showEvent
 * @param event
 */
void DialogAsciiString::showEvent(QShowEvent *event) {
	Q_UNUSED(event)
	ui.txtAscii->setFocus();
}

}

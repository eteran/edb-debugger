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

#include "DialogReferences.h"
#include "IDebugger.h"
#include "MemoryRegions.h"
#include "edb.h"
#include "util/Math.h"

#include <QMessageBox>
#include <QPushButton>
#include <QVector>

namespace ReferencesPlugin {

enum Role {
	TypeRole    = Qt::UserRole + 0,
	AddressRole = Qt::UserRole + 1
};

/**
 * @brief DialogReferences::DialogReferences
 * @param parent
 * @param f
 */
DialogReferences::DialogReferences(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);
	connect(this, &DialogReferences::updateProgress, ui.progressBar, &QProgressBar::setValue);

	buttonFind_ = new QPushButton(QIcon::fromTheme("edit-find"), tr("Find"));
	connect(buttonFind_, &QPushButton::clicked, this, [this]() {
		buttonFind_->setEnabled(false);
		ui.progressBar->setValue(0);
		ui.listWidget->clear();
		doFind();
		ui.progressBar->setValue(100);
		buttonFind_->setEnabled(true);
	});

	ui.buttonBox->addButton(buttonFind_, QDialogButtonBox::ActionRole);
}

/**
 * @brief DialogReferences::showEvent
 */
void DialogReferences::showEvent(QShowEvent *) {
	ui.listWidget->clear();
	ui.progressBar->setValue(0);
}

/**
 * @brief DialogReferences::doFind
 */
void DialogReferences::doFind() {
	bool ok = false;
	edb::address_t address;
	const size_t page_size = edb::v1::debugger_core->pageSize();

	const QString text = ui.txtAddress->text();
	if (!text.isEmpty()) {
		ok = edb::v1::eval_expression(text, &address);
	}

	if (ok) {
		edb::v1::memory_regions().sync();
		const QList<std::shared_ptr<IRegion>> regions = edb::v1::memory_regions().regions();

		int i = 0;
		for (const std::shared_ptr<IRegion> &region : regions) {
			// a short circut for speading things up
			if (region->accessible() || !ui.chkSkipNoAccess->isChecked()) {

				const size_t page_count      = region->size() / page_size;
				const QVector<uint8_t> pages = edb::v1::read_pages(region->start(), page_count);

				if (!pages.isEmpty()) {
					const uint8_t *p               = &pages[0];
					const uint8_t *const pages_end = &pages[0] + region->size();

					while (p != pages_end) {

						if (pages_end - p < static_cast<int>(edb::v1::pointer_size())) {
							break;
						}

						const edb::address_t addr = p - &pages[0] + region->start();

						edb::address_t test_address(0);
						memcpy(&test_address, p, edb::v1::pointer_size());

						if (test_address == address) {
							auto item = new QListWidgetItem(edb::v1::format_pointer(addr));
							item->setData(TypeRole, 'D');
							item->setData(AddressRole, addr.toQVariant());
							ui.listWidget->addItem(item);
						}

						edb::Instruction inst(p, pages_end, addr);

						if (inst) {
							switch (inst.operation()) {
							case X86_INS_MOV:
								// instructions of the form: mov [ADDR], 0xNNNNNNNN
								Q_ASSERT(inst.operandCount() == 2);

								if (is_expression(inst[0])) {
									if (is_immediate(inst[1]) && static_cast<edb::address_t>(inst[1]->imm) == address) {
										auto item = new QListWidgetItem(edb::v1::format_pointer(addr));
										item->setData(TypeRole, 'C');
										item->setData(AddressRole, addr.toQVariant());
										ui.listWidget->addItem(item);
									}
								}

								break;
							case X86_INS_PUSH:
								// instructions of the form: push 0xNNNNNNNN
								Q_ASSERT(inst.operandCount() == 1);

								if (is_immediate(inst[0]) && static_cast<edb::address_t>(inst[0]->imm) == address) {
									auto item = new QListWidgetItem(edb::v1::format_pointer(addr));
									item->setData(TypeRole, 'C');
									item->setData(AddressRole, addr.toQVariant());
									ui.listWidget->addItem(item);
								}
								break;
							default:
								if (is_jump(inst) || is_call(inst)) {
									if (is_immediate(inst[0])) {
										if (static_cast<edb::address_t>(inst[0]->imm) == address) {
											auto item = new QListWidgetItem(edb::v1::format_pointer(addr));
											item->setData(TypeRole, 'C');
											item->setData(AddressRole, addr.toQVariant());
											ui.listWidget->addItem(item);
										}
									}
								}
								break;
							}
						}

						Q_EMIT updateProgress(util::percentage(i, regions.size(), p - &pages[0], region->size()));
						++p;
					}
				}

			} else {
				Q_EMIT updateProgress(util::percentage(i, regions.size()));
			}
			++i;
		}
	}
}

/**
 * @brief DialogReferences::on_listWidget_itemDoubleClicked
 *
 * follows the found item in the data view
 *
 * @param item
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

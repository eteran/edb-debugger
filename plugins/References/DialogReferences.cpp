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
#include "Util.h"
#include "edb.h"

#include <QMessageBox>
#include <QVector>

#include "ui_DialogReferences.h"

namespace References {

enum Role {
	TypeRole    = Qt::UserRole + 0,
	AddressRole = Qt::UserRole + 1
};

//------------------------------------------------------------------------------
// Name: DialogReferences
// Desc: constructor
//------------------------------------------------------------------------------
DialogReferences::DialogReferences(QWidget *parent) : QDialog(parent), ui(new Ui::DialogReferences) {
	ui->setupUi(this);
	connect(this, SIGNAL(updateProgress(int)), ui->progressBar, SLOT(setValue(int)));
}

//------------------------------------------------------------------------------
// Name: ~DialogReferences
// Desc:
//------------------------------------------------------------------------------
DialogReferences::~DialogReferences() {
	delete ui;
}

//------------------------------------------------------------------------------
// Name: showEvent
// Desc:
//------------------------------------------------------------------------------
void DialogReferences::showEvent(QShowEvent *) {
	ui->listWidget->clear();
	ui->progressBar->setValue(0);
}

//------------------------------------------------------------------------------
// Name: do_find
// Desc:
//------------------------------------------------------------------------------
void DialogReferences::do_find() {
	bool ok;
	const edb::address_t address   = edb::v1::string_to_address(ui->txtAddress->text(), &ok);
	const edb::address_t page_size = edb::v1::debugger_core->page_size();

	if(ok) {
		edb::v1::memory_regions().sync();
		const QList<IRegion::pointer> regions = edb::v1::memory_regions().regions();

		int i = 0;
		for(const IRegion::pointer &region: regions) {
			// a short circut for speading things up
			if(region->accessible() || !ui->chkSkipNoAccess->isChecked()) {

				const edb::address_t page_count = region->size() / page_size;
				const QVector<quint8> pages = edb::v1::read_pages(region->start(), page_count);

				if(!pages.isEmpty()) {
					const quint8 *p = &pages[0];
					const quint8 *const pages_end = &pages[0] + region->size();

					while(p != pages_end) {

						if(pages_end - p < edb::v1::pointer_size()) {
							break;
						}

						const edb::address_t addr = p - &pages[0] + region->start();

						edb::address_t test_address(0);
						memcpy(&test_address, p, edb::v1::pointer_size());

						if(test_address == address) {
							auto item = new QListWidgetItem(edb::v1::format_pointer(addr));
							item->setData(TypeRole, 'D');
							item->setData(AddressRole, addr);
							ui->listWidget->addItem(item);
						}

						edb::Instruction inst(p, pages_end, addr);

						if(inst) {
							switch(inst.operation()) {
							case edb::Instruction::Operation::X86_INS_MOV:
								// instructions of the form: mov [ADDR], 0xNNNNNNNN
								Q_ASSERT(inst.operand_count() == 2);

								if(inst.operands()[0].general_type() == edb::Operand::TYPE_EXPRESSION) {
									if(inst.operands()[1].general_type() == edb::Operand::TYPE_IMMEDIATE && static_cast<edb::address_t>(inst.operands()[1].immediate()) == address) {
										auto item = new QListWidgetItem(edb::v1::format_pointer(addr));
										item->setData(TypeRole, 'C');
										item->setData(AddressRole, addr);
										ui->listWidget->addItem(item);
									}
								}

								break;
							case edb::Instruction::Operation::X86_INS_PUSH:
								// instructions of the form: push 0xNNNNNNNN
								Q_ASSERT(inst.operand_count() == 1);

								if(inst.operands()[0].general_type() == edb::Operand::TYPE_IMMEDIATE && static_cast<edb::address_t>(inst.operands()[0].immediate()) == address) {
									auto item = new QListWidgetItem(edb::v1::format_pointer(addr));
									item->setData(TypeRole, 'C');
									item->setData(AddressRole, addr);
									ui->listWidget->addItem(item);
								}
								break;
							default:
								if(is_jump(inst) || is_call(inst)) {
									if(inst.operands()[0].general_type() == edb::Operand::TYPE_REL) {
										if(inst.operands()[0].relative_target() == address) {
											auto item = new QListWidgetItem(edb::v1::format_pointer(addr));
											item->setData(TypeRole, 'C');
											item->setData(AddressRole, addr);
											ui->listWidget->addItem(item);
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

//------------------------------------------------------------------------------
// Name: on_btnFind_clicked
// Desc: find button event handler
//------------------------------------------------------------------------------
void DialogReferences::on_btnFind_clicked() {
	ui->btnFind->setEnabled(false);
	ui->progressBar->setValue(0);
	ui->listWidget->clear();
	do_find();
	ui->progressBar->setValue(100);
	ui->btnFind->setEnabled(true);
}

//------------------------------------------------------------------------------
// Name: on_listWidget_itemDoubleClicked
// Desc: follows the found item in the data view
//------------------------------------------------------------------------------
void DialogReferences::on_listWidget_itemDoubleClicked(QListWidgetItem *item) {
	const edb::address_t addr = item->data(AddressRole).toULongLong();
	if(item->data(TypeRole).toChar() == 'D') {
		edb::v1::dump_data(addr, false);
	} else {
		edb::v1::jump_to_address(addr);
	}
}

}

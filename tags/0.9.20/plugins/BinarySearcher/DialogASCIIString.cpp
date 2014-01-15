/*
Copyright (C) 2006 - 2014 Evan Teran
                          eteran@alum.rit.edu

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

#include "DialogASCIIString.h"
#include "edb.h"
#include "IDebuggerCore.h"
#include "MemoryRegions.h"
#include "State.h"
#include "Util.h"
#include <QMessageBox>
#include <QVector>
#include <cstring>

#include "ui_DialogASCIIString.h"

#include <QtDebug>

namespace BinarySearcher {

//------------------------------------------------------------------------------
// Name: DialogASCIIString
// Desc: constructor
//------------------------------------------------------------------------------
DialogASCIIString::DialogASCIIString(QWidget *parent) : QDialog(parent), ui(new Ui::DialogASCIIString) {
	ui->setupUi(this);
	ui->progressBar->setValue(0);
	ui->listWidget->clear();
}

//------------------------------------------------------------------------------
// Name: ~DialogASCIIString
// Desc:
//------------------------------------------------------------------------------
DialogASCIIString::~DialogASCIIString() {
	delete ui;
}

//------------------------------------------------------------------------------
// Name: do_find
// Desc: This will find *stack aligned pointers* to exact string matches
//------------------------------------------------------------------------------
void DialogASCIIString::do_find() {

	const QByteArray b = ui->txtASCII->text().toLatin1();
	ui->listWidget->clear();

	const int sz = b.size();
	if(sz != 0) {

		edb::v1::memory_regions().sync();
		
		State state;
		edb::v1::debugger_core->get_state(&state);
		edb::address_t stack_ptr = state.stack_pointer();

		if(IRegion::pointer region = edb::v1::memory_regions().find_region(stack_ptr)) {
				
			std::size_t count = (region->end() - stack_ptr) / sizeof(edb::address_t);
			stack_ptr = region->start();

			try {
				QVector<quint8> chars(sz);

				int i = 0;
				while(stack_ptr < region->end()) {
					// get the value from the stack
					edb::address_t value;
					if(edb::v1::debugger_core->read_bytes(stack_ptr, &value, sizeof(edb::address_t))) {
						if(edb::v1::debugger_core->read_bytes(value, &chars[0], sz)) {
							if(std::memcmp(&chars[0], b.constData(), sz) == 0) {
								QListWidgetItem *const item = new QListWidgetItem(edb::v1::format_pointer(stack_ptr));
								item->setData(Qt::UserRole, stack_ptr);
								ui->listWidget->addItem(item);
							}
						}
					}
					ui->progressBar->setValue(util::percentage(i++, count));
					stack_ptr += sizeof(edb::address_t);
				}
			} catch(const std::bad_alloc &) {
				QMessageBox::information(0, tr("Memroy Allocation Error"),
					tr("Unable to satisfy memory allocation request for search string."));
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_btnFind_clicked
// Desc: find button event handler
//------------------------------------------------------------------------------
void DialogASCIIString::on_btnFind_clicked() {

	ui->btnFind->setEnabled(false);
	ui->progressBar->setValue(0);
	do_find();
	ui->progressBar->setValue(100);
	ui->btnFind->setEnabled(true);
}

//------------------------------------------------------------------------------
// Name: on_listWidget_itemDoubleClicked
// Desc: follows the found item in the data view
//------------------------------------------------------------------------------
void DialogASCIIString::on_listWidget_itemDoubleClicked(QListWidgetItem *item) {
	const edb::address_t addr = item->data(Qt::UserRole).toULongLong();
	edb::v1::dump_stack(addr, true);
}

}

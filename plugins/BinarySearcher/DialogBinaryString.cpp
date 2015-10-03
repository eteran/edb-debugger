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

#include "DialogBinaryString.h"
#include "ByteShiftArray.h"
#include "edb.h"
#include "IDebugger.h"
#include "MemoryRegions.h"
#include "Util.h"
#include <QMessageBox>
#include <QVector>
#include <cstring>

#include "ui_DialogBinaryString.h"

namespace BinarySearcher {

//------------------------------------------------------------------------------
// Name: DialogBinaryString
// Desc: constructor
//------------------------------------------------------------------------------
DialogBinaryString::DialogBinaryString(QWidget *parent) : QDialog(parent), ui(new Ui::DialogBinaryString) {
	ui->setupUi(this);
	ui->progressBar->setValue(0);
	ui->listWidget->clear();
}

//------------------------------------------------------------------------------
// Name: ~DialogBinaryString
// Desc:
//------------------------------------------------------------------------------
DialogBinaryString::~DialogBinaryString() {
	delete ui;
}

//------------------------------------------------------------------------------
// Name: do_find
// Desc:
//------------------------------------------------------------------------------
void DialogBinaryString::do_find() {

	const QByteArray b = ui->binaryString->value();
	ui->listWidget->clear();

	const int sz = b.size();
	if(sz != 0) {
		ByteShiftArray bsa(sz);

		edb::v1::memory_regions().sync();
		const QList<IRegion::pointer> regions = edb::v1::memory_regions().regions();
		const edb::address_t page_size = edb::v1::debugger_core->page_size();

		int i = 0;
		for(const IRegion::pointer &region: regions) {

			bsa.clear();

			// a short circut for speading things up
			if(ui->chkSkipNoAccess->isChecked() && !region->accessible()) {
				ui->progressBar->setValue(util::percentage(++i, regions.size()));
				continue;
			}

			const size_t page_count     = region->size() / page_size;
			const QVector<quint8> pages = edb::v1::read_pages(region->start(), page_count);
			
			if(!pages.isEmpty()) {

				const quint8 *p = &pages[0];
				const quint8 *const pages_end = &pages[0] + region->size();
				
				QString temp;
				while(p != pages_end) {
					// shift in the next byte
					bsa << *p;

					// compare values..
					if(std::memcmp(bsa.data(), b.constData(), sz) == 0) {
						const edb::address_t addr = (p - &pages[0] + region->start()) - sz + 1;
						const edb::address_t align = 1 << (ui->cmbAlignment->currentIndex() + 1);

						if(!ui->chkAlignment->isChecked() || (addr % align) == 0) {
							auto item = new QListWidgetItem(edb::v1::format_pointer(addr));
							item->setData(Qt::UserRole, addr);
							ui->listWidget->addItem(item);
						}
					}

					ui->progressBar->setValue(util::percentage(i, regions.size(), p - &pages[0], region->size()));

					++p;
				}
			}
			++i;
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_btnFind_clicked
// Desc: find button event handler
//------------------------------------------------------------------------------
void DialogBinaryString::on_btnFind_clicked() {

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
void DialogBinaryString::on_listWidget_itemDoubleClicked(QListWidgetItem *item) {
	const edb::address_t addr = item->data(Qt::UserRole).toULongLong();
	edb::v1::dump_data(addr, false);
}

}

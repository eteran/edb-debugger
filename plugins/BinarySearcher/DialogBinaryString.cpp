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
#include "edb.h"
#include "IDebugger.h"
#include "IRegion.h"
#include "MemoryRegions.h"
#include "DialogResults.h"
#include "Util.h"
#include <QMessageBox>
#include <QVector>
#include <QListWidget>
#include <cstring>
#include <QPushButton>

namespace BinarySearcherPlugin {

//------------------------------------------------------------------------------
// Name: DialogBinaryString
// Desc: constructor
//------------------------------------------------------------------------------
DialogBinaryString::DialogBinaryString(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f) {
	ui.setupUi(this);
	ui.progressBar->setValue(0);

	// NOTE(eteran): address issue #574
	ui.binaryString->setShowKeepSize(false);

	btnFind_ = new QPushButton(QIcon::fromTheme("edit-find"), tr("Find"));
	connect(btnFind_, &QPushButton::clicked, this, [this]() {
		btnFind_->setEnabled(false);
		ui.progressBar->setValue(0);
		do_find();
		ui.progressBar->setValue(100);
		btnFind_->setEnabled(true);
	});

	ui.buttonBox->addButton(btnFind_, QDialogButtonBox::ActionRole);
}

//------------------------------------------------------------------------------
// Name: do_find
// Desc:
//------------------------------------------------------------------------------
void DialogBinaryString::do_find() {

	const QByteArray b = ui.binaryString->value();

	auto results = new DialogResults(this);

	const int sz = b.size();
	if(sz != 0) {
		edb::v1::memory_regions().sync();
		const QList<std::shared_ptr<IRegion>> regions = edb::v1::memory_regions().regions();
		const size_t page_size = edb::v1::debugger_core->page_size();

		int i = 0;
		for(const std::shared_ptr<IRegion> &region: regions) {
			const size_t region_size = region->size();

			// a short circut for speading things up
			if(ui.chkSkipNoAccess->isChecked() && !region->accessible()) {
				ui.progressBar->setValue(util::percentage(++i, regions.size()));
				continue;
			}

			const size_t page_count     = region_size / page_size;
			const QVector<uint8_t> pages = edb::v1::read_pages(region->start(), page_count);

			if(!pages.isEmpty()) {

				const uint8_t *p = &pages[0];
				const uint8_t *const pages_end = &pages[0] + region_size - sz;

				while(p < pages_end) {
					// compare values..
					if(std::memcmp(p, b.constData(), sz) == 0) {
						const edb::address_t addr = p - &pages[0] + region->start();
						const edb::address_t align = 1 << (ui.cmbAlignment->currentIndex() + 1);

						if(!ui.chkAlignment->isChecked() || (addr % align) == 0) {
							results->addResult(DialogResults::RegionType::Data, addr);
						}
					}

					// update progress bar every 64KB
					if ((quint64(p) & 0xFFFF) == 0) {
						ui.progressBar->setValue(util::percentage(i, regions.size(), p - &pages[0], region_size));
					}

					++p;
				}
			}
			++i;
		}

		if(results->resultCount() == 0) {
			QMessageBox::information(nullptr, tr("No Results"), tr("No Results were found!"));
			delete results;
		} else {
			results->show();
		}
	}
}


}

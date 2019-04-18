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

#include "DialogASCIIString.h"
#include "DialogResults.h"
#include "edb.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "IRegion.h"
#include "IThread.h"
#include "MemoryRegions.h"
#include "State.h"
#include "Util.h"
#include <QMessageBox>
#include <QVector>
#include <QListWidget>
#include <QPushButton>
#include <QtDebug>

#include <cstring>

namespace BinarySearcherPlugin {

//------------------------------------------------------------------------------
// Name: DialogASCIIString
// Desc: constructor
//------------------------------------------------------------------------------
DialogASCIIString::DialogASCIIString(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f) {
	ui.setupUi(this);
	ui.progressBar->setValue(0);

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
// Desc: This will find *stack aligned pointers* to exact string matches
//------------------------------------------------------------------------------
void DialogASCIIString::do_find() {

	const QByteArray b = ui.txtASCII->text().toLatin1();
	auto results = new DialogResults(this);

	const auto sz = static_cast<size_t>(b.size());
	if(sz != 0) {

		edb::v1::memory_regions().sync();

		if(IProcess *process = edb::v1::debugger_core->process()) {
			if(std::shared_ptr<IThread> thread = process->current_thread()) {

				State state;
				thread->get_state(&state);
				edb::address_t stack_ptr = state.stack_pointer();

				if(std::shared_ptr<IRegion> region = edb::v1::memory_regions().find_region(stack_ptr)) {

					edb::address_t count = (region->end() - stack_ptr) / edb::v1::pointer_size();
					stack_ptr = region->start();

					try {
						std::vector<quint8> chars(sz);

						int i = 0;
						while(stack_ptr < region->end()) {

							// get the value from the stack
							edb::address_t stack_address;

							if(process->read_bytes(stack_ptr, &stack_address, edb::v1::pointer_size())) {
								if(process->read_bytes(stack_address, &chars[0], chars.size())) {
									if(std::memcmp(&chars[0], b.constData(), chars.size()) == 0) {
										results->addResult(DialogResults::RegionType::Stack, stack_ptr);
									}
								}
							}
							ui.progressBar->setValue(util::percentage(i++, count));
							stack_ptr += edb::v1::pointer_size();
						}

					} catch(const std::bad_alloc &) {
						QMessageBox::critical(
						            nullptr,
						            tr("Memroy Allocation Error"),
						            tr("Unable to satisfy memory allocation request for search string."));
					}
				}
			}
		}
	}

	if(results->resultCount() == 0) {
		QMessageBox::information(nullptr, tr("No Results"), tr("No Results were found!"));
		delete results;
	} else {
		results->show();
	}

}

/**
 * @brief DialogASCIIString::showEvent
 * @param event
 */
void DialogASCIIString::showEvent(QShowEvent *event) {
	Q_UNUSED(event)
	ui.txtASCII->setFocus();
}

}

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

#include "DialogSymbolViewer.h"
#include "Configuration.h"
#include "IDebugger.h"
#include "ISymbolManager.h"
#include "Symbol.h"
#include "Util.h"
#include "edb.h"

#include <QStringListModel>
#include <QSortFilterProxyModel>
#include <QMenu>
#include <QPushButton>

namespace SymbolViewerPlugin {

//------------------------------------------------------------------------------
// Name: DialogSymbolViewer
// Desc:
//------------------------------------------------------------------------------
DialogSymbolViewer::DialogSymbolViewer(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f)  {
	ui.setupUi(this);

	btnRefresh_ = new QPushButton(QIcon::fromTheme("view-refresh"), tr("Refresh"));
	connect(btnRefresh_, &QPushButton::clicked, this, [this]() {
		btnRefresh_->setEnabled(false);
		do_find();
		btnRefresh_->setEnabled(true);
	});

	ui.buttonBox->addButton(btnRefresh_, QDialogButtonBox::ActionRole);

	ui.listView->setContextMenuPolicy(Qt::CustomContextMenu);

	model_        = new QStringListModel(this);
	filter_model_ = new QSortFilterProxyModel(this);

	filter_model_->setFilterKeyColumn(0);
	filter_model_->setSourceModel(model_);
	ui.listView->setModel(filter_model_);
	ui.listView->setUniformItemSizes(true);

	connect(ui.txtSearch, &QLineEdit::textChanged, filter_model_, &QSortFilterProxyModel::setFilterFixedString);
}

//------------------------------------------------------------------------------
// Name: on_listView_doubleClicked
// Desc: follows the found item in the data view
//------------------------------------------------------------------------------
void DialogSymbolViewer::on_listView_doubleClicked(const QModelIndex &index) {

	const QString s = index.data().toString();

	if(const Result<edb::address_t, QString> addr = edb::v1::string_to_address(s.split(":")[0])) {
		const std::shared_ptr<Symbol> sym = edb::v1::symbol_manager().find(*addr);

		if(sym && sym->is_code()) {
			edb::v1::jump_to_address(*addr);
		} else {
			edb::v1::dump_data(*addr, false);
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_listView_customContextMenuRequested
// Desc:
//------------------------------------------------------------------------------
void DialogSymbolViewer::on_listView_customContextMenuRequested(const QPoint &pos) {

	const QModelIndex index = ui.listView->indexAt(pos);
	if(index.isValid()) {

		const QString s = index.data().toString();

		if(const Result<edb::address_t, QString> addr = edb::v1::string_to_address(s.split(":")[0])) {

			QMenu menu;
			QAction *const action1 = menu.addAction(tr("&Follow In Disassembly"),    this, SLOT(mnuFollowInCPU()));
			QAction *const action2 = menu.addAction(tr("&Follow In Dump"),           this, SLOT(mnuFollowInDump()));
			QAction *const action3 = menu.addAction(tr("&Follow In Dump (New Tab)"), this, SLOT(mnuFollowInDumpNewTab()));
			QAction *const action4 = menu.addAction(tr("&Follow In Stack"),          this, SLOT(mnuFollowInStack()));

			action1->setData(addr->toQVariant());
			action2->setData(addr->toQVariant());
			action3->setData(addr->toQVariant());
			action4->setData(addr->toQVariant());

			menu.exec(ui.listView->mapToGlobal(pos));
		}
	}
}

//------------------------------------------------------------------------------
// Name: mnuFollowInDump
// Desc:
//------------------------------------------------------------------------------
void DialogSymbolViewer::mnuFollowInDump() {
	if(auto action = qobject_cast<QAction *>(sender())) {
		const edb::address_t address = action->data().toULongLong();
		edb::v1::dump_data(address, false);
	}
}

//------------------------------------------------------------------------------
// Name: mnuFollowInDumpNewTab
// Desc:
//------------------------------------------------------------------------------
void DialogSymbolViewer::mnuFollowInDumpNewTab() {
	if(auto action = qobject_cast<QAction *>(sender())) {
		const edb::address_t address = action->data().toULongLong();
		edb::v1::dump_data(address, true);
	}
}

//------------------------------------------------------------------------------
// Name: mnuFollowInStack
// Desc:
//------------------------------------------------------------------------------
void DialogSymbolViewer::mnuFollowInStack() {
	if(auto action = qobject_cast<QAction *>(sender())) {
		const edb::address_t address = action->data().toULongLong();
		edb::v1::dump_stack(address, false);
	}
}

//------------------------------------------------------------------------------
// Name: mnuFollowInCPU
// Desc:
//------------------------------------------------------------------------------
void DialogSymbolViewer::mnuFollowInCPU() {
	if(auto action = qobject_cast<QAction *>(sender())) {
		const edb::address_t address = action->data().toULongLong();
		edb::v1::jump_to_address(address);
	}
}

//------------------------------------------------------------------------------
// Name: do_find
// Desc:
//------------------------------------------------------------------------------
void DialogSymbolViewer::do_find() {
	QStringList results;

	const QList<std::shared_ptr<Symbol>> symbols = edb::v1::symbol_manager().symbols();
	for(const std::shared_ptr<Symbol> &sym: symbols) {
		results << QString("%1: %2").arg(edb::v1::format_pointer(sym->address), sym->name);
	}

	model_->setStringList(results);
}

//------------------------------------------------------------------------------
// Name: showEvent
// Desc:
//------------------------------------------------------------------------------
void DialogSymbolViewer::showEvent(QShowEvent *) {
	btnRefresh_->setEnabled(false);
	do_find();
	btnRefresh_->setEnabled(true);
}

}

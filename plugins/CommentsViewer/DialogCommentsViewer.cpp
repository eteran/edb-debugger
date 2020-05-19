/*
Copyright (C) 2020 Victorien Molle <biche@biche.re>

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

#include "DialogCommentsViewer.h"
#include "Configuration.h"
#include "IDebugger.h"
#include "ISessionManager.h"
#include "edb.h"

#include <QMenu>
#include <QSortFilterProxyModel>
#include <QStringListModel>

namespace CommentsViewerPlugin {

/**
 * @brief DialogCommentsViewer::DialogCommentsViewer
 * @param parent
 * @param f
 */
DialogCommentsViewer::DialogCommentsViewer(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);

	ui.listView->setContextMenuPolicy(Qt::CustomContextMenu);

	model_       = new QStringListModel(this);
	filterModel_ = new QSortFilterProxyModel(this);

	filterModel_->setFilterKeyColumn(0);
	filterModel_->setSourceModel(model_);
	ui.listView->setModel(filterModel_);
	ui.listView->setUniformItemSizes(true);

	connect(ui.txtSearch, &QLineEdit::textChanged, filterModel_, &QSortFilterProxyModel::setFilterFixedString);
}

/**
 * @brief DialogCommentsViewer::on_listView_doubleClicked
 *
 * follows the found item in the data view
 *
 * @param index
 */
void DialogCommentsViewer::on_listView_doubleClicked(const QModelIndex &index) {

	const QString s = index.data().toString();

	if (const Result<edb::address_t, QString> addr = edb::v1::string_to_address(s.split(":")[0])) {
		edb::v1::jump_to_address(*addr);
	}
}

/**
 * @brief DialogCommentsViewer::on_listView_customContextMenuRequested
 * @param pos
 */
void DialogCommentsViewer::on_listView_customContextMenuRequested(const QPoint &pos) {

	const QModelIndex index = ui.listView->indexAt(pos);
	if (index.isValid()) {

		const QString s = index.data().toString();

		if (const Result<edb::address_t, QString> addr = edb::v1::string_to_address(s.split(":")[0])) {

			QMenu menu;
			QAction *const action1 = menu.addAction(tr("&Follow In Disassembly"), this, SLOT(mnuFollowInCPU()));
			action1->setData(addr->toQVariant());
			menu.exec(ui.listView->mapToGlobal(pos));
		}
	}
}

/**
 * @brief DialogCommentsViewer::mnuFollowInCPU
 */
void DialogCommentsViewer::mnuFollowInCPU() {
	if (auto action = qobject_cast<QAction *>(sender())) {
		const edb::address_t address = action->data().toULongLong();
		edb::v1::jump_to_address(address);
	}
}

/**
 * @brief DialogCommentsViewer::showEvent
 */
void DialogCommentsViewer::showEvent(QShowEvent*) {
	edb::address_t addr;
	QStringList results;
	QVariantList comments;

	comments = edb::v1::session_manager().comments();
	for (auto it = comments.begin(); it != comments.end(); ++it) {
		QVariantMap data = it->toMap();
		addr = edb::v1::string_to_address(data["address"].toString()).value();
		results << QString("%1: %2").arg(edb::v1::format_pointer(addr), data["comment"].toString());
	}

	model_->setStringList(results);
}

}
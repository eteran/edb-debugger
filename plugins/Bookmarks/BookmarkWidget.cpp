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

#include "BookmarkWidget.h"
#include "edb.h"
#include "Expression.h"
#include "BookmarksModel.h"
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QTableWidgetItem>

namespace BookmarksPlugin {

//------------------------------------------------------------------------------
// Name: BookmarkWidget
// Desc:
//------------------------------------------------------------------------------
BookmarkWidget::BookmarkWidget(QWidget *parent, Qt::WindowFlags f) : QWidget(parent, f)  {
	ui.setupUi(this);

	model_ = new BookmarksModel(this);
	ui.tableView->setModel(model_);

	connect(edb::v1::debugger_ui, SIGNAL(detachEvent()), model_, SLOT(clearBookmarks()));
}


//------------------------------------------------------------------------------
// Name: on_tableView_doubleClicked
// Desc:
//------------------------------------------------------------------------------
void BookmarkWidget::on_tableView_doubleClicked(const QModelIndex &index) {

	if(auto item = static_cast<BookmarksModel::Bookmark *>(index.internalPointer())) {
		switch(index.column()) {
		case 0: //address
			switch(item->type) {
			case BookmarksModel::Bookmark::Code:
				edb::v1::jump_to_address(item->address);
				break;
			case BookmarksModel::Bookmark::Data:
				edb::v1::dump_data(item->address);
				break;
			case BookmarksModel::Bookmark::Stack:
				edb::v1::dump_stack(item->address);
				break;
			}
			break;
		case 1: // type
			{
				QString old_type = BookmarksModel::BookmarkTypeToString(item->type);
				QStringList items;
				items << tr("Code") << tr("Data") << tr("Stack");

				bool ok;
				const QString new_type = QInputDialog::getItem(ui.tableView, tr("Comment"), tr("Set Type:"), items, items.indexOf(old_type), false, &ok);
				if(ok) {
					model_->setType(index, new_type);
				}
			}
			break;
		case 2: //comment
			{
				QString old_comment = item->comment;
				bool ok;
				const QString new_comment = QInputDialog::getText(ui.tableView, tr("Comment"), tr("Set Comment:"), QLineEdit::Normal, old_comment, &ok);
				if(ok) {
					model_->setComment(index, new_comment);
				}
			}
			break;
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_btnAdd_clicked
// Desc:
//------------------------------------------------------------------------------
void BookmarkWidget::on_btnAdd_clicked() {

	if(boost::optional<edb::address_t> address = edb::v2::get_expression_from_user(tr("Bookmark Address"), tr("Address:"))) {
		add_address(*address);
	}
}

//------------------------------------------------------------------------------
// Name: on_btnDel_clicked
// Desc:
//------------------------------------------------------------------------------
void BookmarkWidget::on_btnDel_clicked() {

	const QItemSelectionModel *const selModel = ui.tableView->selectionModel();
	const QModelIndexList selections = selModel->selectedRows();

	if(selections.size() == 1) {
		QModelIndex index = selections[0];
		model_->deleteBookmark(index);
	}
}

//------------------------------------------------------------------------------
// Name: on_btnClear_clicked
// Desc:
//------------------------------------------------------------------------------
void BookmarkWidget::on_btnClear_clicked() {
	model_->clearBookmarks();
}

//------------------------------------------------------------------------------
// Name: add_address
// Desc:
//------------------------------------------------------------------------------
void BookmarkWidget::add_address(edb::address_t address, const QString &type, const QString &comment) {

	QVector<BookmarksModel::Bookmark> &bookmarks = model_->bookmarks();
	auto it = std::find_if(bookmarks.begin(), bookmarks.end(), [address](const BookmarksModel::Bookmark &bookmark) {
		return bookmark.address == address;
	});


	if(it == bookmarks.end()) {
		BookmarksModel::Bookmark bookmark = {
			address, BookmarksModel::BookmarkStringToType(type), comment
		};

		model_->addBookmark(bookmark);
	}
}

//------------------------------------------------------------------------------
// Name: shortcut
// Desc:
//------------------------------------------------------------------------------
void BookmarkWidget::shortcut(int index) {

	QVector<BookmarksModel::Bookmark> &bookmarks = model_->bookmarks();
	if(index < bookmarks.size()) {
		BookmarksModel::Bookmark *item = &bookmarks[index];

		switch(item->type) {
		case BookmarksModel::Bookmark::Code:
			edb::v1::jump_to_address(item->address);
			break;
		case BookmarksModel::Bookmark::Data:
			edb::v1::dump_data(item->address);
			break;
		case BookmarksModel::Bookmark::Stack:
			edb::v1::dump_stack(item->address);
			break;
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_tableView_customContextMenuRequested
// Desc:
//------------------------------------------------------------------------------
void BookmarkWidget::on_tableView_customContextMenuRequested(const QPoint &pos) {

	QMenu menu;
	QAction *const actionAdd     = menu.addAction(tr("&Add Address"));
	QAction *const actionDel     = menu.addAction(tr("&Delete Address"));
	QAction *const actionClear   = menu.addAction(tr("&Clear"));
	menu.addSeparator();
	QAction *const actionComment = menu.addAction(tr("&Set Comment"));
	QAction *const actionType    = menu.addAction(tr("Set &Type"));
	QAction *const chosen = menu.exec(ui.tableView->mapToGlobal(pos));

	if(chosen == actionAdd) {
		on_btnAdd_clicked();
	} else if(chosen == actionDel) {
		on_btnDel_clicked();
	} else if(chosen == actionClear) {
		on_btnClear_clicked();
	} else if(chosen == actionComment) {

		const QItemSelectionModel *const selModel = ui.tableView->selectionModel();
		const QModelIndexList selections = selModel->selectedRows();

		if(selections.size() == 1) {
			QModelIndex index = selections[0];

			if(auto item = static_cast<BookmarksModel::Bookmark *>(index.internalPointer())) {
				QString old_comment = item->comment;
				bool ok;
				const QString new_comment = QInputDialog::getText(ui.tableView, tr("Comment"), tr("Set Comment:"), QLineEdit::Normal, old_comment, &ok);
				if(ok) {
					model_->setComment(index, new_comment);
				}
			}
		}
	} else if(chosen == actionType) {
		const QItemSelectionModel *const selModel = ui.tableView->selectionModel();
		const QModelIndexList selections = selModel->selectedRows();

		if(selections.size() == 1) {
			QModelIndex index = selections[0];

			if(auto item = static_cast<BookmarksModel::Bookmark *>(index.internalPointer())) {

				QString old_type = BookmarksModel::BookmarkTypeToString(item->type);
				QStringList items;
				items << tr("Code") << tr("Data") << tr("Stack");

				bool ok;
				const QString new_type = QInputDialog::getItem(ui.tableView, tr("Comment"), tr("Set Type:"), items, items.indexOf(old_type), false, &ok);
				if(ok) {
					model_->setType(index, new_type);
				}
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: entries
// Desc:
//------------------------------------------------------------------------------
QList<BookmarksModel::Bookmark> BookmarkWidget::entries() const {
	QVector<BookmarksModel::Bookmark> &bookmarks = model_->bookmarks();
	return bookmarks.toList();
}

}

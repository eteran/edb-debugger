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
#include "BookmarksModel.h"
#include "Expression.h"
#include "edb.h"
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QTableWidgetItem>

namespace BookmarksPlugin {

/**
 * @brief BookmarkWidget::BookmarkWidget
 * @param parent
 * @param f
 */
BookmarkWidget::BookmarkWidget(QWidget *parent, Qt::WindowFlags f)
	: QWidget(parent, f) {

	ui.setupUi(this);

	model_ = new BookmarksModel(this);
	ui.tableView->setModel(model_);

	connect(edb::v1::debugger_ui, SIGNAL(detachEvent()), model_, SLOT(clearBookmarks()));
	connect(ui.buttonAdd, &QPushButton::clicked, this, &BookmarkWidget::buttonAddClicked);
	connect(ui.buttonDel, &QPushButton::clicked, this, &BookmarkWidget::buttonDelClicked);
	connect(ui.buttonClear, &QPushButton::clicked, this, &BookmarkWidget::buttonClearClicked);
}

/**
 * @brief BookmarkWidget::on_tableView_doubleClicked
 * @param index
 */
void BookmarkWidget::on_tableView_doubleClicked(const QModelIndex &index) {

	if (auto item = static_cast<BookmarksModel::Bookmark *>(index.internalPointer())) {
		switch (index.column()) {
		case 0: //address
			switch (item->type) {
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
			QString old_type = BookmarksModel::bookmarkTypeToString(item->type);
			QStringList items;
			items << tr("Code") << tr("Data") << tr("Stack");

			bool ok;
			const QString new_type = QInputDialog::getItem(ui.tableView, tr("Comment"), tr("Set Type:"), items, items.indexOf(old_type), false, &ok);
			if (ok) {
				model_->setType(index, new_type);
			}
		} break;
		case 2: //comment
		{
			QString old_comment = item->comment;
			bool ok;
			const QString new_comment = QInputDialog::getText(ui.tableView, tr("Comment"), tr("Set Comment:"), QLineEdit::Normal, old_comment, &ok);
			if (ok) {
				model_->setComment(index, new_comment);
			}
		} break;
		}
	}
}

/**
 * @brief BookmarkWidget::buttonAddClicked
 */
void BookmarkWidget::buttonAddClicked() {

	if (boost::optional<edb::address_t> address = edb::v2::get_expression_from_user(tr("Bookmark Address"), tr("Address:"))) {
		addAddress(*address);
	}
}

/**
 * @brief BookmarkWidget::buttonDelClicked
 */
void BookmarkWidget::buttonDelClicked() {

	const QItemSelectionModel *const selModel = ui.tableView->selectionModel();
	const QModelIndexList selections          = selModel->selectedRows();

	if (selections.size() == 1) {
		QModelIndex index = selections[0];
		model_->deleteBookmark(index);
	}
}

/**
 * @brief BookmarkWidget::buttonClearClicked
 */
void BookmarkWidget::buttonClearClicked() {
	model_->clearBookmarks();
}

/**
 * @brief BookmarkWidget::addAddress
 * @param address
 * @param type
 * @param comment
 */
void BookmarkWidget::addAddress(edb::address_t address, const QString &type, const QString &comment) {

	const QVector<BookmarksModel::Bookmark> &bookmarks = model_->bookmarks();

	auto it = std::find_if(bookmarks.begin(), bookmarks.end(), [address](const BookmarksModel::Bookmark &bookmark) {
		return bookmark.address == address;
	});

	if (it == bookmarks.end()) {
		BookmarksModel::Bookmark bookmark = {
			address,
			BookmarksModel::bookmarkStringToType(type),
			comment,
		};

		model_->addBookmark(bookmark);
	}
}

/**
 * @brief BookmarkWidget::shortcut
 * @param index
 */
void BookmarkWidget::shortcut(int index) {

	const QVector<BookmarksModel::Bookmark> &bookmarks = model_->bookmarks();
	if (index < bookmarks.size()) {
		const BookmarksModel::Bookmark *item = &bookmarks[index];

		switch (item->type) {
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

/**
 * @brief BookmarkWidget::on_tableView_customContextMenuRequested
 * @param pos
 */
void BookmarkWidget::on_tableView_customContextMenuRequested(const QPoint &pos) {

	QMenu menu;
	QAction *const actionAdd   = menu.addAction(tr("&Add Address"));
	QAction *const actionDel   = menu.addAction(tr("&Delete Address"));
	QAction *const actionClear = menu.addAction(tr("&Clear"));
	menu.addSeparator();
	QAction *const actionComment = menu.addAction(tr("&Set Comment"));
	QAction *const actionType    = menu.addAction(tr("Set &Type"));
	QAction *const chosen        = menu.exec(ui.tableView->mapToGlobal(pos));

	if (chosen == actionAdd) {
		buttonAddClicked();
	} else if (chosen == actionDel) {
		buttonDelClicked();
	} else if (chosen == actionClear) {
		buttonClearClicked();
	} else if (chosen == actionComment) {

		const QItemSelectionModel *const selModel = ui.tableView->selectionModel();
		const QModelIndexList selections          = selModel->selectedRows();

		if (selections.size() == 1) {
			QModelIndex index = selections[0];

			if (auto item = static_cast<BookmarksModel::Bookmark *>(index.internalPointer())) {
				QString old_comment = item->comment;
				bool ok;
				const QString new_comment = QInputDialog::getText(ui.tableView, tr("Comment"), tr("Set Comment:"), QLineEdit::Normal, old_comment, &ok);
				if (ok) {
					model_->setComment(index, new_comment);
				}
			}
		}
	} else if (chosen == actionType) {
		const QItemSelectionModel *const selModel = ui.tableView->selectionModel();
		const QModelIndexList selections          = selModel->selectedRows();

		if (selections.size() == 1) {
			QModelIndex index = selections[0];

			if (auto item = static_cast<BookmarksModel::Bookmark *>(index.internalPointer())) {

				QString old_type = BookmarksModel::bookmarkTypeToString(item->type);
				QStringList items;
				items << tr("Code") << tr("Data") << tr("Stack");

				bool ok;
				const QString new_type = QInputDialog::getItem(ui.tableView, tr("Comment"), tr("Set Type:"), items, items.indexOf(old_type), false, &ok);
				if (ok) {
					model_->setType(index, new_type);
				}
			}
		}
	}
}

/**
 * @brief BookmarkWidget::entries
 * @return
 */
QList<BookmarksModel::Bookmark> BookmarkWidget::entries() const {
	const QVector<BookmarksModel::Bookmark> &bookmarks = model_->bookmarks();
	return bookmarks.toList();
}

}

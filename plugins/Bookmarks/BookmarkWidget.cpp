/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "BookmarkWidget.h"
#include "BookmarksModel.h"
#include "Expression.h"
#include "IBreakpoint.h"
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
	connect(edb::v1::disassembly_widget(), SIGNAL(signalUpdated()), model_, SLOT(updateList()));
	connect(ui.buttonAdd, &QPushButton::clicked, this, &BookmarkWidget::buttonAddClicked);
	connect(ui.buttonDel, &QPushButton::clicked, this, &BookmarkWidget::buttonDelClicked);
	connect(ui.buttonClear, &QPushButton::clicked, this, &BookmarkWidget::buttonClearClicked);

	toggleBreakpointAction_      = createAction(tr("&Toggle Breakpoint"), QKeySequence(tr("B")), &BookmarkWidget::toggleBreakpoint);
	conditionalBreakpointAction_ = createAction(tr("Add &Conditional Breakpoint"), QKeySequence(tr("Shift+B")), &BookmarkWidget::addConditionalBreakpoint);
}

/**
 * @brief BookmarkWidget::on_tableView_doubleClicked
 * @param index
 */
void BookmarkWidget::on_tableView_doubleClicked(const QModelIndex &index) {

	if (auto item = static_cast<BookmarksModel::Bookmark *>(index.internalPointer())) {
		switch (index.column()) {
		case 0: // address
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
		case 2: // comment
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

	if (std::optional<edb::address_t> address = edb::v2::get_expression_from_user(tr("Bookmark Address"), tr("Address:"))) {
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
 * @brief BookmarkWidget::toggleBreakpoint
 */
void BookmarkWidget::toggleBreakpoint() {

	const QItemSelectionModel *const selModel = ui.tableView->selectionModel();
	const QModelIndexList selections          = selModel->selectedRows();

	for (const auto index : selections) {
		auto item = static_cast<BookmarksModel::Bookmark *>(index.internalPointer());
		edb::v1::toggle_breakpoint(item->address);
	}
}

/**
 * @brief BookmarkWidget::addConditionalBreakpoint
 */
void BookmarkWidget::addConditionalBreakpoint() {

	const QItemSelectionModel *const selModel = ui.tableView->selectionModel();
	const QModelIndexList selections          = selModel->selectedRows();

	if (selections.size() == 1) {
		bool ok;
		const QString condition = QInputDialog::getText(this, tr("Set Breakpoint Condition"), tr("Expression:"), QLineEdit::Normal, QString(), &ok);
		auto item               = static_cast<BookmarksModel::Bookmark *>(selections[0].internalPointer());

		if (ok) {
			if (std::shared_ptr<IBreakpoint> bp = edb::v1::create_breakpoint(item->address)) {
				if (!condition.isEmpty()) {
					bp->condition = condition;
				}
			}
		}
	}
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
	menu.addAction(toggleBreakpointAction_);
	menu.addAction(conditionalBreakpointAction_);
	QAction *const chosen = menu.exec(ui.tableView->mapToGlobal(pos));

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
	} else if (chosen == toggleBreakpointAction_) {
		toggleBreakpoint();
	} else if (chosen == conditionalBreakpointAction_) {
		addConditionalBreakpoint();
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

// This is copied from Debugger::createAction, so really there should either be a class that implements
// this that both inherit from, or this should be a generic function that takes the QObject* to act upon
// as a parameter.
template <class F>
QAction *BookmarkWidget::createAction(const QString &text, const QKeySequence &keySequence, F func) {
	auto action = new QAction(text, this);
	action->setShortcut(keySequence);
	addAction(action);
	connect(action, &QAction::triggered, this, func);
	return action;
}

}

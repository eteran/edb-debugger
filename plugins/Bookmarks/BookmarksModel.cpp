/*
 * Copyright (C) 2017 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "BookmarksModel.h"
#include "edb.h"

namespace BookmarksPlugin {

/**
 * @brief Constructs the bookmarks model and loads the breakpoint and current-instruction indicator icons.
 *
 * @param parent
 */
BookmarksModel::BookmarksModel(QObject *parent)
	: QAbstractItemModel(parent),
	  breakpointIcon_(QLatin1String(":/debugger/images/breakpoint.svg")),
	  currentIcon_(QLatin1String(":/debugger/images/arrow-right.svg")),
	  currentBpIcon_(QLatin1String(":/debugger/images/arrow-right-red.svg")) {
}

/**
 * @brief Returns the column header labels: Address, Type, and Comment.
 *
 * @param section
 * @param orientation
 * @param role
 * @return
 */
QVariant BookmarksModel::headerData(int section, Qt::Orientation orientation, int role) const {

	if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
		switch (section) {
		case 0:
			return tr("Address");
		case 1:
			return tr("Type");
		case 2:
			return tr("Comment");
		}
	}

	return QVariant();
}

/**
 * @brief Returns display text and decoration icons for each bookmark cell based on the role.
 *
 * @param index
 * @param role
 * @return
 */
QVariant BookmarksModel::data(const QModelIndex &index, int role) const {

	if (!index.isValid()) {
		return QVariant();
	}

	const Bookmark &bookmark = bookmarks_[index.row()];

	if (role == Qt::DisplayRole) {
		switch (index.column()) {
		case 0: {
			// Return the address with symbol name, if there is one
			const QString symname = edb::v1::find_function_symbol(bookmark.address);
			const QString address = edb::v1::format_pointer(bookmark.address);

			if (!symname.isEmpty()) {
				return tr("%1 <%2>").arg(address, symname);
			}

			return address;
		}
		case 1:
			switch (bookmark.type) {
			case Bookmark::Code:
				return tr("Code");
			case Bookmark::Data:
				return tr("Data");
			case Bookmark::Stack:
				return tr("Stack");
			}
			break;
		case 2:
			return bookmark.comment;
		default:
			return QVariant();
		}
	}

	if (role == Qt::DecorationRole) {
		switch (index.column()) {
		case 0: {
			// Puts the correct icon (BP, current BP, current instruction) next to the address

			// TODO: This is mostly copied from QDisassemblyView::drawSidebarElements, and both
			// should really be factored out into a common location (although this uses icons
			// and the other uses SVG renderers and painting).
			const bool is_eip         = (bookmark.address == edb::v1::instruction_pointer_address());
			const bool has_breakpoint = (edb::v1::find_breakpoint(bookmark.address) != nullptr);

			const QIcon *icon = nullptr;
			if (is_eip) {
				icon = has_breakpoint ? &currentBpIcon_ : &currentIcon_;
			} else if (has_breakpoint) {
				icon = &breakpointIcon_;
			}

			if (icon) {
				return *icon;
			}

			// This acts as a dummy icon so even addresses with no icon will be aligned properly
			return QColor(0, 0, 0, 0);
		}
		default:
			return QVariant();
		}
	}

	return QVariant();
}

/**
 * @brief Appends a new bookmark entry to the list and notifies the view.
 *
 * @param r
 */
void BookmarksModel::addBookmark(const Bookmark &r) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	bookmarks_.push_back(r);
	endInsertRows();
}

/**
 * @brief Removes all bookmarks from the model and notifies the view.
 */
void BookmarksModel::clearBookmarks() {
	beginResetModel();
	bookmarks_.clear();
	endResetModel();
}

/**
 * @brief Returns the model index for the given row and column, embedding a pointer to the bookmark entry.
 *
 * @param row
 * @param column
 * @param parent
 * @return
 */
QModelIndex BookmarksModel::index(int row, int column, const QModelIndex &parent) const {

	Q_UNUSED(parent)

	if (row >= bookmarks_.size()) {
		return QModelIndex();
	}

	if (column >= 3) {
		return QModelIndex();
	}

	if (row >= 0) {
		return createIndex(row, column, const_cast<Bookmark *>(&bookmarks_[row]));
	}

	return createIndex(row, column);
}

/**
 * @brief Returns an invalid index since bookmarks form a flat list with no hierarchical parent.
 *
 * @param index
 * @return
 */
QModelIndex BookmarksModel::parent(const QModelIndex &index) const {
	Q_UNUSED(index)
	return QModelIndex();
}

/**
 * @brief Returns the number of bookmarks currently stored in the model.
 *
 * @param parent
 * @return
 */
int BookmarksModel::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return bookmarks_.size();
}

/**
 * @brief Returns 3, representing the fixed Address, Type, and Comment columns.
 *
 * @param parent
 * @return
 */
int BookmarksModel::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return 3;
}

/**
 * @brief Updates the comment string for the bookmark at the given index and notifies the view.
 *
 * @param index
 * @param comment
 */
void BookmarksModel::setComment(const QModelIndex &index, const QString &comment) {

	if (!index.isValid()) {
		return;
	}

	Bookmark &bookmark = bookmarks_[index.row()];

	bookmark.comment = comment;
	Q_EMIT dataChanged(index, index);
}

/**
 * @brief Updates the bookmark type for the entry at the given index and notifies the view.
 *
 * @param index
 * @param type
 */
void BookmarksModel::setType(const QModelIndex &index, const QString &type) {

	if (!index.isValid()) {
		return;
	}

	Bookmark &bookmark = bookmarks_[index.row()];

	bookmark.type = bookmarkStringToType(type);

	Q_EMIT dataChanged(index, index);
}

/**
 * @brief Invalidates all displayed bookmark data and triggers a full view refresh.
 */
void BookmarksModel::updateList() {
	// Every time the disassembly view changes, all the bookmark data is invalidated.
	// This is not super expensive (unless you have a million bookmarks) but is not
	// optimal either. Ideally we could factor out eip updates with the signalUpdated
	// signal, and breakpoint updates with the toggleBreakPoint signal, but the latter
	// can't use the SIGNAL/SLOT macros which means Debugger.h would need to be moved
	// into the include directory.
	beginResetModel();
	endResetModel();
}

/**
 * @brief Removes the bookmark at the given index from the model and notifies the view.
 *
 * @param index
 */
void BookmarksModel::deleteBookmark(const QModelIndex &index) {

	if (!index.isValid()) {
		return;
	}

	int row = index.row();

	beginRemoveRows(QModelIndex(), row, row);
	bookmarks_.remove(row);
	endRemoveRows();
}

}

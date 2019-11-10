/*
Copyright (C) 2017 - 2017 Evan Teran
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

#include "BookmarksModel.h"
#include "edb.h"

namespace BookmarksPlugin {

/**
 * @brief BookmarksModel::BookmarksModel
 * @param parent
 */
BookmarksModel::BookmarksModel(QObject *parent)
	: QAbstractItemModel(parent) {
}

/**
 * @brief BookmarksModel::headerData
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
 * @brief BookmarksModel::data
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
		case 0:
			return edb::v1::format_pointer(bookmark.address);
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

	return QVariant();
}

/**
 * @brief BookmarksModel::addBookmark
 * @param r
 */
void BookmarksModel::addBookmark(const Bookmark &r) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	bookmarks_.push_back(r);
	endInsertRows();
}

/**
 * @brief BookmarksModel::clearBookmarks
 */
void BookmarksModel::clearBookmarks() {
	beginResetModel();
	bookmarks_.clear();
	endResetModel();
}

/**
 * @brief BookmarksModel::index
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
	} else {
		return createIndex(row, column);
	}
}

/**
 * @brief BookmarksModel::parent
 * @param index
 * @return
 */
QModelIndex BookmarksModel::parent(const QModelIndex &index) const {
	Q_UNUSED(index)
	return QModelIndex();
}

/**
 * @brief BookmarksModel::rowCount
 * @param parent
 * @return
 */
int BookmarksModel::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return bookmarks_.size();
}

/**
 * @brief BookmarksModel::columnCount
 * @param parent
 * @return
 */
int BookmarksModel::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
	return 3;
}

/**
 * @brief BookmarksModel::setComment
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
 * @brief BookmarksModel::setType
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
 * @brief BookmarksModel::deleteBookmark
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

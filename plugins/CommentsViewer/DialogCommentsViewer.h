/*
Copyright (C) 2020 Victorien molle <biche@biche.re>

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

#ifndef DIALOG_COMMENTS_VIEWER_H_20200519
#define DIALOG_COMMENTS_VIEWER_H_20200519

#include "Types.h"
#include "ui_DialogCommentsViewer.h"
#include <QDialog>

class QModelIndex;
class QPoint;
class QSortFilterProxyModel;
class QStringListModel;

namespace CommentsViewerPlugin {

class DialogCommentsViewer : public QDialog {
	Q_OBJECT

public:
	explicit DialogCommentsViewer(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogCommentsViewer() override = default;

public Q_SLOTS:
	void on_listView_doubleClicked(const QModelIndex &index);
	void on_listView_customContextMenuRequested(const QPoint &pos);

private Q_SLOTS:
	void mnuFollowInCPU();

private:
	void showEvent(QShowEvent *event) override;

private:
	Ui::DialogCommentsViewer ui;
	QStringListModel *model_            = nullptr;
	QSortFilterProxyModel *filterModel_ = nullptr;
};

}

#endif

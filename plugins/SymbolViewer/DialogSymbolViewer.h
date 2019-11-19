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

#ifndef DIALOG_SYMBOL_VIEWER_H_20080812_
#define DIALOG_SYMBOL_VIEWER_H_20080812_

#include "Types.h"
#include "ui_DialogSymbolViewer.h"
#include <QDialog>

class QModelIndex;
class QPoint;
class QSortFilterProxyModel;
class QStringListModel;

namespace SymbolViewerPlugin {

class DialogSymbolViewer : public QDialog {
	Q_OBJECT

public:
	explicit DialogSymbolViewer(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogSymbolViewer() override = default;

public Q_SLOTS:
	void on_listView_doubleClicked(const QModelIndex &index);
	void on_listView_customContextMenuRequested(const QPoint &pos);

private Q_SLOTS:
	void mnuFollowInDump();
	void mnuFollowInDumpNewTab();
	void mnuFollowInStack();
	void mnuFollowInCPU();

private:
	void showEvent(QShowEvent *event) override;

private:
	void doFind();

private:
	Ui::DialogSymbolViewer ui;
	QStringListModel *model_            = nullptr;
	QSortFilterProxyModel *filterModel_ = nullptr;
	QPushButton *buttonRefresh_         = nullptr;
};

}

#endif

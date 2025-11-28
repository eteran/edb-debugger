/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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

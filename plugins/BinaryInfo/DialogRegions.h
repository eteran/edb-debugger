/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_REGIONS_H_20111128_
#define DIALOG_REGIONS_H_20111128_

#include "Types.h"
#include "ui_DialogRegions.h"
#include <QDialog>

class QStringListModel;
class QSortFilterProxyModel;
class QModelIndex;

namespace BinaryInfoPlugin {

class DialogRegions : public QDialog {
	Q_OBJECT

public:
	explicit DialogRegions(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogRegions() override = default;

private:
	void showEvent(QShowEvent *event) override;

private:
	Ui::DialogRegions ui;
	QSortFilterProxyModel *filterModel_ = nullptr;
	QPushButton *buttonExplore_         = nullptr;
};

}

#endif

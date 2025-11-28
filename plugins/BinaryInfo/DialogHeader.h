/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_HEADER_H_20190403_
#define DIALOG_HEADER_H_20190403_

#include "IRegion.h"
#include "Types.h"
#include "ui_DialogHeader.h"
#include <QDialog>
#include <memory>

class QStringListModel;
class QSortFilterProxyModel;
class QModelIndex;

namespace BinaryInfoPlugin {

class DialogHeader : public QDialog {
	Q_OBJECT

public:
	explicit DialogHeader(const std::shared_ptr<IRegion> &region, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogHeader() override = default;

private:
	Ui::DialogHeader ui;
};

}

#endif

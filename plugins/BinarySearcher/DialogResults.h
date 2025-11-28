/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_RESULTS_H_20190403_
#define DIALOG_RESULTS_H_20190403_

#include "edb.h"
#include "ui_DialogResults.h"
#include <QDialog>

class QListWidgetItem;

namespace BinarySearcherPlugin {

class DialogResults : public QDialog {
	Q_OBJECT

public:
	enum class RegionType {
		Code,
		Stack,
		Data
	};

public:
	explicit DialogResults(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogResults() override = default;

public:
	void addResult(RegionType region, edb::address_t address);
	[[nodiscard]] int resultCount() const;

public Q_SLOTS:
	void on_listWidget_itemDoubleClicked(QListWidgetItem *);

private:
	Ui::DialogResults ui;
};

}

#endif

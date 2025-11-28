/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_XREFS_H_20191119_
#define DIALOG_XREFS_H_20191119_

#include "edb.h"
#include "ui_DialogXRefs.h"

#include <QDialog>
#include <set>
#include <utility>

class QListWidgetItem;

namespace AnalyzerPlugin {

class DialogXRefs : public QDialog {
	Q_OBJECT

public:
	explicit DialogXRefs(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogXRefs() override = default;

public Q_SLOTS:
	void on_listReferences_itemDoubleClicked(QListWidgetItem *item);

public:
	void addReference(const std::pair<edb::address_t, edb::address_t> &reference);

private:
	Ui::DialogXRefs ui;
	std::set<std::pair<edb::address_t, edb::address_t>> references_;
};

}

#endif

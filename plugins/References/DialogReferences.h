/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_REFERENCES_H_20061101_
#define DIALOG_REFERENCES_H_20061101_

#include "IRegion.h"
#include "Types.h"
#include "ui_DialogReferences.h"
#include <QDialog>

class QListWidgetItem;

namespace ReferencesPlugin {

class DialogReferences : public QDialog {
	Q_OBJECT

public:
	explicit DialogReferences(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogReferences() override = default;

public Q_SLOTS:
	void on_listWidget_itemDoubleClicked(QListWidgetItem *item);

Q_SIGNALS:
	void updateProgress(int);

private:
	void showEvent(QShowEvent *event) override;

private:
	void doFind();

private:
	Ui::DialogReferences ui;
	QPushButton *buttonFind_ = nullptr;
};

}

#endif

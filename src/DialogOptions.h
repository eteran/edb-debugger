/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_OPTIONS_H_20061101_
#define DIALOG_OPTIONS_H_20061101_

#include <QDialog>

#include "ui_DialogOptions.h"

class QToolBox;

class DialogOptions final : public QDialog {
	Q_OBJECT
public:
	explicit DialogOptions(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogOptions() override = default;

public Q_SLOTS:
	void on_btnSymbolDir_clicked();
	void on_btnPluginDir_clicked();
	void on_btnTTY_clicked();
	void on_btnSessionDir_clicked();

protected:
	void closeEvent(QCloseEvent *event) override;
	void accept() override;

public:
	void showEvent(QShowEvent *event) override;
	void addOptionsPage(QWidget *page);

private:
	QString fontFromDialog(const QString &default_font);
	QString directoryFromDialog();

private:
	Ui::DialogOptions ui;
	QToolBox *toolbox_ = nullptr;
	QString currentThemeName_;
};

#endif

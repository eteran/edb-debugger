/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_PLUGINS_H_20080926_
#define DIALOG_PLUGINS_H_20080926_

#include <QDialog>

#include "ui_DialogPlugins.h"

class QSortFilterProxyModel;
class PluginModel;

class DialogPlugins : public QDialog {
	Q_OBJECT

public:
	explicit DialogPlugins(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	DialogPlugins(const DialogPlugins &)            = delete;
	DialogPlugins &operator=(const DialogPlugins &) = delete;
	~DialogPlugins() override                       = default;

public:
	void showEvent(QShowEvent *) override;

private:
	Ui::DialogPlugins ui;
	PluginModel *pluginModel_            = nullptr;
	QSortFilterProxyModel *pluginFilter_ = nullptr;
};

#endif

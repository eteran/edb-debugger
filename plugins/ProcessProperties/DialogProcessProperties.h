/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_PROCESS_PROPERTIES_H_20120817_
#define DIALOG_PROCESS_PROPERTIES_H_20120817_

#include "ThreadsModel.h"
#include "ui_DialogProcessProperties.h"
#include <QDialog>
#include <QSortFilterProxyModel>

namespace ProcessPropertiesPlugin {

class DialogProcessProperties : public QDialog {
	Q_OBJECT

public:
	explicit DialogProcessProperties(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogProcessProperties() override = default;

public Q_SLOTS:
	void on_btnParent_clicked();
	void on_btnImage_clicked();
	void on_btnRefreshEnvironment_clicked();
	void on_btnRefreshHandles_clicked();
	void on_btnStrings_clicked();
	void on_btnRefreshThreads_clicked();
	void on_btnRefreshMemory_clicked();
	void on_txtSearchEnvironment_textChanged(const QString &text);

private:
	void updateGeneralPage();
	void updateMemoryPage();
	void updateModulePage();
	void updateHandles();
	void updateThreads();
	void updateEnvironmentPage(const QString &filter);

private:
	void showEvent(QShowEvent *event) override;

private:
	Ui::DialogProcessProperties ui;
	ThreadsModel *threadsModel_           = nullptr;
	QSortFilterProxyModel *threadsFilter_ = nullptr;
};

}

#endif

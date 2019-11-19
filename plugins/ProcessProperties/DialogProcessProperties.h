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

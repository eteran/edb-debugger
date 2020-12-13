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

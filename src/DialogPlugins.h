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
	DialogPlugins(const DialogPlugins &) = delete;
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

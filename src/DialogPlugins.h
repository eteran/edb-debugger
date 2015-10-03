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

#ifndef DIALOG_PLUGINS_20080926_H_
#define DIALOG_PLUGINS_20080926_H_

#include <QDialog>

class QSortFilterProxyModel;
class PluginModel;

namespace Ui { class DialogPlugins; }

class DialogPlugins : public QDialog {
	Q_OBJECT
	Q_DISABLE_COPY(DialogPlugins);
public:
	DialogPlugins(QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogPlugins();

public:
	void showEvent(QShowEvent *);

private:
	Ui::DialogPlugins *const ui;
	PluginModel           *plugin_model_;
	QSortFilterProxyModel *plugin_filter_;
};

#endif


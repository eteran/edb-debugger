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

#include "DialogPlugins.h"
#include "IPlugin.h"
#include "PluginModel.h"
#include "edb.h"

#include <QMetaClassInfo>
#include <QSortFilterProxyModel>

#include "ui_DialogPlugins.h"

//------------------------------------------------------------------------------
// Name: DialogPlugins
// Desc:
//------------------------------------------------------------------------------
DialogPlugins::DialogPlugins(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f), ui(new Ui::DialogPlugins) {
	ui->setupUi(this);
	
	plugin_model_  = new PluginModel(this);
	plugin_filter_ = new QSortFilterProxyModel(this);
	
	plugin_filter_->setSourceModel(plugin_model_);
	plugin_filter_->setFilterCaseSensitivity(Qt::CaseInsensitive);
	
	ui->plugins_table->setModel(plugin_filter_);
}

//------------------------------------------------------------------------------
// Name: ~DialogPlugins
// Desc:
//------------------------------------------------------------------------------
DialogPlugins::~DialogPlugins() {
	delete ui;
}

//------------------------------------------------------------------------------
// Name: showEvent
// Desc:
//------------------------------------------------------------------------------
void DialogPlugins::showEvent(QShowEvent *) {

	QMap<QString, QObject *> plugins = edb::v1::plugin_list();

	plugin_model_->clear();

	for(auto it = plugins.begin(); it != plugins.end(); ++it) {

		const QString filename = it.key();
		QString plugin_name;
		QString author;
		QString url;

		// get a QObject from the plugin
		if(QObject *const p = it.value()) {
			const QMetaObject *const meta = p->metaObject();
			plugin_name = meta->className();
			const int author_index = meta->indexOfClassInfo("author");
			if(author_index != -1) {
				author = meta->classInfo(author_index).value();
			}

			const int url_index = meta->indexOfClassInfo("url");
			if(url_index != -1) {
				url = meta->classInfo(url_index).value();
			}
		}
		
		plugin_model_->addPlugin(filename, plugin_name, author, url);
	}

	ui->plugins_table->resizeColumnsToContents();
}

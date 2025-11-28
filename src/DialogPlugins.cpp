/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogPlugins.h"
#include "IPlugin.h"
#include "PluginModel.h"
#include "edb.h"

#include <QMetaClassInfo>
#include <QSortFilterProxyModel>

//------------------------------------------------------------------------------
// Name: DialogPlugins
// Desc:
//------------------------------------------------------------------------------
DialogPlugins::DialogPlugins(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);

	pluginModel_  = new PluginModel(this);
	pluginFilter_ = new QSortFilterProxyModel(this);

	pluginFilter_->setSourceModel(pluginModel_);
	pluginFilter_->setFilterCaseSensitivity(Qt::CaseInsensitive);

	ui.plugins_table->setModel(pluginFilter_);
}

//------------------------------------------------------------------------------
// Name: showEvent
// Desc:
//------------------------------------------------------------------------------
void DialogPlugins::showEvent(QShowEvent *) {

	QMap<QString, QObject *> plugins = edb::v1::plugin_list();

	pluginModel_->clear();

	for (auto it = plugins.begin(); it != plugins.end(); ++it) {

		const QString &filename = it.key();
		QString plugin_name;
		QString author;
		QString url;

		// get a QObject from the plugin
		if (QObject *const p = it.value()) {
			const QMetaObject *const meta = p->metaObject();
			plugin_name                   = meta->className();
			const int author_index        = meta->indexOfClassInfo("author");
			if (author_index != -1) {
				author = meta->classInfo(author_index).value();
			}

			const int url_index = meta->indexOfClassInfo("url");
			if (url_index != -1) {
				url = meta->classInfo(url_index).value();
			}
		}

		pluginModel_->addPlugin(filename, plugin_name, author, url);
	}

	ui.plugins_table->resizeColumnsToContents();
}

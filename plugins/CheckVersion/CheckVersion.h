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

#ifndef CHECKVERSION_20061122_H_
#define CHECKVERSION_20061122_H_

#include "IPlugin.h"

class QMenu;
class QNetworkReply;
class QNetworkAccessManager;
class QUrl;

namespace CheckVersion {

class CheckVersion : public QObject, public IPlugin {
	Q_OBJECT
	Q_INTERFACES(IPlugin)
#if QT_VERSION >= 0x050000
	Q_PLUGIN_METADATA(IID "edb.IPlugin/1.0")
#endif
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")

public:
	CheckVersion();
	virtual ~CheckVersion();

public:
	virtual QMenu *menu(QWidget *parent = 0);
	virtual QWidget *options_page();

public Q_SLOTS:
	void show_menu();
	void requestFinished(QNetworkReply *reply);

protected:
	virtual void private_init();

private:
	void do_check();
	void set_proxy(const QUrl &url);

private:
	QMenu                 *menu_;
	QNetworkAccessManager *network_;
	bool                   initial_check_;
};

}

#endif

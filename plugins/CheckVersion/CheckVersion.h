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

#ifndef CHECK_VERSION_H_20061122_
#define CHECK_VERSION_H_20061122_

#include "IPlugin.h"

class QMenu;
class QNetworkReply;
class QNetworkAccessManager;
class QUrl;

namespace CheckVersionPlugin {

class CheckVersion : public QObject, public IPlugin {
	Q_OBJECT
	Q_INTERFACES(IPlugin)
	Q_PLUGIN_METADATA(IID "edb.IPlugin/1.0")
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")

public:
	explicit CheckVersion(QObject *parent = nullptr);
	~CheckVersion() override = default;

public:
	QMenu *menu(QWidget *parent = nullptr) override;
	QWidget *optionsPage() override;

public Q_SLOTS:
	void showMenu();

private:
	void requestFinished(QNetworkReply *reply);

protected:
	void privateInit() override;

private:
	void doCheck();
	void setProxy(const QUrl &url);

private:
	QMenu *menu_                    = nullptr;
	QNetworkAccessManager *network_ = nullptr;
	bool initialCheck_              = true;
};

}

#endif

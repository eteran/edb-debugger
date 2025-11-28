/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
	[[nodiscard]] QMenu *menu(QWidget *parent = nullptr) override;
	[[nodiscard]] QWidget *optionsPage() override;

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

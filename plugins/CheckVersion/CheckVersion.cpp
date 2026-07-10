/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "CheckVersion.h"
#include "OptionsPage.h"
#include "edb.h"
#include "version.h"

#include <QDebug>
#include <QJsonDocument>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>

namespace CheckVersionPlugin {

/**
 * @brief Constructs the CheckVersion plugin object.
 *
 * @param parent
 */
CheckVersion::CheckVersion(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief Performs an automatic version check on startup if enabled in settings.
 */
void CheckVersion::privateInit() {
	QSettings settings;
	if (settings.value(QStringLiteral("CheckVersion/check_on_start.enabled"), true).toBool()) {
		doCheck();
	}
}

/**
 * @brief Returns the plugin's configuration options widget.
 *
 * @return
 */
QWidget *CheckVersion::optionsPage() {
	return new OptionsPage;
}

/**
 * @brief Creates and returns the CheckVersion plugin menu, building it on first call.
 *
 * @param parent
 * @return
 */
QMenu *CheckVersion::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if (!menu_) {
		menu_ = new QMenu(tr("CheckVersion"), parent);
		menu_->addAction(tr("&Check For Latest Version"), this, &CheckVersion::showMenu);
	}

	return menu_;
}

/**
 * @brief Initiates an HTTP request to fetch the latest version information from the project server.
 */
void CheckVersion::doCheck() {

	if (!network_) {
		network_ = new QNetworkAccessManager(this);
		connect(network_, &QNetworkAccessManager::finished, this, &CheckVersion::requestFinished);
	}

	QUrl update_url(QStringLiteral("https://codef00.com/projects/debugger-latest.json?v=%1").arg(QLatin1String(EDB_VERSION_STRING)));
	QNetworkRequest request(update_url);

	setProxy(update_url);
	network_->get(request);
}

/**
 * @brief Configures the network manager's proxy from the HTTP_PROXY environment variable or system settings.
 *
 * @param url
 */
void CheckVersion::setProxy(const QUrl &url) {

	QNetworkProxy proxy;

#ifdef Q_OS_LINUX
	Q_UNUSED(url)
	auto proxy_str = QString::fromUtf8(qgetenv("HTTP_PROXY"));
	if (proxy_str.isEmpty()) {
		proxy_str = QString::fromUtf8(qgetenv("http_proxy"));
	}

	if (!proxy_str.isEmpty()) {
		const QUrl proxy_url = QUrl::fromUserInput(proxy_str);
		const int port       = proxy_url.port(80);
		const auto qport     = static_cast<quint16>(qBound(0, port, 65535));

		proxy = QNetworkProxy(QNetworkProxy::HttpProxy, proxy_url.host(), qport, proxy_url.userName(), proxy_url.password());
	}

#else
	QList<QNetworkProxy> proxies = QNetworkProxyFactory::systemProxyForQuery(QNetworkProxyQuery(url));
	if (proxies.size() >= 1) {
		proxy = proxies.first();
	}
#endif
	network_->setProxy(proxy);
}

/**
 * @brief Triggers a manual version check when the user selects it from the menu.
 */
void CheckVersion::showMenu() {
	initialCheck_ = false;
	doCheck();
}

/**
 * @brief Handles the HTTP version check response, showing a notification dialog if a newer version is available.
 *
 * @param reply
 */
void CheckVersion::requestFinished(QNetworkReply *reply) {

	if (reply->error() != QNetworkReply::NoError) {
		if (!initialCheck_) {
			QMessageBox::critical(
				nullptr,
				tr("An Error Occurred"),
				reply->errorString());
		}
	} else {

		QByteArray s = reply->readAll();

		QJsonParseError e;
		QJsonDocument d = QJsonDocument::fromJson(s, &e);
		if (d.isNull() || e.error != QJsonParseError::NoError) {
			qDebug("[CheckVersion] Error parsing JSON response: %s", qPrintable(e.errorString()));
			return;
		}

		if (!d.isObject()) {
			qDebug("[CheckVersion] Unexpected data format in JSON response");
			return;
		}

		QJsonObject obj = d.object();

		QString version = obj[QStringLiteral("version")].toString();
		QString url     = obj[QStringLiteral("url")].toString();
		QString md5     = obj[QStringLiteral("md5")].toString();
		QString sha1    = obj[QStringLiteral("sha1")].toString();

		if (version.isEmpty() || url.isEmpty() || md5.isEmpty() || sha1.isEmpty()) {
			qDebug("[CheckVersion] Unexpected data format in JSON response");
			return;
		}

		qDebug("comparing versions: [%d] [%d]", edb::v1::int_version(version), edb::v1::edb_version());

		if (edb::v1::int_version(version) > edb::v1::edb_version()) {
			QMessageBox msg;
			msg.setTextFormat(Qt::RichText);
			msg.setWindowTitle(tr("New Version Available"));
			msg.setText(tr("A newer version of edb is available: <strong>%1</strong><br><br>"
						   "URL: <a href=\"%2\">%2</a><br><br>"
						   "MD5: %3<br>"
						   "SHA1: %4")
							.arg(version, url, md5, sha1));
			msg.setStandardButtons(QMessageBox::Ok);
			msg.exec();

		} else {
			if (!initialCheck_) {
				QMessageBox::information(
					nullptr,
					tr("You are up to date"),
					tr("You are running the latest version of edb"));
			}
		}
	}

	reply->deleteLater();
	initialCheck_ = false;
}

}

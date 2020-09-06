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

#include "CheckVersion.h"
#include "OptionsPage.h"
#include "edb.h"
#include "version.h"

#include <QDebug>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>
#include <QJsonDocument>

namespace CheckVersionPlugin {

/**
 * @brief CheckVersion::CheckVersion
 * @param parent
 */
CheckVersion::CheckVersion(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief CheckVersion::privateInit
 */
void CheckVersion::privateInit() {
	QSettings settings;
	if (settings.value("CheckVersion/check_on_start.enabled", true).toBool()) {
		doCheck();
	}
}

/**
 * @brief CheckVersion::optionsPage
 * @return
 */
QWidget *CheckVersion::optionsPage() {
	return new OptionsPage;
}

/**
 * @brief CheckVersion::menu
 * @param parent
 * @return
 */
QMenu *CheckVersion::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if (!menu_) {
		menu_ = new QMenu(tr("CheckVersion"), parent);
		menu_->addAction(tr("&Check For Latest Version"), this, SLOT(showMenu()));
	}

	return menu_;
}

/**
 * @brief CheckVersion::doCheck
 */
void CheckVersion::doCheck() {

	if (!network_) {
		network_ = new QNetworkAccessManager(this);
		connect(network_, &QNetworkAccessManager::finished, this, &CheckVersion::requestFinished);
	}


	QUrl update_url(QString("https://codef00.com/projects/debugger-latest.json?v=%1").arg(edb::version));
	QNetworkRequest request(update_url);

	setProxy(update_url);
	network_->get(request);
}

/**
 * @brief CheckVersion::setProxy
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

		proxy = QNetworkProxy(QNetworkProxy::HttpProxy, proxy_url.host(), proxy_url.port(80), proxy_url.userName(), proxy_url.password());
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
 * @brief CheckVersion::showMenu
 */
void CheckVersion::showMenu() {
	initialCheck_ = false;
	doCheck();
}

/**
 * @brief CheckVersion::requestFinished
 * @param reply
 */
void CheckVersion::requestFinished(QNetworkReply *reply) {

	if (reply->error() != QNetworkReply::NoError) {
		if (!initialCheck_) {
			QMessageBox::critical(
				nullptr,
				tr("An Error Occured"),
				reply->errorString());
		}
	} else {

		QByteArray s = reply->readAll();

		QJsonParseError e;
		QJsonDocument d = QJsonDocument::fromJson(s, &e);
		if(d.isNull() || e.error != QJsonParseError::NoError) {
			qDebug("[CheckVersion] Error parsing JSON response: %s", qPrintable(e.errorString()));
			return;
		}


		if(!d.isObject()) {
			qDebug("[CheckVersion] Unexpected data format in JSON response");
			return;
		}

		QJsonObject obj = d.object();

		QString version = obj["version"].toString();
		QString url = obj["url"].toString();
		QString md5 = obj["md5"].toString();
		QString sha1 = obj["sha1"].toString();

		if(version.isEmpty() || url.isEmpty() || md5.isEmpty() || sha1.isEmpty()) {
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
						   "SHA1: %4").arg(version, url, md5, sha1));
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

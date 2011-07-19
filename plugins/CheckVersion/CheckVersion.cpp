/*
Copyright (C) 2006 - 2011 Evan Teran
                          eteran@alum.rit.edu

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
#include "Debugger.h"
#include "CheckVersionOptionsPage.h"
#include <QDebug>
#include <QHttp>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QUrl>

//------------------------------------------------------------------------------
// Name: CheckVersion()
// Desc:
//------------------------------------------------------------------------------
CheckVersion::CheckVersion() : menu_(0), http_(0), initial_check_(true) {

	QSettings settings;
	if(settings.value("CheckVersion/check_on_start.enabled", true).toBool()) {
		do_check();
	}
}

//------------------------------------------------------------------------------
// Name: ~CheckVersion()
// Desc:
//------------------------------------------------------------------------------
CheckVersion::~CheckVersion() {
}

//------------------------------------------------------------------------------
// Name: options_page()
// Desc:
//------------------------------------------------------------------------------
QWidget *CheckVersion::options_page() {
	return new CheckVersionOptionsPage;
}

//------------------------------------------------------------------------------
// Name: menu(QWidget *parent)
// Desc:
//------------------------------------------------------------------------------
QMenu *CheckVersion::menu(QWidget *parent) {

	if(menu_ == 0) {
		menu_ = new QMenu(tr("CheckVersion"), parent);
		menu_->addAction(tr("&Check For Latest Version"), this, SLOT(show_menu()));
	}

	return menu_;
}

//------------------------------------------------------------------------------
// Name: do_check()
// Desc:
//------------------------------------------------------------------------------
void CheckVersion::do_check() {

	if(http_ == 0) {
		http_ = new QHttp(this);
		connect(http_, SIGNAL(done(bool)), this, SLOT(requestDone(bool)));
	}

	http_->abort();

#ifndef Q_OS_WIN32
	QByteArray proxy = qgetenv("HTTP_PROXY");
	if(proxy.isEmpty()) {
		proxy = qgetenv("http_proxy");
	}

	if(!proxy.isEmpty()) {
		const QUrl url(proxy);
		http_->setProxy(url.host(), url.port(80), url.userName(), url.password());
	}
#endif

    http_->setHost("codef00.com");
    http_->get("/projects/debugger-latest");
}

//------------------------------------------------------------------------------
// Name: show_menu()
// Desc:
//------------------------------------------------------------------------------
void CheckVersion::show_menu() {
	initial_check_ = false;
	do_check();
}

//------------------------------------------------------------------------------
// Name: requestDone()
// Desc:
//------------------------------------------------------------------------------
void CheckVersion::requestDone(bool error) {
	if(error) {
		if(!initial_check_) {
			QMessageBox::information(
				0,
				tr("An Error Occured"),
				http_->errorString());
		}
	} else {
		const QByteArray result = http_->readAll();
		const QString s = result;

		qDebug("comparing versions: [%d] [%d]", edb::v1::int_version(s), edb::v1::edb_version());

		if(edb::v1::int_version(s) > edb::v1::edb_version()) {
			QMessageBox::information(
				0,
				tr("New Version Available"),
				tr("There is a newer version of edb available: <strong>%1</strong>").arg(s));
		} else {
			if(!initial_check_) {
				QMessageBox::information(
					0,
					tr("You are up to date"),
					tr("You are running the latest version of edb"));
			}
		}
	}
	initial_check_ = false;
}

Q_EXPORT_PLUGIN2(CheckVersion, CheckVersion)

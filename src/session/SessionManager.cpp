/*
Copyright (C) 2006 - 2017 Evan Teran
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

#include "SessionManager.h"
#include "IPlugin.h"
#include "edb.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

constexpr int SessionFileVersion  = 1;
const auto SessionFileIdString = QLatin1String("edb-session");

SessionManager& SessionManager::instance() {
	static SessionManager inst;
	return inst;
}

//------------------------------------------------------------------------------
// Name: load_session
// Desc:
//------------------------------------------------------------------------------
Result<void, SessionError> SessionManager::load_session(const QString &session_file) {

	QFile file(session_file);
	if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		// the checks for open() and exists() are racy, but
		// this is fine as it affects only whether we show an
		// error message or not
		if (!file.exists()) {
			// file doesn't exists means this is the first time we
			// opened the debuggee and there is no session file for
			// it yet

			// return success as we don't want showing error message
			// in this case
			return {};
		} else {
			SessionError session_error;
			session_error.err = SessionError::InvalidSessionFile;
			session_error.message = tr("Failed to open session file. %1").arg(file.errorString());
			return make_unexpected(session_error);
		}
	}

	QByteArray json = file.readAll();
	QJsonParseError error;
	auto doc = QJsonDocument::fromJson(json, &error);
	if(error.error != QJsonParseError::NoError) {
		SessionError session_error;
		session_error.err          = SessionError::UnknownError;
		session_error.message = tr("An error occured while loading session JSON file. %1").arg(error.errorString());
		return make_unexpected(session_error);
	}

	if(!doc.isObject()) {
		SessionError session_error;
		session_error.err          = SessionError::NotAnObject;
		session_error.message = tr("Session file is invalid. Not an object.");
		return make_unexpected(session_error);
	}

	QJsonObject object = doc.object();
	session_data = object.toVariantMap();

	QString id  = session_data["id"].toString();
	QString ts  = session_data["timestamp"].toString();
	int version = session_data["version"].toInt();

	Q_UNUSED(ts)

	if(id != SessionFileIdString || version > SessionFileVersion) {
		SessionError session_error;
		session_error.err          = SessionError::InvalidSessionFile;
		session_error.message = tr("Session file is invalid.");
		return make_unexpected(session_error);
	}

	qDebug("Loading session file");
	load_plugin_data(); //First, load the plugin-data
	return {};
}

//------------------------------------------------------------------------------
// Name: save_session
// Desc:
//------------------------------------------------------------------------------
void SessionManager::save_session(const QString &session_file) {

	qDebug("Saving session file");

	QVariantMap plugin_data;

	for(QObject *plugin: edb::v1::plugin_list()) {
		if(auto p = qobject_cast<IPlugin *>(plugin)) {
			if(const QMetaObject *const meta = plugin->metaObject()) {
				QString name    = meta->className();
				QVariantMap data = p->save_state();

				if(!data.empty()) {
					plugin_data[name] = data;
				}
			}
		}
	}

	session_data["version"]     = SessionFileVersion;
	session_data["id"]          = SessionFileIdString; // just so we can sanity check things
	session_data["timestamp"]   = QDateTime::currentDateTimeUtc();
	session_data["plugin-data"] = plugin_data;

	auto object = QJsonObject::fromVariantMap(session_data);
	QJsonDocument doc(object);

	QByteArray json = doc.toJson();
	QFile file(session_file);

	if(file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		file.write(json);
	}
}

void SessionManager::load_plugin_data() {

	qDebug("Loading plugin-data");

	QVariantMap plugin_data = session_data["plugin-data"].toMap();
	for(auto it = plugin_data.begin(); it != plugin_data.end(); ++it) {
		for(QObject *plugin: edb::v1::plugin_list()) {
			if(auto p = qobject_cast<IPlugin *>(plugin)) {
				if(const QMetaObject *const meta = plugin->metaObject()) {
					QString name     = meta->className();
					QVariantMap data = it.value().toMap();

					if(name == it.key()) {
						p->restore_state(data);
						break;
					}
				}
			}
		}
	}
}

/**
* Gets all comments from the session_data
* @param QVariantList &
*/
void SessionManager::get_comments(QVariantList &data) {
	data = session_data["comments"].toList();
}

/**
* Adds a comment to the session_data
* @param Comment & (struct in Types.h)
*/
void SessionManager::add_comment(Comment &c) {

	QVariantList comments_data = session_data["comments"].toList();
	QVariantMap comment;
	comment["address"] = c.address.toHexString();
	comment["comment"] = c.comment;

	if(!comments_data.isEmpty()) {
		//Check if we already have an entry with the same address and overwrite it
		bool found_comment = false;
		for(auto it = comments_data.begin(); it != comments_data.end(); ++it) {
			QVariantMap data = it->toMap();
			if(comment["address"] == data["address"]) {
				qDebug("Found");
				*it = comment;
				found_comment = true;
				break;
			}
		}

		if(!found_comment) {
			//We found no entry with the same address
			comments_data.push_back(comment);
		}
	} else {
		comments_data.push_back(comment);
	}
	
	session_data["comments"] = comments_data;
}

/**
* Removes a comment from the session_data
* @param edb::address_t
*/
void SessionManager::remove_comment(edb::address_t address) {
	QString hexAddressString = address.toHexString();
	QVariantList comments_data = session_data["comments"].toList();

	for(auto it = comments_data.begin(); it != comments_data.end(); ++it) {
		QVariantMap data = it->toMap();
		if(hexAddressString == data["address"]) {
			comments_data.erase(it);
			break;
		}
	}
	session_data["comments"] = comments_data;
}

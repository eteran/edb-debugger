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
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMessageBox>

namespace {

constexpr int SessionFileVersion = 1;
const auto SessionFileIdString   = QLatin1String("edb-session");

}

/**
 * @brief SessionManager::instance
 * @return
 */
SessionManager &SessionManager::instance() {
	static SessionManager inst;
	return inst;
}

/**
 * @brief SessionManager::loadSession
 * @param filename
 * @return
 */
Result<void, SessionError> SessionManager::loadSession(const QString &filename) {

	QFile file(filename);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
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
			session_error.err     = SessionError::InvalidSessionFile;
			session_error.message = tr("Failed to open session file. %1").arg(file.errorString());
			return make_unexpected(session_error);
		}
	}

	QByteArray json = file.readAll();
	QJsonParseError error;
	auto doc = QJsonDocument::fromJson(json, &error);
	if (error.error != QJsonParseError::NoError) {
		SessionError session_error;
		session_error.err     = SessionError::UnknownError;
		session_error.message = tr("An error occured while loading session JSON file. %1").arg(error.errorString());
		return make_unexpected(session_error);
	}

	if (!doc.isObject()) {
		SessionError session_error;
		session_error.err     = SessionError::NotAnObject;
		session_error.message = tr("Session file is invalid. Not an object.");
		return make_unexpected(session_error);
	}

	QJsonObject object = doc.object();
	sessionData_       = object.toVariantMap();

	QString id  = sessionData_["id"].toString();
	QString ts  = sessionData_["timestamp"].toString();
	int version = sessionData_["version"].toInt();

	Q_UNUSED(ts)

	if (id != SessionFileIdString || version > SessionFileVersion) {
		SessionError session_error;
		session_error.err     = SessionError::InvalidSessionFile;
		session_error.message = tr("Session file is invalid.");
		return make_unexpected(session_error);
	}

	qDebug("Loading session file");
	loadPluginData(); //First, load the plugin-data
	return {};
}

/**
 * @brief SessionManager::saveSession
 * @param filename
 */
void SessionManager::saveSession(const QString &filename) {

	qDebug("Saving session file");

	QVariantMap plugin_data;

	for (QObject *plugin : edb::v1::plugin_list()) {
		if (auto p = qobject_cast<IPlugin *>(plugin)) {
			if (const QMetaObject *const meta = plugin->metaObject()) {
				QString name     = meta->className();
				QVariantMap data = p->saveState();

				if (!data.empty()) {
					plugin_data[name] = data;
				}
			}
		}
	}

	sessionData_["version"]     = SessionFileVersion;
	sessionData_["id"]          = SessionFileIdString; // just so we can sanity check things
	sessionData_["timestamp"]   = QDateTime::currentDateTimeUtc();
	sessionData_["plugin-data"] = plugin_data;

	auto object = QJsonObject::fromVariantMap(sessionData_);
	QJsonDocument doc(object);

	QByteArray json = doc.toJson();
	QFile file(filename);

	if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		file.write(json);
	}
}

/**
 * @brief SessionManager::loadPluginData
 */
void SessionManager::loadPluginData() {

	qDebug("Loading plugin-data");

	QVariantMap plugin_data = sessionData_["plugin-data"].toMap();
	for (auto it = plugin_data.begin(); it != plugin_data.end(); ++it) {
		for (QObject *plugin : edb::v1::plugin_list()) {
			if (auto p = qobject_cast<IPlugin *>(plugin)) {
				if (const QMetaObject *const meta = plugin->metaObject()) {
					QString name     = meta->className();
					QVariantMap data = it.value().toMap();

					if (name == it.key()) {
						p->restoreState(data);
						break;
					}
				}
			}
		}
	}
}

/**
 * @brief SessionManager::breakpoints
 * @return all breakpoints from the sessionData_
 */
QVariantList SessionManager::breakpoints() const {
    return sessionData_["breakpoints"].toList();
}

/**
 * @brief SessionManager::comments
 * @return all comments from the sessionData_
 */
QVariantList SessionManager::comments() const {
	return sessionData_["comments"].toList();
}

/**
 * @brief SessionManager::labels
 * @return all labels from the sessionData_
 */
QVariantList SessionManager::labels() const {
    return sessionData_["labels"].toList();
}

/**
* Adds a breakpoint to the session_data
* @param b
*/
void SessionManager::addBreakpoint(const IBreakpoint &b) {
    QVariantList breakpoints_data = sessionData_["breakpoints"].toList();

    QVariantMap breakpoint;
    breakpoint["address"] = b.address().toHexString();
    breakpoint["enabled"] = b.enabled();
    breakpoint["type"] = quint8(b.type());

    //Check if we already have an entry with the same address and overwrite it
    auto it = std::find_if(breakpoints_data.begin(), breakpoints_data.end(), [&breakpoint](QVariant entry) {
        QVariantMap data = entry.toMap();
        return data["address"] == breakpoint["address"];
    });

    if (it != breakpoints_data.end()) {
        *it = breakpoint;
    } else {
        breakpoints_data.push_back(breakpoint);
    }

    sessionData_["breakpoints"] = breakpoints_data;
}

/**
* Adds a comment to the session_data
* @param c (struct in Types.h)
*/
void SessionManager::addComment(const Comment &c) {

	QVariantList comments_data = sessionData_["comments"].toList();

	QVariantMap comment;
	comment["address"] = c.address.toHexString();
	comment["comment"] = c.comment;

	//Check if we already have an entry with the same address and overwrite it
	auto it = std::find_if(comments_data.begin(), comments_data.end(), [&comment](QVariant entry) {
		QVariantMap data = entry.toMap();
		return data["address"] == comment["address"];
	});

	if (it != comments_data.end()) {
		*it = comment;
	} else {
		comments_data.push_back(comment);
	}

	sessionData_["comments"] = comments_data;
}

/**
* Adds a label to the session_data
* @param l (struct in Types.h)
*/
void SessionManager::addLabel(const Label &l) {

    QVariantList labels_data = sessionData_["labels"].toList();

    QVariantMap label;
    label["address"] = l.address.toHexString();
    label["label"] = l.comment;

    qDebug() << "Add new label " << l.comment;

    //Check if we already have an entry with the same address and overwrite it
    auto it = std::find_if(labels_data.begin(), labels_data.end(), [&label](QVariant entry) {
        QVariantMap data = entry.toMap();
        return data["address"] == label["address"];
    });

    if (it != labels_data.end()) {
        *it = label;
    } else {
        labels_data.push_back(label);
    }

    sessionData_["labels"] = labels_data;
}

/**
* Removes a breakpoint from the session_data
* @param address
*/
void SessionManager::removeBreakpoint(edb::address_t address) {
	QString hexAddressString   = address.toHexString();
    QVariantList breakpoints_data = sessionData_["breakpoints"].toList();

    auto it = std::find_if(breakpoints_data.begin(), breakpoints_data.end(), [&hexAddressString](QVariant entry) {
		QVariantMap data = entry.toMap();
		return data["address"] == hexAddressString;
	});

    if (it != breakpoints_data.end()) {
        breakpoints_data.erase(it);
	}

    sessionData_["breakpoints"] = breakpoints_data;
}

/**
* Removes a label from the session_data
* @param address
*/
void SessionManager::removeComment(edb::address_t address) {
    QString hexAddressString   = address.toHexString();
    QVariantList comments_data = sessionData_["comments"].toList();

    auto it = std::find_if(comments_data.begin(), comments_data.end(), [&hexAddressString](QVariant entry) {
        QVariantMap data = entry.toMap();
        return data["address"] == hexAddressString;
    });

    if (it != comments_data.end()) {
        comments_data.erase(it);
    }

    sessionData_["comments"] = comments_data;
}

/**
* Removes a label from the session_data
* @param address
*/
void SessionManager::removeLabel(edb::address_t address) {
    QString hexAddressString   = address.toHexString();
    QVariantList labels_data = sessionData_["labels"].toList();

    auto it = std::find_if(labels_data.begin(), labels_data.end(), [&hexAddressString](QVariant entry) {
        QVariantMap data = entry.toMap();
        return data["address"] == hexAddressString;
    });

    if (it != labels_data.end()) {
        labels_data.erase(it);
    }

    sessionData_["labels"] = labels_data;
}

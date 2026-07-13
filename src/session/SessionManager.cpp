/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
const auto SessionFileIdString   = QStringLiteral("edb-session");

}

/**
 * @brief Returns the instance of the session manager.
 *
 * @return A reference to the session manager instance.
 */
SessionManager &SessionManager::instance() {
	static SessionManager inst;
	return inst;
}

/**
 * @brief Loads a session from the specified file.
 *
 * @param filename The path to the session file to load.
 * @return A result indicating success or an error.
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
		}

		SessionError session_error;
		session_error.err     = SessionError::InvalidSessionFile;
		session_error.message = tr("Failed to open session file. %1").arg(file.errorString());
		return make_unexpected(session_error);
	}

	QByteArray json = file.readAll();
	QJsonParseError error;
	auto doc = QJsonDocument::fromJson(json, &error);
	if (error.error != QJsonParseError::NoError) {
		SessionError session_error;
		session_error.err     = SessionError::UnknownError;
		session_error.message = tr("An error occurred while loading session JSON file. %1").arg(error.errorString());
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

	QString id  = sessionData_[QStringLiteral("id")].toString();
	QString ts  = sessionData_[QStringLiteral("timestamp")].toString();
	int version = sessionData_[QStringLiteral("version")].toInt();

	Q_UNUSED(ts)

	if (id != SessionFileIdString || version > SessionFileVersion) {
		SessionError session_error;
		session_error.err     = SessionError::InvalidSessionFile;
		session_error.message = tr("Session file is invalid.");
		return make_unexpected(session_error);
	}

	qDebug("Loading session file");
	loadPluginData(); // First, load the plugin-data
	return {};
}

/**
 * @brief Saves the current session to the specified file.
 *
 * @param filename The path to the session file to save.
 */
void SessionManager::saveSession(const QString &filename) {

	qDebug("Saving session file");

	QVariantMap plugin_data;

	for (QObject *plugin : edb::v1::plugin_list()) {
		if (auto p = qobject_cast<IPlugin *>(plugin)) {
			if (const QMetaObject *const meta = plugin->metaObject()) {
				auto name        = QString::fromLocal8Bit(meta->className());
				QVariantMap data = p->saveState();

				if (!data.empty()) {
					plugin_data[name] = data;
				}
			}
		}
	}

	sessionData_[QStringLiteral("version")]     = SessionFileVersion;
	sessionData_[QStringLiteral("id")]          = SessionFileIdString; // just so we can sanity check things
	sessionData_[QStringLiteral("timestamp")]   = QDateTime::currentDateTimeUtc();
	sessionData_[QStringLiteral("plugin-data")] = plugin_data;

	auto object = QJsonObject::fromVariantMap(sessionData_);
	QJsonDocument doc(object);

	QByteArray json = doc.toJson();
	QFile file(filename);

	if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		file.write(json);
	}
}

/**
 * @brief Loads the plugin data from the session.
 */
void SessionManager::loadPluginData() {

	qDebug("Loading plugin-data");

	QVariantMap plugin_data = sessionData_[QStringLiteral("plugin-data")].toMap();
	for (auto it = plugin_data.begin(); it != plugin_data.end(); ++it) {
		for (QObject *plugin : edb::v1::plugin_list()) {
			if (auto p = qobject_cast<IPlugin *>(plugin)) {
				if (const QMetaObject *const meta = plugin->metaObject()) {
					auto name        = QString::fromLocal8Bit(meta->className());
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
 * @brief Returns all comments in the session
 *
 * @return A list of all comments in the session.
 */
QVariantList SessionManager::comments() const {
	return sessionData_[QStringLiteral("comments")].toList();
}

/**
 * @brief Adds a comment to the session
 *
 * @param c The comment to add.
 */
void SessionManager::addComment(const Comment &c) {

	QVariantList comments_data = sessionData_[QStringLiteral("comments")].toList();

	QVariantMap comment;
	comment[QStringLiteral("address")] = c.address.toHexString();
	comment[QStringLiteral("comment")] = c.comment;

	// Check if we already have an entry with the same address and overwrite it
	auto it = std::find_if(comments_data.begin(), comments_data.end(), [&comment](QVariant entry) {
		QVariantMap data = entry.toMap();
		return data[QStringLiteral("address")] == comment[QStringLiteral("address")];
	});

	if (it != comments_data.end()) {
		*it = comment;
	} else {
		comments_data.push_back(comment);
	}

	sessionData_[QStringLiteral("comments")] = comments_data;
}

/**
 * @brief Removes a comment from the session_data
 *
 * @param address The address of the comment to remove.
 */
void SessionManager::removeComment(edb::address_t address) {
	QString hexAddressString   = address.toHexString();
	QVariantList comments_data = sessionData_[QStringLiteral("comments")].toList();

	auto it = std::find_if(comments_data.begin(), comments_data.end(), [&hexAddressString](QVariant entry) {
		QVariantMap data = entry.toMap();
		return data[QStringLiteral("address")] == hexAddressString;
	});

	if (it != comments_data.end()) {
		comments_data.erase(it);
	}

	sessionData_[QStringLiteral("comments")] = comments_data;
}

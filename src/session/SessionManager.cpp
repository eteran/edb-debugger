/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "SessionManager.h"
#include "IDebugger.h"
#include "IPlugin.h"
#include "IProcess.h"
#include "Module.h"
#include "QDisassemblyView.h"
#include "SymbolManager.h"
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

	QString id               = sessionData_[QStringLiteral("id")].toString();
	QString ts               = sessionData_[QStringLiteral("timestamp")].toString();
	int version              = sessionData_[QStringLiteral("version")].toInt();
	QVariantList labels      = sessionData_[QStringLiteral("labels")].toList();
	QVariantList comments    = sessionData_[QStringLiteral("comments")].toList();
	QVariantList breakpoints = sessionData_[QStringLiteral("breakpoints")].toList();

	loadLabels(labels);
	loadComments(comments);
	loadBreakpoints(breakpoints);

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
	sessionData_[QStringLiteral("labels")]      = saveLabels();
	sessionData_[QStringLiteral("comments")]    = saveComments();
	sessionData_[QStringLiteral("breakpoints")] = saveBreakpoints();

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
 * @brief Saves the labels to a QVariantMap for session persistence.
 *
 * @return A QVariantMap containing the labels.
 */
QVariantList SessionManager::saveLabels() const {
	QVector<ISymbolManager::LabelEntry> labels = edb::v1::symbol_manager().labelData();

	QVariantList label_data;

	for (const auto &label : labels) {

		edb::address_t address = label.module ? label.address - label.module->baseAddress : edb::address_t();
		QString name           = label.name;

		QVariantMap entry;
		entry[QStringLiteral("module")] = label.module ? label.module->name : QString();
		entry[QStringLiteral("offset")] = address.toHexString();
		entry[QStringLiteral("name")]   = name;
		label_data.push_back(entry);
	}

	return label_data;
}

/**
 * @brief Saves the comments to a QVariantMap for session persistence.
 *
 * @return A QVariantMap containing the comments.
 */
QVariantList SessionManager::saveComments() const {

	auto cpuView = qobject_cast<QDisassemblyView *>(edb::v1::disassembly_widget());
	if (!cpuView) {
		qDebug() << "Failed to get disassembly widget";
		return {};
	}
	QVector<Comment> comments = cpuView->commentData();

	QVariantList comment_data;

	for (const auto &comment : comments) {

		edb::address_t address = comment.module ? comment.address - comment.module->baseAddress : edb::address_t();
		QString comment_text   = comment.comment;

		QVariantMap entry;
		entry[QStringLiteral("module")] = comment.module ? comment.module->name : QString();
		entry[QStringLiteral("offset")] = address.toHexString();
		entry[QStringLiteral("name")]   = comment_text;
		comment_data.push_back(entry);
	}

	return comment_data;
}

/**
 * @brief Loads the labels from a QVariantMap for session restoration.
 *
 * @param labels A QVariantList containing the labels to load.
 */
void SessionManager::loadLabels(const QVariantList &labels) {

	qDebug("Loading labels");

	IProcess *process = edb::v1::debugger_core->process();
	Q_ASSERT(process);

	QSet<Module> modules = process->loadedModules();

	for (auto &entry : labels) {
		auto label = entry.value<QVariantMap>();

		QString module_name = label[QStringLiteral("module")].toString();
		QString offset_str  = label[QStringLiteral("offset")].toString();
		QString name        = label[QStringLiteral("name")].toString();

		edb::address_t offset = edb::address_t::fromHexString(offset_str);

		// Figure out which module this bookmark belongs to and add it if the module is loaded
		auto it = std::find_if(modules.begin(), modules.end(), [&module_name](const Module &module) {
			return edb::v2::compare_module_names(module.name, module_name);
		});

		if (it != modules.end()) {
			edb::address_t address = offset + it->baseAddress;
			edb::v1::symbol_manager().setLabel(address, name);
			continue;
		} else {

			// If the module is not loaded, store the bookmark entry for later restoration when the module is loaded
			LabelEntry entry;
			entry.name   = name;
			entry.module = module_name;
			entry.offset = offset_str;
			deferredLabels_.push_back(entry);
		}
	}
}

/**
 * @brief Loads the comments from a QVariantMap for session restoration.
 *
 * @param comments A QVariantList containing the comments to load.
 */
void SessionManager::loadComments(const QVariantList &comments) {
	qDebug("Loading comments");

	IProcess *process = edb::v1::debugger_core->process();
	Q_ASSERT(process);

	QSet<Module> modules = process->loadedModules();

	auto cpuView = qobject_cast<QDisassemblyView *>(edb::v1::disassembly_widget());
	if (!cpuView) {
		qDebug() << "Failed to get disassembly widget";
		return;
	}

	for (auto &entry : comments) {
		auto comment = entry.value<QVariantMap>();

		QString module_name = comment[QStringLiteral("module")].toString();
		QString offset_str  = comment[QStringLiteral("offset")].toString();
		QString name        = comment[QStringLiteral("name")].toString();

		edb::address_t offset = edb::address_t::fromHexString(offset_str);

		// Figure out which module this bookmark belongs to and add it if the module is loaded
		auto it = std::find_if(modules.begin(), modules.end(), [&module_name](const Module &module) {
			return edb::v2::compare_module_names(module.name, module_name);
		});

		if (it != modules.end()) {
			edb::address_t address = offset + it->baseAddress;
			cpuView->addComment(address, name);
			continue;
		} else {

			// If the module is not loaded, store the bookmark entry for later restoration when the module is loaded
			Comment entry;
			entry.comment = name;
			entry.module  = edb::v2::module_for_address(offset);
			entry.address = offset;
			deferredComments_.push_back(entry);
		}
	}
}

void SessionManager::libraryEvent(const Module &module, bool loaded) {
	if (loaded) {
		// Load labels for the module that was just loaded
		{
			auto it = std::remove_if(deferredLabels_.begin(), deferredLabels_.end(), [&module, this](const LabelEntry &entry) {
				if (edb::v2::compare_module_names(entry.module, module.name)) {
					edb::address_t offset  = edb::address_t::fromHexString(entry.offset);
					edb::address_t address = offset + module.baseAddress;
					edb::v1::symbol_manager().setLabel(address, entry.name);
					return true;
				}

				return false;
			});
			deferredLabels_.erase(it, deferredLabels_.end());
		}

		// Load comments for the module that was just loaded
		{
			auto cpuView = qobject_cast<QDisassemblyView *>(edb::v1::disassembly_widget());
			if (!cpuView) {
				qDebug() << "Failed to get disassembly widget";
				return;
			}

			auto it = std::remove_if(deferredComments_.begin(), deferredComments_.end(), [&module, cpuView, this](const Comment &entry) {
				if (entry.module && edb::v2::compare_module_names(entry.module->name, module.name)) {
					edb::address_t address = entry.address + module.baseAddress;
					cpuView->addComment(address, entry.comment);
					return true;
				}

				return false;
			});
			deferredComments_.erase(it, deferredComments_.end());
		}
	}
}

QVariantList SessionManager::saveBreakpoints() const {
	QVariantList breakpoints;

	const IDebugger::BreakpointList breakpoint_state = edb::v1::debugger_core->backupBreakpoints();
	for (const std::shared_ptr<IBreakpoint> &bp : breakpoint_state) {
		if (bp->internal()) {
			continue;
		}

		// TODO(eteran): make relative to the module

		const edb::address_t address = bp->address();
		const QString condition      = bp->condition;
		const bool onetime           = bp->oneTime();

		QVariantMap entry;
		entry[QStringLiteral("module")]    = QString();
		entry[QStringLiteral("offset")]    = address.toHexString();
		entry[QStringLiteral("condition")] = condition;
		entry[QStringLiteral("one_time")]  = onetime;
		breakpoints.push_back(entry);
	}

	return breakpoints;
}

void SessionManager::loadBreakpoints(const QVariantList &breakpoints) {
}

/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SESSION_MANAGER_H_20170928_
#define SESSION_MANAGER_H_20170928_

#include "Comment.h"
#include "SessionError.h"
#include "Status.h"
#include "Types.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>

class Module;

class SessionManager {
	Q_DECLARE_TR_FUNCTIONS(SessionManager)

private:
	struct LabelEntry {
		QString name;
		QString module;
		QString offset;
	};

	struct BreakpointEntry {
		QString module;
		QString offset;
		QString condition;
		bool oneTime = false;
	};

private:
	SessionManager() = default;

public:
	SessionManager(const SessionManager &)            = delete;
	SessionManager &operator=(const SessionManager &) = delete;

public:
	static SessionManager &instance();

public:
	Result<void, SessionError> loadSession(const QString &filename);
	void saveSession(const QString &filename);

public:
	void libraryEvent(const Module &module, bool loaded);

private:
	void loadPluginData(const QJsonObject &plugin_data);

	QJsonArray saveLabels() const;
	void loadLabels(const QJsonArray &labels);

	QJsonArray saveComments() const;
	void loadComments(const QJsonArray &comments);

	QJsonArray saveBreakpoints() const;
	void loadBreakpoints(const QJsonArray &breakpoints);

private:
	std::vector<LabelEntry> deferredLabels_;
	std::vector<Comment> deferredComments_;
	std::vector<BreakpointEntry> deferredBreakpoints_;
};

#endif

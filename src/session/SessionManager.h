/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SESSION_MANAGER_H_20170928_
#define SESSION_MANAGER_H_20170928_

#include "SessionError.h"
#include "Status.h"
#include "Types.h"

#include <QCoreApplication>
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
	[[nodiscard]] QVariantList comments() const;
	void addComment(const Comment &c);
	void removeComment(edb::address_t address);

public:
	void libraryEvent(const Module &module, bool loaded);

private:
	void loadPluginData();
	QVariantList saveLabels() const;
	void loadLabels(const QVariantList &labels);

private:
	QVariantMap sessionData_;
	std::vector<LabelEntry> deferredLabels_;
};

#endif

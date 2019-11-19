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

#ifndef SESSION_MANAGER_H_20170928_
#define SESSION_MANAGER_H_20170928_

#include "SessionError.h"
#include "Status.h"
#include "Types.h"

#include <QCoreApplication>
#include <QString>
#include <QVariant>

class SessionManager {
	Q_DECLARE_TR_FUNCTIONS(SessionManager)

private:
	SessionManager() = default;

public:
	SessionManager(const SessionManager &) = delete;
	SessionManager &operator=(const SessionManager &) = delete;

public:
	static SessionManager &instance();

public:
	Result<void, SessionError> loadSession(const QString &filename);
	void saveSession(const QString &filename);
	QVariantList comments() const;
	void addComment(const Comment &c);
	void removeComment(edb::address_t address);

private:
	void loadPluginData();

private:
	QVariantMap sessionData_;
};

#endif

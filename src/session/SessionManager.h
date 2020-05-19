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

#include "ISessionManager.h"
#include "SessionError.h"

#include <QCoreApplication>

class SessionManager final : public ISessionManager {
	Q_DECLARE_TR_FUNCTIONS(SessionManager)

private:
	SessionManager() = default;

public:
	SessionManager(const SessionManager &) = delete;
	SessionManager &operator=(const SessionManager &) = delete;

public:
	static SessionManager &instance();

public:
	Result<void, SessionError> loadSession(const QString &filename) override;
	void saveSession(const QString &filename) override;
	QVariantList comments() const override;
	void addComment(const Comment &c) override;
	void removeComment(edb::address_t address) override;

private:
	void loadPluginData();

private:
	QVariantMap sessionData_;
};

#endif

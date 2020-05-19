/*
Copyright (C) 2020 Victorien Molle <biche@biche.re>

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

#ifndef ISESSION_MANAGER_H_20200519_
#define ISESSION_MANAGER_H_20200519_

#include "IBreakpoint.h"
#include "Status.h"
#include "Types.h"

#include <QString>
#include <QVariant>

class QString;
class SessionError;

class ISessionManager {
public:
	virtual ~ISessionManager() = default;

public:
    virtual Result<void, SessionError> loadSession(const QString &filename) = 0;
    virtual void saveSession(const QString &filename)                       = 0;
    virtual QVariantList breakpoints() const                                = 0;
    virtual QVariantList comments() const                                   = 0;
    virtual QVariantList labels() const                                     = 0;
    virtual void addBreakpoint(const IBreakpoint &b)                        = 0;
    virtual void addComment(const Comment &c)                               = 0;
    virtual void addLabel(const Label &l)                                   = 0;
    virtual void removeBreakpoint(edb::address_t address)                   = 0;
    virtual void removeComment(edb::address_t address)                      = 0;
    virtual void removeLabel(edb::address_t address)                        = 0;
};

#endif

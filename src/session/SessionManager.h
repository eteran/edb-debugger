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

#ifndef SESSIONMANAGER_20170928_H_
#define SESSIONMANAGER_20170928_H_

#include "SessionError.h"
#include "Types.h"

#include <QString>
#include <QVariant>

class SessionManager {
public:
  static SessionManager &instance();
  bool load_session(const QString &, SessionError&);
  void save_session(const QString &);
  void get_comments(QVariantList &);
  void add_comment(Comment &);
  void remove_comment(edb::address_t);
private:
  QVariantMap session_data;
  SessionManager() {}
  SessionManager( const SessionManager& );
  SessionManager & operator = (const SessionManager &);
  void load_plugin_data();
};

#endif

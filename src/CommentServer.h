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

#ifndef COMMENT_SERVER_H_20070427_
#define COMMENT_SERVER_H_20070427_

#include "Status.h"
#include "Types.h"

#include <QCoreApplication>
#include <QHash>
#include <QString>

class CommentServer {
	Q_DECLARE_TR_FUNCTIONS(CommentServer)
public:
	void setComment(edb::address_t address, const QString &comment);
	QString comment(edb::address_t address, int size) const;
	void clear();

private:
	Result<QString, QString> resolveFunctionCall(edb::address_t address) const;
	Result<QString, QString> resolveString(edb::address_t address) const;

private:
	QHash<quint64, QString> customComments_;
};

#endif

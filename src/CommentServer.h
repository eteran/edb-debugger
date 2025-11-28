/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
	[[nodiscard]] QString comment(edb::address_t address, int size) const;
	void clear();
	void setComment(edb::address_t address, const QString &comment);

private:
	[[nodiscard]] Result<QString, QString> resolveFunctionCall(edb::address_t address) const;
	[[nodiscard]] Result<QString, QString> resolveString(edb::address_t address) const;

private:
	QHash<quint64, QString> customComments_;
};

#endif

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

#ifndef DEBUGGERCOREUNIX_20090529_H_
#define DEBUGGERCOREUNIX_20090529_H_

#include "DebuggerCoreBase.h"
#include <cerrno>

class Status;

namespace DebuggerCorePlugin {

class DebuggerCoreUNIX : public DebuggerCoreBase {
public:
	DebuggerCoreUNIX();
	~DebuggerCoreUNIX() override = default;

protected:
	Status execute_process(const QString &path, const QString &cwd, const QList<QByteArray> &args);

public:
	QMap<qlonglong, QString> exceptions() const override;
	QString                  exceptionName(qlonglong value) override;
	qlonglong                exceptionValue(const QString &name) override;
};

}

#endif

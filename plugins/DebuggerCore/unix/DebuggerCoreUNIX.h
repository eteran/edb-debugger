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
#include <QList>
#include <cerrno>

namespace DebuggerCore {

namespace native {

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
int select_ex(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, quint64 msecs);
pid_t waitpid(pid_t pid, int *status, int options);
pid_t waitpid_timeout(pid_t pid, int *status, int options, int msecs, bool *timeout);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
bool wait_for_sigchld(int msecs);

}

class DebuggerCoreUNIX : public DebuggerCoreBase {
public:
	DebuggerCoreUNIX();
	virtual ~DebuggerCoreUNIX() {}

protected:
	void execute_process(const QString &path, const QString &cwd, const QList<QByteArray> &args);

public:
	virtual QMap<long, QString> exceptions() const;
};

}

#endif

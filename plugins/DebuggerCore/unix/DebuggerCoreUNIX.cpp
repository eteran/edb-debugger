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

// this code is common to all unix variants (linux/bsd/osx)

#include "DebuggerCoreUNIX.h"
#include "edb.h"

#include <QStringList>
#include <cerrno>
#include <climits>
#include <csignal>
#include <cstring>
#include <cstdio>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace DebuggerCore {

namespace {

int selfpipe[2];
void (*old_sigchld_handler)(int sig, siginfo_t *, void *);

//------------------------------------------------------------------------------
// Name: sigchld_handler
// Desc:
//------------------------------------------------------------------------------
void sigchld_handler(int sig, siginfo_t *info, void *p) {
	if(sig == SIGCHLD) {
		native::write(selfpipe[1], " ", sizeof(char));
	}

	if(old_sigchld_handler) {
		old_sigchld_handler(sig, info, p);
	}
}

}

//------------------------------------------------------------------------------
// Name: read
// Desc: read, but handles being interrupted
//------------------------------------------------------------------------------
ssize_t native::read(int fd, void *buf, size_t count) {
	ssize_t ret;
	do {
		ret = ::read(fd, buf, count);
	} while(ret == -1 && errno == EINTR);
	return ret;
}

//------------------------------------------------------------------------------
// Name: write
// Desc: write, but handles being interrupted
//------------------------------------------------------------------------------
ssize_t native::write(int fd, const void *buf, size_t count) {
	ssize_t ret;
	do {
		ret = ::write(fd, buf, count);
	} while(ret == -1 && errno == EINTR);
	return ret;
}

//------------------------------------------------------------------------------
// Name: select
// Desc: select but handles being interrupted
//------------------------------------------------------------------------------
int native::select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
	int ret;
	do {
		ret = ::select(nfds, readfds, writefds, exceptfds, timeout);
	} while(ret == -1 && errno == EINTR);
	return ret;
}

//------------------------------------------------------------------------------
// Name: waitpid
// Desc: waitpid, but handles being interrupted
//------------------------------------------------------------------------------
pid_t native::waitpid(pid_t pid, int *status, int options) {
	pid_t ret;
	do {
		ret = ::waitpid(pid, status, options);
	} while(ret == -1 && errno == EINTR);
	return ret;
}

//------------------------------------------------------------------------------
// Name: select_ex
// Desc: similar to select but has the timeout specified as an unsigned quantity of msecs
// Note: msecs == 0 means wait forever.
//------------------------------------------------------------------------------
int native::select_ex(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, quint64 msecs) {
	if(msecs != 0) {
		struct timeval tv;

		tv.tv_sec  = (msecs / 1000);
		tv.tv_usec = (msecs % 1000) * 1000;

		return native::select(nfds, readfds, writefds, exceptfds, &tv);
	} else {
		return native::select(nfds, readfds, writefds, exceptfds, NULL);
	}
}

//------------------------------------------------------------------------------
// Name: wait_for_sigchld
// Desc:
//------------------------------------------------------------------------------
bool native::wait_for_sigchld(int msecs) {

	fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(selfpipe[0], &rfds);

	if(native::select_ex(selfpipe[0] + 1, &rfds, 0, 0, msecs) == 0) {
		return true;
	}

	char ch;
	if(native::read(selfpipe[0], &ch, sizeof(char)) == -1) {
		return true;
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: waitpid_timeout
// Desc:
//------------------------------------------------------------------------------
pid_t native::waitpid_timeout(pid_t pid, int *status, int options, int msecs, bool *timeout) {

	Q_ASSERT(pid > 0);
	Q_ASSERT(timeout);

	*timeout = wait_for_sigchld(msecs);

	if(!*timeout) {
		return native::waitpid(pid, status, options | WNOHANG);
	}

	return -1;
}

//------------------------------------------------------------------------------
// Name: DebuggerCoreUNIX
// Desc:
//------------------------------------------------------------------------------
DebuggerCoreUNIX::DebuggerCoreUNIX() {

	// create a pipe and make it non-blocking
	int r = ::pipe(selfpipe);
	Q_UNUSED(r);

	::fcntl(selfpipe[0], F_SETFL, ::fcntl(selfpipe[0], F_GETFL) | O_NONBLOCK);
	::fcntl(selfpipe[1], F_SETFL, ::fcntl(selfpipe[1], F_GETFL) | O_NONBLOCK);

	// setup a signal handler
	struct sigaction new_action;
	struct sigaction old_action;

	std::memset(&new_action, 0, sizeof(new_action));
	std::memset(&old_action, 0, sizeof(old_action));

	new_action.sa_sigaction = sigchld_handler;
	new_action.sa_flags     = SA_RESTART | SA_SIGINFO;

	sigaction(SIGCHLD, &new_action, &old_action);

	old_sigchld_handler = old_action.sa_sigaction;
}


//------------------------------------------------------------------------------
// Name: execute_process
// Desc:
//------------------------------------------------------------------------------
void DebuggerCoreUNIX::execute_process(const QString &path, const QString &cwd, const QList<QByteArray> &args) {
	// change to the desired working directory
	if(::chdir(qPrintable(cwd)) == 0) {

		// allocate space for all the arguments
		auto argv_pointers = new char *[args.count() + 2];

		char **p = argv_pointers;

		*p = new char[path.length() + 1];
		std::strcpy(*p, qPrintable(path));
		++p;

		for(int i = 0; i < args.count(); ++i) {
			const QByteArray s(args[i]);
			*p = new char[s.length() + 1];
			std::strcpy(*p, s.constData());
			++p;
		}

		*p = 0;

		const int ret = execvp(argv_pointers[0], argv_pointers);

		// should be no need to cleanup, the process which allocated all that
		// space no longer exists!
		// if we get here...execvp failed!
		if(ret == -1) {
			p = argv_pointers;
			while(*p) {
				delete [] *p++;
			}
			delete [] argv_pointers;
		}
	}
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QMap<long, QString> DebuggerCoreUNIX::exceptions() const {
	QMap<long, QString> exceptions;


	#ifdef SIGABRT
		exceptions[SIGABRT] = "SIGABRT";
	#endif
	#ifdef SIGALRM
		exceptions[SIGALRM] = "SIGALRM";
	#endif
	#ifdef SIGVTALRM
		exceptions[SIGVTALRM] = "SIGVTALRM";
	#endif
	#ifdef SIGPROF
		exceptions[SIGPROF] = "SIGPROF";
	#endif
	#ifdef SIGBUS
		exceptions[SIGBUS] = "SIGBUS";
	#endif
	#ifdef SIGCHLD
		exceptions[SIGCHLD] = "SIGCHLD";
	#endif
	#ifdef SIGCONT
		exceptions[SIGCONT] = "SIGCONT";
	#endif
	#ifdef SIGFPE
		exceptions[SIGFPE] = "SIGFPE";
	#endif
	#ifdef SIGHUP
		exceptions[SIGHUP] = "SIGHUP";
	#endif
	#ifdef SIGILL
		exceptions[SIGILL] = "SIGILL";
	#endif
	#ifdef SIGINT
		exceptions[SIGINT] = "SIGINT";
	#endif
	#ifdef SIGKILL
		exceptions[SIGKILL] = "SIGKILL";
	#endif
	#ifdef SIGPIPE
		exceptions[SIGPIPE] = "SIGPIPE";
	#endif
	#ifdef SIGQUIT
		exceptions[SIGQUIT] = "SIGQUIT";
	#endif
	#ifdef SIGSEGV
		exceptions[SIGSEGV] = "SIGSEGV";
	#endif
	#ifdef SIGSTOP
		exceptions[SIGSTOP] = "SIGSTOP";
	#endif
	#ifdef SIGTERM
		exceptions[SIGTERM] = "SIGTERM";
	#endif
	#ifdef SIGTSTP
		exceptions[SIGTSTP] = "SIGTSTP";
	#endif
	#ifdef SIGTTIN
		exceptions[SIGTTIN] = "SIGTTIN";
	#endif
	#ifdef SIGTTOU
		exceptions[SIGTTOU] = "SIGTTOU";
	#endif
	#ifdef SIGUSR1
		exceptions[SIGUSR1] = "SIGUSR1";
	#endif
	#ifdef SIGUSR2
		exceptions[SIGUSR2] = "SIGUSR2";
	#endif
	#ifdef SIGPOLL
		exceptions[SIGPOLL] = "SIGPOLL";
	#endif
	#ifdef SIGSYS
		exceptions[SIGSYS] = "SIGSYS";
	#endif
	#ifdef SIGTRAP
		exceptions[SIGTRAP] = "SIGTRAP";
	#endif
	#ifdef SIGURG
		exceptions[SIGURG] = "SIGURG";
	#endif
	#ifdef SIGXCPU
		exceptions[SIGXCPU] = "SIGXCPU";
	#endif
	#ifdef SIGXFSZ
		exceptions[SIGXFSZ] = "SIGXFSZ";
	#endif
	#ifdef SIGRTMIN
		exceptions[SIGRTMIN] = "SIGRTMIN";
	#endif
	#ifdef SIGRTMAX
		exceptions[SIGRTMAX] = "SIGRTMAX";
	#endif
	#ifdef SIGIO
		exceptions[SIGIO] = "SIGIO";
	#endif
	#ifdef SIGSTKFLT
		exceptions[SIGSTKFLT] = "SIGSTKFLT";
	#endif
	#ifdef SIGWINCH
		exceptions[SIGWINCH] = "SIGWINCH";
	#endif
	return exceptions;
}

}

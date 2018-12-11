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
#include "Posix.h"

#include <QtGlobal>
#include <QProcess>
#include <QStringList>
#include <signal.h>

namespace DebuggerCorePlugin {
namespace {

struct Exception {
    qlonglong         value;
	const char *const name;
} Exceptions[] = {
#ifdef SIGABRT
    { SIGABRT, "SIGABRT" },
#endif
#ifdef SIGALRM
    { SIGALRM, "SIGALRM" },
#endif
#ifdef SIGVTALRM
    { SIGVTALRM, "SIGVTALRM" },
#endif
#ifdef SIGPROF
    { SIGPROF, "SIGPROF" },
#endif
#ifdef SIGBUS
    { SIGBUS, "SIGBUS" },
#endif
#ifdef SIGCHLD
    { SIGCHLD, "SIGCHLD" },
#endif
#ifdef SIGCONT
    { SIGCONT, "SIGCONT" },
#endif
#ifdef SIGFPE
    { SIGFPE, "SIGFPE" },
#endif
#ifdef SIGHUP
    { SIGHUP, "SIGHUP" },
#endif
#ifdef SIGILL
    { SIGILL, "SIGILL" },
#endif
#ifdef SIGINT
    { SIGINT, "SIGINT" },
#endif
#ifdef SIGKILL
    { SIGKILL, "SIGKILL" },
#endif
#ifdef SIGPIPE
    { SIGPIPE, "SIGPIPE" },
#endif
#ifdef SIGQUIT
    { SIGQUIT, "SIGQUIT" },
#endif
#ifdef SIGSEGV
    { SIGSEGV, "SIGSEGV" },
#endif
#ifdef SIGSTOP
    { SIGSTOP, "SIGSTOP" },
#endif
#ifdef SIGTERM
    { SIGTERM, "SIGTERM" },
#endif
#ifdef SIGTSTP
    { SIGTSTP, "SIGTSTP" },
#endif
#ifdef SIGTTIN
    { SIGTTIN, "SIGTTIN" },
#endif
#ifdef SIGTTOU
    { SIGTTOU, "SIGTTOU" },
#endif
#ifdef SIGUSR1
    { SIGUSR1, "SIGUSR1" },
#endif
#ifdef SIGUSR2
    { SIGUSR2, "SIGUSR2" },
#endif
#ifdef SIGPOLL
    { SIGPOLL, "SIGPOLL" },
#endif
#ifdef SIGSYS
    { SIGSYS, "SIGSYS" },
#endif
#ifdef SIGTRAP
    { SIGTRAP, "SIGTRAP" },
#endif
#ifdef SIGURG
    { SIGURG, "SIGURG" },
#endif
#ifdef SIGXCPU
    { SIGXCPU, "SIGXCPU" },
#endif
#ifdef SIGXFSZ
    { SIGXFSZ, "SIGXFSZ" },
#endif
#ifdef SIGRTMIN
    { SIGRTMIN, "SIGRTMIN" },
#endif
#ifdef SIGRTMAX
    { SIGRTMAX, "SIGRTMAX" },
#endif
#ifdef SIGIO
    { SIGIO, "SIGIO" },
#endif
#ifdef SIGSTKFLT
    { SIGSTKFLT, "SIGSTKFLT" },
#endif
#ifdef SIGWINCH
    { SIGWINCH, "SIGWINCH" },
#endif
};

}

//------------------------------------------------------------------------------
// Name: DebuggerCoreUNIX
// Desc:
//------------------------------------------------------------------------------
DebuggerCoreUNIX::DebuggerCoreUNIX() {
	Posix::initialize();
}

//------------------------------------------------------------------------------
// Name: execute_process
// Desc: tries to execute the process, returns error string on error
//------------------------------------------------------------------------------
Status DebuggerCoreUNIX::execute_process(const QString &path, const QString &cwd, const QList<QByteArray> &args) {

	QString errorString = "internal error";
	
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

		*p = nullptr;

		// NOTE: it's a bad idea to use execvp and similar functions searching in
		// $PATH. At least on Linux, if the file is corrupted/unsupported, they
		// instead appear to launch shell
		const int ret = execv(argv_pointers[0], argv_pointers);

		// should be no need to cleanup, the process which allocated all that
		// space no longer exists!
		// if we get here...execv failed!
		if(ret == -1) {
			errorString = tr("execv() failed: %1").arg(strerror(errno));
			p = argv_pointers;
			while(*p) {
				delete [] *p++;
			}
			delete [] argv_pointers;
		}
	}
	
	// frankly, any return is technically an error I think
	// this is only executed from a fork
	return Status(errorString);
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString DebuggerCoreUNIX::exceptionName(qlonglong value) {

	auto it = std::find_if(std::begin(Exceptions), std::end(Exceptions), [value](Exception &ex) {
	    return ex.value == value;
    });

	if(it != std::end(Exceptions)) {
		return it->name;
	}

    return QString();
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
qlonglong DebuggerCoreUNIX::exceptionValue(const QString &name) {

	auto it = std::find_if(std::begin(Exceptions), std::end(Exceptions), [&name](Exception &ex) {
	    return ex.name == name;
    });

	if(it != std::end(Exceptions)) {
		return it->value;
	}

    return -1;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QMap<qlonglong, QString> DebuggerCoreUNIX::exceptions() const {

    QMap<qlonglong, QString> exceptions;
    for(Exception e : Exceptions) {
        exceptions[e.value] = e.name;
    }
	return exceptions;
}

}


#include "Unix.h"
#include <cstring>
#include <signal.h>

namespace DebuggerCorePlugin {
namespace {

struct Exception {
	qlonglong value;
	const char *const name;
};

constexpr Exception Exceptions[] = {
#ifdef SIGABRT
	{SIGABRT, "SIGABRT"},
#endif
#ifdef SIGALRM
	{SIGALRM, "SIGALRM"},
#endif
#ifdef SIGVTALRM
	{SIGVTALRM, "SIGVTALRM"},
#endif
#ifdef SIGPROF
	{SIGPROF, "SIGPROF"},
#endif
#ifdef SIGBUS
	{SIGBUS, "SIGBUS"},
#endif
#ifdef SIGCHLD
	{SIGCHLD, "SIGCHLD"},
#endif
#ifdef SIGCONT
	{SIGCONT, "SIGCONT"},
#endif
#ifdef SIGFPE
	{SIGFPE, "SIGFPE"},
#endif
#ifdef SIGHUP
	{SIGHUP, "SIGHUP"},
#endif
#ifdef SIGILL
	{SIGILL, "SIGILL"},
#endif
#ifdef SIGINT
	{SIGINT, "SIGINT"},
#endif
#ifdef SIGKILL
	{SIGKILL, "SIGKILL"},
#endif
#ifdef SIGPIPE
	{SIGPIPE, "SIGPIPE"},
#endif
#ifdef SIGQUIT
	{SIGQUIT, "SIGQUIT"},
#endif
#ifdef SIGSEGV
	{SIGSEGV, "SIGSEGV"},
#endif
#ifdef SIGSTOP
	{SIGSTOP, "SIGSTOP"},
#endif
#ifdef SIGTERM
	{SIGTERM, "SIGTERM"},
#endif
#ifdef SIGTSTP
	{SIGTSTP, "SIGTSTP"},
#endif
#ifdef SIGTTIN
	{SIGTTIN, "SIGTTIN"},
#endif
#ifdef SIGTTOU
	{SIGTTOU, "SIGTTOU"},
#endif
#ifdef SIGUSR1
	{SIGUSR1, "SIGUSR1"},
#endif
#ifdef SIGUSR2
	{SIGUSR2, "SIGUSR2"},
#endif
#ifdef SIGPOLL
	{SIGPOLL, "SIGPOLL"},
#endif
#ifdef SIGSYS
	{SIGSYS, "SIGSYS"},
#endif
#ifdef SIGTRAP
	{SIGTRAP, "SIGTRAP"},
#endif
#ifdef SIGURG
	{SIGURG, "SIGURG"},
#endif
#ifdef SIGXCPU
	{SIGXCPU, "SIGXCPU"},
#endif
#ifdef SIGXFSZ
	{SIGXFSZ, "SIGXFSZ"},
#endif
#ifdef SIGIO
	{SIGIO, "SIGIO"},
#endif
#ifdef SIGSTKFLT
	{SIGSTKFLT, "SIGSTKFLT"},
#endif
#ifdef SIGWINCH
	{SIGWINCH, "SIGWINCH"},
#endif
};

/**
 * @brief copyString
 * @param str
 * @return
 */
char *copyString(const QByteArray &str) {
	char *p = new char[str.length() + 1];
	std::strcpy(p, str.constData());
	return p;
}

}

/**
 * @brief Unix::exceptions
 * @return
 */
QMap<qlonglong, QString> Unix::exceptions() {

	QMap<qlonglong, QString> exceptions;
	for (Exception e : Exceptions) {
		exceptions.insert(e.value, e.name);
	}
	return exceptions;
}

/**
 * @brief Unix::exception_name
 * @param value
 * @return
 */
QString Unix::exception_name(qlonglong value) {
	auto it = std::find_if(std::begin(Exceptions), std::end(Exceptions), [value](const Exception &ex) {
		return ex.value == value;
	});

	if (it != std::end(Exceptions)) {
		return it->name;
	}

	return QString();
}

/**
 * @brief Unix::exception_value
 * @param name
 * @return
 */
qlonglong Unix::exception_value(const QString &name) {
	auto it = std::find_if(std::begin(Exceptions), std::end(Exceptions), [&name](const Exception &ex) {
		return ex.name == name;
	});

	if (it != std::end(Exceptions)) {
		return it->value;
	}

	return -1;
}

/**
 * @brief Unix::execute_process
 * @param path
 * @param cwd
 * @param args
 * @return
 */
Status Unix::execute_process(const QString &path, const QString &cwd, const QList<QByteArray> &args) {

	QString errorString = "internal error";

	// change to the desired working directory
	if (::chdir(qPrintable(cwd)) == 0) {

		// allocate space for all the arguments
		auto argv_pointers = new char *[args.count() + 2];

		char **p = argv_pointers;

		*p++ = copyString(path.toLocal8Bit());

		for (int i = 0; i < args.count(); ++i) {
			*p++ = copyString(args[i]);
		}

		*p = nullptr;

		// NOTE: it's a bad idea to use execvp and similar functions searching in
		// $PATH. At least on Linux, if the file is corrupted/unsupported, they
		// instead appear to launch shell
		const int ret = execv(argv_pointers[0], argv_pointers);

		// should be no need to cleanup, the process which allocated all that
		// space no longer exists!
		// if we get here...execv failed!
		if (ret == -1) {
			errorString = QString("execv() failed: %1").arg(strerror(errno));
			p           = argv_pointers;
			while (*p) {
				delete[] * p++;
			}
			delete[] argv_pointers;
		}
	}

	// frankly, any return is technically an error I think
	// this is only executed from a fork
	return Status(errorString);
}

}

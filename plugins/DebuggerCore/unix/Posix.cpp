
#include "Posix.h"
#include <cerrno>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <QProcess>

#ifdef Q_OS_LINUX
#include <linux/version.h>
// being very conservative for now, technically this could be
// as low as 2.6.22
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
#define USE_SIGTIMEDWAIT
#endif
#endif

namespace DebuggerCorePlugin {

namespace {

#if !defined(USE_SIGTIMEDWAIT)
int selfpipe[2];
struct sigaction old_action;

/**
 * @brief sigchld_handler
 * @param sig
 * @param info
 * @param p
 */
void sigchld_handler(int sig, siginfo_t *info, void *p) {

	if (sig == SIGCHLD) {
		Posix::write(selfpipe[1], " ", sizeof(char));
	}

	// load as volatile
	volatile struct sigaction *vsa = &old_action;

	if (old_action.sa_flags & SA_SIGINFO) {
		using old_action_type       = void (*)(int, siginfo_t *, void *);
		old_action_type prev_action = vsa->sa_sigaction;

		if (prev_action) {
			prev_action(sig, info, p);
		}
	} else {
		using old_action_type       = void (*)(int);
		old_action_type prev_action = vsa->sa_handler;

		if (prev_action && prev_action != SIG_IGN) {
			prev_action(sig);
		}
	}
}

struct timeval duration_to_timeval(std::chrono::milliseconds msecs) {
	struct timeval tv;
	tv.tv_sec  = (msecs.count() / 1000);
	tv.tv_usec = (msecs.count() % 1000) * 1000;
	return tv;
}
#else
timespec duration_to_timespec(std::chrono::milliseconds msecs) {
	struct timespec ts;
	ts.tv_sec  = (msecs.count() / 1000);
	ts.tv_nsec = (msecs.count() % 1000) * 1000000;
	return ts;
}
#endif
}

namespace detail {
namespace {
/**
 * @brief sigtimedwait
 * @param set
 * @param info
 * @param timeout
 * @return
 */
int sigtimedwait(const sigset_t *set, siginfo_t *info, const struct timespec *timeout) {
	int ret;
	do {
		ret = ::sigtimedwait(set, info, timeout);
	} while (ret == -1 && errno == EINTR);
	return ret;
}

}
}

/**
 * @brief Posix::read
 * @param fd
 * @param buf
 * @param count
 * @return
 */
ssize_t Posix::read(int fd, void *buf, size_t count) {
	ssize_t ret;
	do {
		ret = ::read(fd, buf, count);
	} while (ret == -1 && errno == EINTR);
	return ret;
}

/**
 * @brief Posix::write
 * @param fd
 * @param buf
 * @param count
 * @return
 */
ssize_t Posix::write(int fd, const void *buf, size_t count) {
	ssize_t ret;
	do {
		ret = ::write(fd, buf, count);
	} while (ret == -1 && errno == EINTR);
	return ret;
}

/**
 * @brief Posix::select
 * @param nfds
 * @param readfds
 * @param writefds
 * @param exceptfds
 * @param timeout
 * @return
 */
int Posix::select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
	int ret;
	do {
		ret = ::select(nfds, readfds, writefds, exceptfds, timeout);
	} while (ret == -1 && errno == EINTR);
	return ret;
}

/**
 * @brief Posix::waitpid
 * @param pid
 * @param status
 * @param options
 * @return
 */
pid_t Posix::waitpid(pid_t pid, int *status, int options) {
	pid_t ret;
	do {
		ret = ::waitpid(pid, status, options);
	} while (ret == -1 && errno == EINTR);
	return ret;
}

#if !defined(USE_SIGTIMEDWAIT)
/**
 * similar to select but has the timeout specified as a quantity of msecs
 *
 * @brief Posix::select_ex
 * @param nfds
 * @param readfds
 * @param writefds
 * @param exceptfds
 * @param msecs - 0 means wait forever.
 * @return
 */
int Posix::select_ex(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, std::chrono::milliseconds msecs) {
	if (msecs.count() != 0) {
		struct timeval tv = duration_to_timeval(msecs);
		return Posix::select(nfds, readfds, writefds, exceptfds, &tv);
	} else {
		return Posix::select(nfds, readfds, writefds, exceptfds, nullptr);
	}
}
#endif

/**
 * @brief Posix::wait_for_sigchld
 * @param msecs
 * @return
 */
bool Posix::wait_for_sigchld(std::chrono::milliseconds msecs) {
#if !defined(USE_SIGTIMEDWAIT)
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(selfpipe[0], &rfds);

	if (Posix::select_ex(selfpipe[0] + 1, &rfds, nullptr, nullptr, msecs) == 0) {
		return true;
	}

	char ch;
	if (Posix::read(selfpipe[0], &ch, sizeof(char)) == -1) {
		return true;
	}

	return false;
#else
	sigset_t mask;
	siginfo_t info;
	struct timespec ts = duration_to_timespec(msecs);
	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);

	return detail::sigtimedwait(&mask, &info, &ts) == SIGCHLD;
#endif
}

/**
 * @brief Posix::initialize
 */
void Posix::initialize() {
#if !defined(USE_SIGTIMEDWAIT)

	// HACK(eteran): so, the first time we create a QProcess, it will hook SIGCHLD
	//               unfortunately, in Qt5 it doesn't seem to call our handler
	//               so we do this to force it to hook BEFORE we do, letting us
	//               get the first crack at the signal, then we call the one that
	//               Qt installed.
	auto p = new QProcess(nullptr);
	p->start("/bin/true");

	// create a pipe and make it non-blocking
	int r = ::pipe(selfpipe);
	Q_UNUSED(r)

	::fcntl(selfpipe[0], F_SETFL, ::fcntl(selfpipe[0], F_GETFL) | O_NONBLOCK);
	::fcntl(selfpipe[1], F_SETFL, ::fcntl(selfpipe[1], F_GETFL) | O_NONBLOCK);

	// setup a signal handler
	struct sigaction new_action = {};

	new_action.sa_sigaction = sigchld_handler;
	new_action.sa_flags     = SA_RESTART | SA_SIGINFO;
	sigemptyset(&new_action.sa_mask);

	sigaction(SIGCHLD, &new_action, &old_action);
#else
	// TODO(eteran): the man pages mention blocking the signal we want to catch
	//               but I'm not sure if it is necessary for this use case...
#endif
}

}

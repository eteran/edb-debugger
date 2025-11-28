/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "Posix.h"
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <QProcess>

namespace DebuggerCorePlugin {

namespace {

timespec duration_to_timespec(std::chrono::milliseconds msecs) {
	struct timespec ts;
	ts.tv_sec  = (msecs.count() / 1000);
	ts.tv_nsec = (msecs.count() % 1000) * 1000000;
	return ts;
}

}

namespace detail {

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

/**
 * @brief Posix::wait_for_sigchld
 * @param msecs
 * @return
 */
bool Posix::wait_for_sigchld(std::chrono::milliseconds msecs) {

	sigset_t mask;
	siginfo_t info;
	struct timespec ts = duration_to_timespec(msecs);
	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);

	return detail::sigtimedwait(&mask, &info, &ts) == SIGCHLD;
}

}

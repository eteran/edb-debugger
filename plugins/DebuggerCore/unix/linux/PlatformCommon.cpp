/*
Copyright (C) 2015 - 2015 Evan Teran
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

#include "PlatformCommon.h"
#include <fstream>
#include <iostream>
#include <linux/limits.h>
#include <sys/wait.h>

namespace DebuggerCorePlugin {

/**
 * @brief resume_code
 * @param status
 * @return
 */
int resume_code(int status) {

	if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP) {
		return 0;
	}

	if (WIFSIGNALED(status)) {
		return WTERMSIG(status);
	}

	if (WIFSTOPPED(status)) {
		return WSTOPSIG(status);
	}

	return 0;
}

/**
 * gets the contents of /proc/<pid>/stat
 *
 * @brief get_user_stat
 * @param path
 * @param user_stat
 * @return the number of elements successfully parsed
 */
int get_user_stat(const char *path, struct user_stat *user_stat) {
	Q_ASSERT(user_stat);

	std::ifstream stream(path);
	std::string line;
	if (std::getline(stream, line)) {
		// the comm field is wrapped with "(" and ")", so we look for the closing one
		size_t left  = line.find_first_of('(');
		size_t right = line.find_last_of(')');

		if (right == std::string::npos || left == std::string::npos) {
			return -1;
		}

		int r = sscanf(&line[right + 2], "%c %d %d %d %d %d %u %llu %llu %llu %llu %llu %llu %lld %lld %lld %lld %lld %lld %llu %llu %lld %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %d %d %u %u %llu %llu %lld %llu %llu %llu %llu %llu %llu %llu %d",
					   &user_stat->state,
					   &user_stat->ppid,
					   &user_stat->pgrp,
					   &user_stat->session,
					   &user_stat->tty_nr,
					   &user_stat->tpgid,
					   &user_stat->flags,
					   &user_stat->minflt,
					   &user_stat->cminflt,
					   &user_stat->majflt,
					   &user_stat->cmajflt,
					   &user_stat->utime,
					   &user_stat->stime,
					   &user_stat->cutime,
					   &user_stat->cstime,
					   &user_stat->priority,
					   &user_stat->nice,
					   &user_stat->num_threads,
					   &user_stat->itrealvalue,
					   &user_stat->starttime,
					   &user_stat->vsize,
					   &user_stat->rss,
					   &user_stat->rsslim,
					   &user_stat->startcode,
					   &user_stat->endcode,
					   &user_stat->startstack,
					   &user_stat->kstkesp,
					   &user_stat->kstkeip,
					   &user_stat->signal,
					   &user_stat->blocked,
					   &user_stat->sigignore,
					   &user_stat->sigcatch,
					   &user_stat->wchan,
					   &user_stat->nswap,
					   &user_stat->cnswap,
					   &user_stat->exit_signal,
					   &user_stat->processor,
					   &user_stat->rt_priority,
					   &user_stat->policy,

					   // Linux 2.6.18
					   &user_stat->delayacct_blkio_ticks,

					   // Linux 2.6.24
					   &user_stat->guest_time,
					   &user_stat->cguest_time,

					   // Linux 3.3
					   &user_stat->start_data,
					   &user_stat->end_data,
					   &user_stat->start_brk,

					   // Linux 3.5
					   &user_stat->arg_start,
					   &user_stat->arg_end,
					   &user_stat->env_start,
					   &user_stat->env_end,
					   &user_stat->exit_code);

		// fill in the pid
		r += sscanf(&line[0], "%d", &user_stat->pid);

		// fill in the comm field
		const size_t len = std::min(sizeof(user_stat->comm), (right - left) - 1);
		line.copy(user_stat->comm, len, left + 1);
		user_stat->comm[len] = '\0';
		++r;

		return r;
	}

	return -1;
}

/**
 * gets the contents of /proc/<pid>/stat
 *
 * @brief get_user_stat
 * @param pid
 * @param user_stat
 * @return the number of elements or -1 on error
 */
int get_user_stat(edb::pid_t pid, struct user_stat *user_stat) {
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "/proc/%d/stat", pid);
	return get_user_stat(path, user_stat);
}

/**
 * gets the contents of /proc/<pid>/task/<tid>/stat
 *
 * @brief get_user_task_stat
 * @param pid
 * @param tid
 * @param user_stat
 * @return the number of elements or -1 on error
 */
int get_user_task_stat(edb::pid_t pid, edb::tid_t tid, struct user_stat *user_stat) {
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "/proc/%d/task/%d/stat", pid, tid);
	return get_user_stat(path, user_stat);
}

}

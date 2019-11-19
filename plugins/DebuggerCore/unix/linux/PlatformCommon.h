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

#ifndef PLATFORM_COMMON_H_20151011_
#define PLATFORM_COMMON_H_20151011_

#include "OSTypes.h"
#include "edb.h"

class QString;

namespace DebuggerCorePlugin {

struct user_stat {
	/* 01 */ int pid;
	/* 02 */ char comm[256];
	/* 03 */ char state;
	/* 04 */ int ppid;
	/* 05 */ int pgrp;
	/* 06 */ int session;
	/* 07 */ int tty_nr;
	/* 08 */ int tpgid;
	/* 09 */ unsigned flags;
	/* 10 */ unsigned long long minflt;
	/* 11 */ unsigned long long cminflt;
	/* 12 */ unsigned long long majflt;
	/* 13 */ unsigned long long cmajflt;
	/* 14 */ unsigned long long utime;
	/* 15 */ unsigned long long stime;
	/* 16 */ long long cutime;
	/* 17 */ long long cstime;
	/* 18 */ long long priority;
	/* 19 */ long long nice;
	/* 20 */ long long num_threads;
	/* 21 */ long long itrealvalue;
	/* 22 */ unsigned long long starttime;
	/* 23 */ unsigned long long vsize;
	/* 24 */ long long rss;
	/* 25 */ unsigned long long rsslim;
	/* 26 */ unsigned long long startcode;
	/* 27 */ unsigned long long endcode;
	/* 28 */ unsigned long long startstack;
	/* 29 */ unsigned long long kstkesp;
	/* 30 */ unsigned long long kstkeip;
	/* 31 */ unsigned long long signal;
	/* 32 */ unsigned long long blocked;
	/* 33 */ unsigned long long sigignore;
	/* 34 */ unsigned long long sigcatch;
	/* 35 */ unsigned long long wchan;
	/* 36 */ unsigned long long nswap;
	/* 37 */ unsigned long long cnswap;
	/* 38 */ int exit_signal;
	/* 39 */ int processor;
	/* 40 */ unsigned rt_priority;
	/* 41 */ unsigned policy;

	// Linux 2.6.18
	/* 42 */ unsigned long long delayacct_blkio_ticks;

	// Linux 2.6.24
	/* 43 */ unsigned long long guest_time;
	/* 44 */ long long cguest_time;

	// Linux 3.3
	/* 45 */ unsigned long long start_data;
	/* 46 */ unsigned long long end_data;
	/* 47 */ unsigned long long start_brk;

	// Linux 3.5
	/* 48 */ unsigned long long arg_start;
	/* 49 */ unsigned long long arg_end;
	/* 50 */ unsigned long long env_start;
	/* 51 */ unsigned long long env_end;
	/* 52 */ int exit_code;
};

int get_user_stat(const char *path, struct user_stat *user_stat);
int get_user_stat(edb::pid_t pid, struct user_stat *user_stat);
int get_user_task_stat(edb::pid_t pid, edb::tid_t tid, struct user_stat *user_stat);
int resume_code(int status);

}

#endif

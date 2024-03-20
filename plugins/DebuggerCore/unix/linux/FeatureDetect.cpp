/*
Copyright (C) 2016 - 2023 Evan Teran
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

#include "FeatureDetect.h"
#include "PlatformFile.h"
#include "edb.h"

#include <cstdint>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <linux/limits.h>
#include <string>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace DebuggerCorePlugin::feature {

namespace {

/**
 * @brief kill_child
 * @param pid
 */
void kill_child(int pid) {
	if (kill(pid, SIGKILL) == -1) {
		perror("failed to kill child");
	}
}

}

/**
 * detects whether or not reads/writes through /proc/<pid>/mem work correctly
 *
 * @brief detect_proc_access
 * @param read_broken
 * @param write_broken
 * @return
 */
bool detect_proc_access(bool *read_broken, bool *write_broken) {

	switch (pid_t pid = fork()) {
	case 0:
		if (ptrace(PTRACE_TRACEME, 0, 0, 0) < 0) {
			perror("child: PTRACE_TRACEME failed");
			abort();
		}

		// force a signal
		raise(SIGCONT);

		for (;;) {
			sleep(10);
		}
		abort();

	case -1:
		perror("fork");
		return false;

	default: {
		int status;
		if (waitpid(pid, &status, __WALL) == -1) {
			perror("parent: waitpid failed");
			kill_child(pid);
			return false;
		}

		if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGCONT) {
			std::cerr << "unexpected status returned by waitpid: 0x" << std::hex << status << "\n";
			kill_child(pid);
			return false;
		}

		char path[PATH_MAX];
		snprintf(path, sizeof(path), "/proc/%d/mem", pid);

		PlatformFile file(path);
		if (!file) {
			perror("failed to open memory file");
			kill_child(pid);
			return false;
		}

		const auto pageAlignMask = ~(sysconf(_SC_PAGESIZE) - 1);
		const auto addr          = reinterpret_cast<uintptr_t>(&edb::v1::debugger_ui) & pageAlignMask;

		int buf = 0x12345678;

		if (file.readAt(&buf, sizeof(buf), addr) == -1) {
			*read_broken  = true;
			*write_broken = true;
			kill_child(pid);
			return false;
		}

		if (file.writeAt(&buf, sizeof(buf), addr) == -1) {
			*read_broken  = false;
			*write_broken = true;
		} else {
			*read_broken  = false;
			*write_broken = false;
		}

		kill_child(pid);
		return true;
	}
	}
}

}

/*
Copyright (C) 2016 - 2016 Evan Teran
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
#include "version.h"

#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <string>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace DebuggerCorePlugin {
namespace feature {
namespace {

// Custom class to work with files, since various wrappers
// appear to be unreliable to check whether writes succeeded
class File {
public:
	explicit File(const std::string &filename) {
		fd_      = ::open(filename.c_str(), O_RDWR);
		success_ = fd_ != -1;
	}

	ssize_t write(const void *buf, size_t count) {
		const ssize_t result = ::write(fd_, buf, count);
		success_             = result != -1;
		return result;
	}

	ssize_t read(void *buf, size_t count) {
		const ssize_t result = ::read(fd_, buf, count);
		success_             = result != -1;
		return result;
	}

	off_t seekp(size_t offset) {
		const off_t result = ::lseek(fd_, offset, SEEK_SET);
		success_           = result != -1;
		return result;
	}

	~File() {
		close(fd_);
	}

	explicit operator bool() {
		return success_;
	}

private:
	int fd_       = -1;
	bool success_ = false;
};

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

		File file("/proc/" + std::to_string(pid) + "/mem");
		if (!file) {
			perror("failed to open memory file");
			kill_child(pid);
			return false;
		}

		const auto pageAlignMask = ~(sysconf(_SC_PAGESIZE) - 1);
		const auto addr          = reinterpret_cast<uintptr_t>(&edb::version) & pageAlignMask;
		file.seekp(addr);
		if (!file) {
			perror("failed to seek to address to read");
			kill_child(pid);
			return false;
		}

		int buf = 0x12345678;
		{
			file.read(&buf, sizeof(buf));
			if (!file) {
				*read_broken  = true;
				*write_broken = true;
				kill_child(pid);
				return false;
			}
		}

		file.seekp(addr);
		if (!file) {
			perror("failed to seek to address to write");
			kill_child(pid);
			return false;
		}

		{
			file.write(&buf, sizeof(buf));
			if (!file) {
				*read_broken  = false;
				*write_broken = true;
			} else {
				*read_broken  = false;
				*write_broken = false;
			}
		}
		kill_child(pid);
		return true;
	}
	}
}

}
}

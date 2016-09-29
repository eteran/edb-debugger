
#include "FeatureDetect.h"

#include <iostream>
#include <iomanip>
#include <string>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

namespace feature {
namespace {

// Custom class to work with files, since various wrappers
// appear to be unreliable to check whether writes succeeded
class File {
	int fd;
	bool success;

public:
	File(const std::string &filename) {
		fd = ::open(filename.c_str(), O_RDWR);
		success = fd != -1;
	}

	ssize_t write(const void *buf, size_t count) {
		const auto result = ::write(fd, buf, count);
		success = result != -1;
		return result;
	}

	ssize_t read(void *buf, size_t count) {
		const auto result = ::read(fd, buf, count);
		success = result != -1;
		return result;
	}

	size_t seekp(size_t offset) {
		const auto result = ::lseek(fd, offset, SEEK_SET);
		success = result != -1;
		return result;
	}

	~File() {
		close(fd);
	}

	explicit operator bool() {
		return success;
	}
};

void kill_child(int pid) {
	if (kill(pid, SIGKILL) == -1) {
		perror("failed to kill child");
	}
}

}

//------------------------------------------------------------------------------
// Name: detect_proc_access
// Desc: detects whether or not reads/writes through /proc/<pid>/mem work
//       correctly
//------------------------------------------------------------------------------
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
		const auto addr = reinterpret_cast<uintptr_t>(&detect_proc_access) & pageAlignMask;
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

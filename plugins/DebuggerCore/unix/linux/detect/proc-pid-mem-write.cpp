
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

enum Method {
	Detected,
	UserSet,
	TestFailed
};

// Custom class to work with files, since various wrappers
// appear to be unreliable to check whether writes succeeded
class File {
	int fd;
	bool success;

public:
	File(const std::string &filename) {
		fd = open(filename.c_str(), O_RDWR);
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

static const char *headerName = nullptr;

bool headerUpToDate(std::string const &line) {
	std::ifstream file(headerName);
	if (!file)
		return false;
	// Try to read one char more than the line has to check that actual size of file is correct
	std::string fileStr(line.length() + 1, 0);
	if (file.readsome(&fileStr[0], fileStr.length()) != signed(line.length()))
		return false;
	fileStr.resize(line.length());
	return fileStr == line;
}

void writeHeader(bool read_broken, bool write_broken, Method method) {
	std::ostringstream line;
	
	line << "#define PROC_PID_MEM_WRITE_BROKEN " << std::boolalpha << write_broken;
	
	switch(method) {
	case Detected:
		line << " /* Method = Detected */";
		break;
	case UserSet:
		line << " /* Method = User Set */";
		break;
	case TestFailed:
		line << " /* Method = Failed To Test */";
		break;
	}
	
	line << "\n";
	
	
	line << "#define PROC_PID_MEM_READ_BROKEN " << std::boolalpha << read_broken;
	
	switch(method) {
	case Detected:
		line << " /* Method = Detected */";
		break;
	case UserSet:
		line << " /* Method = User Set */";
		break;
	case TestFailed:
		line << " /* Method = Failed To Test */";
		break;
	}
	
	line << "\n";


	if (headerUpToDate(line.str())) {
		return;
	}
	
	std::ofstream file(headerName);
	file << line.str();
}

void killChild(int pid) {
	if (kill(pid, SIGKILL) == -1) {
		perror("failed to kill child");
	}
}

bool detectAndWriteHeader() {
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
			killChild(pid);
			return false;
		}
		
		if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGCONT) {
			std::cerr << "unexpected status returned by waitpid: 0x" << std::hex << status << "\n";
			killChild(pid);
			return false;
		}

		File file("/proc/" + std::to_string(pid) + "/mem");
		if (!file) {
			perror("failed to open memory file");
			killChild(pid);
			return false;
		}

		const auto pageAlignMask = ~(sysconf(_SC_PAGESIZE) - 1);
		const auto addr = reinterpret_cast<uintptr_t>(&detectAndWriteHeader) & pageAlignMask;
		file.seekp(addr);
		if (!file) {
			perror("failed to seek to address to read");
			killChild(pid);
			return false;
		}

		int buf = 0x12345678;
		{
			file.read(&buf, sizeof(buf));
			if (!file) {
				writeHeader(true, true, Detected);
				killChild(pid);
				return false;
			}
		}

		file.seekp(addr);
		if (!file) {
			perror("failed to seek to address to write");
			killChild(pid);
			return false;
		}

		{
			file.write(&buf, sizeof(buf));
			if (!file) {
				writeHeader(false, true, Detected);
			} else {
				writeHeader(false, false, Detected);
			}
		}
		killChild(pid);
		return true;
	}
	}
}

int main(int argc, char **argv) {
	
	if (argc == 3) {		
		if(strcmp(argv[2], "--assume-bad") == 0) {
			headerName = argv[1];
			writeHeader(false, true, UserSet);
			return 0;
		} else if(strcmp(argv[2], "--assume-good") == 0) {
			headerName = argv[1];
			writeHeader(false, false, UserSet);
			return 0;
		}
	}


	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " headerFileName.h [--assume-bad|--assume-good]\n";
		return 1;
	}
	
	headerName = argv[1];
	
	

	if (!detectAndWriteHeader()) {
		std::cerr << "Warning: failed to detect whether write to /proc/PID/mem is broken. Assuming it's not.\n";
		writeHeader(false, false, TestFailed);
	}
}

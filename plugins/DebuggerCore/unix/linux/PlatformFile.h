/*
 * Copyright (C) 2024 - 2024 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PLATFORM_FILE_H_20150517_
#define PLATFORM_FILE_H_20150517_

#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace DebuggerCorePlugin {

class PlatformFile {
public:
	explicit PlatformFile(const char *filename, int flags = O_RDWR) {
		fd_ = ::open(filename, flags);
	}

	~PlatformFile() {
		::close(fd_);
	}

	ssize_t writeAt(const void *buf, size_t count, off_t offset) const {
		return ::pwrite(fd_, buf, count, offset);
	}

	ssize_t readAt(void *buf, size_t count, off_t offset) const {
		return ::pread(fd_, buf, count, offset);
	}

	explicit operator bool() const {
		return fd_ != -1;
	}

private:
	int fd_ = -1;
};

}

#endif

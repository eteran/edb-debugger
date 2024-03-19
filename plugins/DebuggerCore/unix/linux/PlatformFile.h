/*
Copyright (C) 2024 - 2024 Evan Teran
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

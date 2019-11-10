
#ifndef POSIX_20181211_H_
#define POSIX_20181211_H_

#include "OSTypes.h"
#include <chrono>
#include <cstdint>

namespace DebuggerCorePlugin {
namespace Posix {

void initialize();
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
int select_ex(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, std::chrono::milliseconds msecs);
pid_t waitpid(pid_t pid, int *status, int options);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
bool wait_for_sigchld(std::chrono::milliseconds msecs);

}
}

#endif

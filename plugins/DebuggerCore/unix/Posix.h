
#ifndef POSIX_20181211_H_
#define POSIX_20181211_H_

#include "OSTypes.h"
#include <cstdint>

namespace Posix {

void initialize();
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
int select_ex(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, int64_t msecs);
pid_t waitpid(pid_t pid, int *status, int options);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
bool wait_for_sigchld(int msecs);

}

#endif


#ifndef PROCESS_INFO_20120728_H_
#define PROCESS_INFO_20120728_H_

#include "Types.h"
#include <QString>

struct ProcessInfo {
	edb::pid_t pid;
	edb::uid_t uid;
	QString    user;
	QString    name;
};

#endif


#ifndef PROCESS_20120728_H_
#define PROCESS_20120728_H_

#include "Types.h"
#include <QString>

struct Process {
	edb::pid_t pid;
	edb::uid_t uid;
	QString    user;
	QString    name;
};

#endif


#ifndef THREAD_INFO_H_
#define THREAD_INFO_H_

#include <QString>

struct ThreadInfo {
	QString        name;
	edb::tid_t     tid;
	edb::address_t ip;
	int            priority;
	QString        state;
	
};

#endif


#ifndef PLATOFORM_PROCESS_20150517_H_
#define PLATOFORM_PROCESS_20150517_H_

#include "IProcess.h"

namespace DebuggerCore {

class DebuggerCore;

class PlatformProcess : public IProcess {
public:
	PlatformProcess(DebuggerCore *core, edb::pid_t pid);
	virtual ~PlatformProcess();
	
public:
	virtual bool write_bytes(edb::address_t address, const void *buf, size_t len);
	virtual bool read_bytes(edb::address_t address, void *buf, size_t len);
	virtual bool read_pages(edb::address_t address, void *buf, size_t count);

private:
	quint8 read_byte(edb::address_t address, bool *ok);
	void write_byte(edb::address_t address, quint8 value, bool *ok);
	quint8 read_byte_base(edb::address_t address, bool *ok);

private:
	DebuggerCore* core_;
	edb::pid_t    pid_;
};

}

#endif

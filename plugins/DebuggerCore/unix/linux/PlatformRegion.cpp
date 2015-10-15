/*
Copyright (C) 2006 - 2015 Evan Teran
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

#include "PlatformRegion.h"

#include "MemoryRegions.h"
#include "edb.h"
#include "IDebugger.h"
#include "State.h"
#include "IDebugEventHandler.h"
#include <QMessageBox>
#include <sys/mman.h>
#include <asm/unistd.h>

namespace DebuggerCore {

namespace {

//------------------------------------------------------------------------------
// Name: permissions_value
// Desc:
//------------------------------------------------------------------------------
IRegion::permissions_t permissions_value(bool read, bool write, bool execute) {
	IRegion::permissions_t perms = 0;
	if(read)    perms |= PROT_READ;
	if(write)   perms |= PROT_WRITE;
	if(execute) perms |= PROT_EXEC;
	return perms;
}

}

template <size_t N>
class BackupInfo : public IDebugEventHandler {
public:
	BackupInfo(edb::address_t address, IRegion::permissions_t perms, PlatformRegion *region);

private:
	Q_DISABLE_COPY(BackupInfo)

public:
	IRegion::permissions_t perms() const { return premissions_; }
	bool locked()                        { return !lock_.testAndSetAcquire(0, 1); }

public:
	bool backup();
	bool restore();

public:
	virtual edb::EVENT_STATUS handle_event(const IDebugEvent::const_pointer &event);

public:
	QAtomicInt             lock_;
	edb::address_t         address_;
	IRegion::permissions_t premissions_;
	State                  state_;
	quint8                 buffer_[N];
	PlatformRegion *const  region_;

public:
	IDebugEventHandler *event_handler_;
};

//------------------------------------------------------------------------------
// Name: BackupInfo
// Desc:
//------------------------------------------------------------------------------
template <size_t N>
BackupInfo<N>::BackupInfo(edb::address_t address, IRegion::permissions_t perms, PlatformRegion *region) :
		lock_(1), address_(address), premissions_(perms), region_(region), event_handler_(0) {
}

//------------------------------------------------------------------------------
// Name: backup
// Desc:
//------------------------------------------------------------------------------
template <size_t N>
bool BackupInfo<N>::backup() {

	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(IThread::pointer thread = process->current_thread()) {
			thread->get_state(&state_);
		}
		return process->read_bytes(address_, buffer_, N);
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: restore
// Desc:
//------------------------------------------------------------------------------
template <size_t N>
bool BackupInfo<N>::restore() {

	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(IThread::pointer thread = process->current_thread()) {
			thread->set_state(state_);
		}

		return process->write_bytes(address_, buffer_, N);
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: handle_event
// Desc:
//------------------------------------------------------------------------------
template <size_t N>
edb::EVENT_STATUS BackupInfo<N>::handle_event(const IDebugEvent::const_pointer &event) {
	Q_UNUSED(event);

	lock_.testAndSetRelease(1, 0);

	// restore the original code and register state
	restore();

	// update permissions mask
	region_->permissions_ = perms();

	// restore the event handler
	edb::v1::set_debug_event_handler(event_handler_);

	// really shouldn't matter since the return value isn't used at all
	// we simply want tot catch the event and set the lock to 0
	return edb::DEBUG_STOP;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
PlatformRegion::PlatformRegion(edb::address_t start, edb::address_t end, edb::address_t base, const QString &name, permissions_t permissions) : start_(start), end_(end), base_(base), name_(name), permissions_(permissions) {
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
PlatformRegion::~PlatformRegion() {
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
IRegion *PlatformRegion::clone() const {
	return new PlatformRegion(start_, end_, base_, name_, permissions_);
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
bool PlatformRegion::accessible() const {
	return readable() || writable() || executable();
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
bool PlatformRegion::readable() const {
	return (permissions_ & PROT_READ) != 0;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
bool PlatformRegion::writable() const {
	return (permissions_ & PROT_WRITE) != 0;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
bool PlatformRegion::executable() const {
	return (permissions_ & PROT_EXEC) != 0;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
edb::address_t PlatformRegion::size() const {
	return end_ - start_;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
void PlatformRegion::set_permissions(bool read, bool write, bool execute) {
	edb::address_t temp_address        = 0;
	int count                          = 0;
	int ret                            = QMessageBox::Yes;
	const QList<IRegion::pointer> &regions = edb::v1::memory_regions().regions();

	// search for an executable region to run our shell code
	for(const IRegion::pointer &region: regions) {
		if(region->executable()) {
			if(temp_address == 0) {
				temp_address = region->start();
			}

			if(++count > 1) {
				break;
			}
		}
	}

	if(executable() && count == 1 && !execute) {
		ret = QMessageBox::question(0,
			tr("Removing Execute Permissions On Last Executable IRegion::pointer"),
			tr("You are about to remove execute permissions from the last executable region. Because of the need "
			"to run code in the process to change permissions, there will be no way to undo this. In addition, "
			"the process will no longer be able to run as it will have no execute permissions in any regions. "
			"Odds are this is not what you want to do."
			"Are you sure you want to remove execute permissions from this region?"),
			QMessageBox::Yes, QMessageBox::No);
	}

	if(ret == QMessageBox::Yes) {
		if(temp_address != 0) {
			set_permissions(read, write, execute, temp_address);
		} else {
			QMessageBox::information(
				0,
				tr("No Suitable Address Found"),
				tr("This feature relies on running shellcode in the debugged process, no executable memory region was found. Unfortunately, this means that no more region permission changes can be made (it also means that there is nothing the process can continue to do since it cannot execute at all)."));
		}
	}
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
edb::address_t PlatformRegion::start() const {
	return start_;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
edb::address_t PlatformRegion::end() const {
	return end_;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
edb::address_t PlatformRegion::base() const {
	return base_;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString PlatformRegion::name() const {
	return name_;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
IRegion::permissions_t PlatformRegion::permissions() const {
	return permissions_;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
void PlatformRegion::set_permissions(bool read, bool write, bool execute, edb::address_t temp_address) {
	const permissions_t perms       = permissions_value(read, write, execute);
	const edb::address_t len        = size();
	const edb::address_t addr       = start();

	// I wish there was a clean way to get the value of this system call for either target
	// but nothing obvious comes to mind. We may have to do something crazy
	// with macros, but for now, we just hard code it :-/
	const edb::address_t syscallnum = edb::v1::debuggeeIs32Bit() ? 125 : 10; //__NR_mprotect;

	// start of nowhere near portable code
	const quint8 shellcode32[] = {
		"\xcd\x80" // int $0x80
		"\xf4"     // hlt
	};

	const quint8 shellcode64[] = {
		"\x0f\x05" // syscall
		"\xf4"     // hlt
	};

	quint8 shellcode[3];


	if(edb::v1::debuggeeIs32Bit()) {
		memcpy(shellcode, shellcode32, sizeof(shellcode));
	} else {
		memcpy(shellcode, shellcode64, sizeof(shellcode));
	}

	// end nowhere near portable code
	typedef BackupInfo<sizeof(shellcode)> BI;
	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(IThread::pointer thread = process->current_thread()) {
			try {
				BI backup_info(temp_address, perms, this);

				if(backup_info.backup()) {
					// write out our shellcode
					if(process->write_bytes(temp_address, shellcode, sizeof(shellcode))) {

						State state;
						thread->get_state(&state);
						state.set_instruction_pointer(temp_address);

						if(edb::v1::debuggeeIs32Bit()) {
							state.set_register("ecx", len);
							state.set_register("ebx", addr);
							state.set_register("edx", perms);
							state.set_register("eax", syscallnum);
						} else {
							state.set_register("rsi", len);
							state.set_register("rdi", addr);
							state.set_register("rdx", perms);
							state.set_register("rax", syscallnum);
						}

						thread->set_state(state);
						backup_info.event_handler_ = edb::v1::set_debug_event_handler(&backup_info);

						// run the system call instruction and wait for the trap
						thread->step(edb::DEBUG_CONTINUE);

						// we use a spinlock here because we want to be able to
						// process events while waiting
						while(backup_info.locked()) {
							QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
						}
					}
				}
			} catch(const std::bad_alloc &) {
				QMessageBox::information(
					0,
					tr("Memory Allocation Error"),
					tr("Unable to satisfy memory allocation request for backup code."));
			}
		}
	}
}

void PlatformRegion::set_start(edb::address_t address) {
	start_ = address;
}

void PlatformRegion::set_end(edb::address_t address) {
	end_ = address;
}

}

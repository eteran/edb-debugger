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
#include "IDebugEventHandler.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "IThread.h"
#include "MemoryRegions.h"
#include "State.h"
#include "edb.h"

#include <QMessageBox>

#include <asm/unistd.h>
#include <sys/mman.h>

namespace DebuggerCorePlugin {

namespace {

/**
 * @brief permissions_value
 * @param read
 * @param write
 * @param execute
 * @return
 */
IRegion::permissions_t permissions_value(bool read, bool write, bool execute) {
	IRegion::permissions_t perms = 0;
	if (read) perms |= PROT_READ;
	if (write) perms |= PROT_WRITE;
	if (execute) perms |= PROT_EXEC;
	return perms;
}

}

template <size_t N>
class BackupInfo : public IDebugEventHandler {
public:
	BackupInfo(edb::address_t address, IRegion::permissions_t perms, PlatformRegion *region);
	~BackupInfo() override;
	BackupInfo(const BackupInfo &) = delete;
	BackupInfo &operator=(const BackupInfo &) = delete;

public:
	IRegion::permissions_t perms() const { return premissions_; }
	bool locked() { return !lock_.testAndSetAcquire(0, 1); }

public:
	bool backup();
	bool restore();

public:
	edb::EventStatus handleEvent(const std::shared_ptr<IDebugEvent> &event) override;

private:
	QAtomicInt lock_ = 1;
	edb::address_t address_;
	IRegion::permissions_t premissions_;
	State state_;
	uint8_t buffer_[N];
	PlatformRegion *const region_;
};

/**
 * @brief BackupInfo<N>::BackupInfo
 * @param address
 * @param perms
 * @param region
 */
template <size_t N>
BackupInfo<N>::BackupInfo(edb::address_t address, IRegion::permissions_t perms, PlatformRegion *region)
	: address_(address), premissions_(perms), region_(region) {
	edb::v1::add_debug_event_handler(this);
}

/**
 * @brief BackupInfo<N>::~BackupInfo
 */
template <size_t N>
BackupInfo<N>::~BackupInfo() {
	edb::v1::remove_debug_event_handler(this);
}

/**
 * @brief BackupInfo<N>::backup
 * @return
 */
template <size_t N>
bool BackupInfo<N>::backup() {

	if (IProcess *process = edb::v1::debugger_core->process()) {
		if (std::shared_ptr<IThread> thread = process->currentThread()) {
			thread->getState(&state_);
		}
		return process->readBytes(address_, buffer_, N);
	}

	return false;
}

/**
 * @brief BackupInfo<N>::restore
 * @return
 */
template <size_t N>
bool BackupInfo<N>::restore() {

	if (IProcess *process = edb::v1::debugger_core->process()) {
		if (std::shared_ptr<IThread> thread = process->currentThread()) {
			thread->setState(state_);
		}

		return process->writeBytes(address_, buffer_, N);
	}

	return false;
}

/**
 * @brief BackupInfo<N>::handleEvent
 * @param event
 * @return
 */
template <size_t N>
edb::EventStatus BackupInfo<N>::handleEvent(const std::shared_ptr<IDebugEvent> &event) {
	Q_UNUSED(event)

	lock_.testAndSetRelease(1, 0);

	// restore the original code and register state
	restore();

	// update permissions mask
	region_->permissions_ = perms();

	// really shouldn't matter since the return value isn't used at all
	// we simply want tot catch the event and set the lock to 0
	return edb::DEBUG_STOP;
}

/**
 * @brief PlatformRegion::PlatformRegion
 * @param start
 * @param end
 * @param base
 * @param name
 * @param permissions
 */
PlatformRegion::PlatformRegion(edb::address_t start, edb::address_t end, edb::address_t base, const QString &name, permissions_t permissions)
	: start_(start), end_(end), base_(base), name_(name), permissions_(permissions) {
}

/**
 * @brief PlatformRegion::clone
 * @return
 */
IRegion *PlatformRegion::clone() const {
	return new PlatformRegion(start_, end_, base_, name_, permissions_);
}

/**
 * @brief PlatformRegion::accessible
 * @return
 */
bool PlatformRegion::accessible() const {
	return readable() || writable() || executable();
}

/**
 * @brief PlatformRegion::readable
 * @return
 */
bool PlatformRegion::readable() const {
	return (permissions_ & PROT_READ) != 0;
}

/**
 * @brief PlatformRegion::writable
 * @return
 */
bool PlatformRegion::writable() const {
	return (permissions_ & PROT_WRITE) != 0;
}

/**
 * @brief PlatformRegion::executable
 * @return
 */
bool PlatformRegion::executable() const {
	return (permissions_ & PROT_EXEC) != 0;
}

/**
 * @brief PlatformRegion::size
 * @return
 */
size_t PlatformRegion::size() const {
	return end_ - start_;
}
/**
 * @brief PlatformRegion::setPermissions
 * @param read
 * @param write
 * @param execute
 */
void PlatformRegion::setPermissions(bool read, bool write, bool execute) {
	edb::address_t temp_address                    = 0;
	int count                                      = 0;
	int ret                                        = QMessageBox::Yes;
	const QList<std::shared_ptr<IRegion>> &regions = edb::v1::memory_regions().regions();

	// search for an executable region to run our shell code
	for (const std::shared_ptr<IRegion> &region : regions) {
		if (region->executable()) {
			if (temp_address == 0) {
				temp_address = region->start();
			}

			if (++count > 1) {
				break;
			}
		}
	}

	if (executable() && count == 1 && !execute) {
		ret = QMessageBox::question(nullptr,
									tr("Removing Execute Permissions On Last Executable std::shared_ptr<IRegion>"),
									tr("You are about to remove execute permissions from the last executable region. Because of the need "
									   "to run code in the process to change permissions, there will be no way to undo this. In addition, "
									   "the process will no longer be able to run as it will have no execute permissions in any regions. "
									   "Odds are this is not what you want to do."
									   "Are you sure you want to remove execute permissions from this region?"),
									QMessageBox::Yes, QMessageBox::No);
	}

	if (ret == QMessageBox::Yes) {
		if (temp_address != 0) {
			setPermissions(read, write, execute, temp_address);
		} else {
			QMessageBox::critical(
				nullptr,
				tr("No Suitable Address Found"),
				tr("This feature relies on running shellcode in the debugged process, no executable memory region was found. Unfortunately, this means that no more region permission changes can be made (it also means that there is nothing the process can continue to do since it cannot execute at all)."));
		}
	}
}

/**
 * @brief PlatformRegion::start
 * @return
 */
edb::address_t PlatformRegion::start() const {
	return start_;
}

/**
 * @brief PlatformRegion::end
 * @return
 */
edb::address_t PlatformRegion::end() const {
	return end_;
}

/**
 * @brief PlatformRegion::base
 * @return
 */
edb::address_t PlatformRegion::base() const {
	return base_;
}

/**
 * @brief PlatformRegion::name
 * @return
 */
QString PlatformRegion::name() const {
	return name_;
}

/**
 * @brief PlatformRegion::permissions
 * @return
 */
IRegion::permissions_t PlatformRegion::permissions() const {
	return permissions_;
}

/**
 * @brief PlatformRegion::setPermissions
 * @param read
 * @param write
 * @param execute
 * @param temp_address
 */
void PlatformRegion::setPermissions(bool read, bool write, bool execute, edb::address_t temp_address) {
	const permissions_t perms = permissions_value(read, write, execute);
	const edb::address_t len  = size();
	const edb::address_t addr = start();

	// I wish there was a clean way to get the value of this system call for either target
	// but nothing obvious comes to mind. We may have to do something crazy
	// with macros, but for now, we just hard code it :-/
	const edb::address_t syscallnum = edb::v1::debuggeeIs32Bit() ? 125 : 10; //__NR_mprotect;

#if defined(EDB_X86) || defined(EDB_X86_64)
	// start of nowhere near portable code
	const uint8_t shellcode32[] = {
		"\xcd\x80" // int $0x80
		"\xf4"     // hlt
	};

	const uint8_t shellcode64[] = {
		"\x0f\x05" // syscall
		"\xf4"     // hlt
	};

	uint8_t shellcode[3];

	if (edb::v1::debuggeeIs32Bit()) {
		memcpy(shellcode, shellcode32, sizeof(shellcode));
	} else {
		memcpy(shellcode, shellcode64, sizeof(shellcode));
	}

	// end nowhere near portable code
	using BI = BackupInfo<sizeof(shellcode)>;
	if (IProcess *process = edb::v1::debugger_core->process()) {
		if (std::shared_ptr<IThread> thread = process->currentThread()) {
			try {
				BI backup_info(temp_address, perms, this);

				if (backup_info.backup()) {
					// write out our shellcode
					if (process->writeBytes(temp_address, shellcode, sizeof(shellcode))) {

						State state;
						thread->getState(&state);
						state.setInstructionPointer(temp_address);

						if (edb::v1::debuggeeIs32Bit()) {
							state.setRegister("ecx", len);
							state.setRegister("ebx", addr);
							state.setRegister("edx", perms);
							state.setRegister("eax", syscallnum);
						} else {
							state.setRegister("rsi", len);
							state.setRegister("rdi", addr);
							state.setRegister("rdx", perms);
							state.setRegister("rax", syscallnum);
						}

						thread->setState(state);

						// run the system call instruction and wait for the trap
						thread->step(edb::DEBUG_CONTINUE);

						// we use a spinlock here because we want to be able to
						// process events while waiting
						while (backup_info.locked()) {
							QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
						}
					}
				}
			} catch (const std::bad_alloc &) {
				QMessageBox::critical(
					nullptr,
					tr("Memory Allocation Error"),
					tr("Unable to satisfy memory allocation request for backup code."));
			}
		}
	}
#endif
}

/**
 * @brief PlatformRegion::setStart
 * @param address
 */
void PlatformRegion::setStart(edb::address_t address) {
	start_ = address;
}

/**
 * @brief PlatformRegion::setEnd
 * @param address
 */
void PlatformRegion::setEnd(edb::address_t address) {
	end_ = address;
}

}

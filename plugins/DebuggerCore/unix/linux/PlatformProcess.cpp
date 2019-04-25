/*
Copyright (C) 2015 - 2015 Evan Teran
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

#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#include "PlatformProcess.h"
#include "DebuggerCore.h"
#include "IBreakpoint.h"
#include "PlatformThread.h"
#include "PlatformCommon.h"
#include "PlatformRegion.h"
#include "ByteShiftArray.h"
#include "MemoryRegions.h"
#include "Module.h"
#include "edb.h"
#include "Util.h"
#include "linker.h"
#include "libELF/elf_binary.h"
#include "libELF/elf_model.h"

#include <QDebug>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDateTime>

#include <boost/functional/hash.hpp>
#include <fstream>

#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <linux/limits.h>
#include <unistd.h>
#include <pwd.h>
#include <elf.h>

namespace DebuggerCorePlugin {
namespace {

// Used as size of ptrace word
constexpr size_t EDB_WORDSIZE = sizeof(long);

void set_ok(bool &ok, long value) {
	ok = (value != -1) || (errno == 0);
}

QStringList split_max(const QString &str, int maxparts) {
	int prev_idx = 0;
	int idx = 0;
	QStringList items;
	for (const QChar &c : str) {
		if (c == ' ') {
			if (prev_idx < idx) {
				if (items.size() < maxparts - 1)
					items << str.mid(prev_idx, idx - prev_idx);
				else {
					items << str.right(str.size() - prev_idx);
					break;
				}
			}
			prev_idx = idx + 1;
		}
		++idx;
	}
	if (prev_idx < str.size() && items.size() < maxparts) {
		items << str.right(str.size() - prev_idx);
	}
	return items;
}

//------------------------------------------------------------------------------
// Name: process_map_line
// Desc: parses the data from a line of a memory map file
//------------------------------------------------------------------------------
std::shared_ptr<IRegion> process_map_line(const QString &line) {

	edb::address_t start;
	edb::address_t end;
	edb::address_t base;
	IRegion::permissions_t permissions;
	QString name;

	const QStringList items = split_max(line, 6);
	if(items.size() >= 3) {
		bool ok;
		const QStringList bounds = items[0].split("-");
		if(bounds.size() == 2) {
			start = edb::address_t::fromHexString(bounds[0],&ok);
			if(ok) {
				end = edb::address_t::fromHexString(bounds[1],&ok);
				if(ok) {
					base = edb::address_t::fromHexString(items[2],&ok);
					if(ok) {
						const QString perms = items[1];
						permissions = 0;
						if(perms[0] == 'r') permissions |= PROT_READ;
						if(perms[1] == 'w') permissions |= PROT_WRITE;
						if(perms[2] == 'x') permissions |= PROT_EXEC;

						if(items.size() >= 6) {
							name = items[5];
						}

						return std::make_shared<PlatformRegion>(start, end, base, name, permissions);
					}
				}
			}
		}
	}
	return nullptr;;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
template<class Addr>
QList<Module> get_loaded_modules(const IProcess* process) {

	QList<Module> ret;

	edb::linux_struct::r_debug<Addr> dynamic_info;
	if(process) {
		if(const edb::address_t debug_pointer = process->debug_pointer()) {
			if(process->read_bytes(debug_pointer, &dynamic_info, sizeof(dynamic_info))) {
				if(dynamic_info.r_map) {

					auto link_address = edb::address_t::fromZeroExtended(dynamic_info.r_map);

					while(link_address) {

						edb::linux_struct::link_map<Addr> map;
						if(process->read_bytes(link_address, &map, sizeof(map))) {
							char path[PATH_MAX];
							if(!process->read_bytes(edb::address_t::fromZeroExtended(map.l_name), &path, sizeof(path))) {
								path[0] = '\0';
							}

							if(map.l_addr) {
								Module module;
								module.name         = path;
								module.base_address = map.l_addr;
								ret.push_back(module);
							}

							link_address = edb::address_t::fromZeroExtended(map.l_next);
						} else {
							break;
						}
					}
				}
			}
		}
	}

	// fallback
	if(ret.isEmpty()) {
		const QList<std::shared_ptr<IRegion>> r = edb::v1::memory_regions().regions();
		QSet<QString> found_modules;

		for(const std::shared_ptr<IRegion> &region: r) {

			// we assume that modules will be listed by absolute path
			if(region->name().startsWith("/")) {
				if(!util::contains(found_modules, region->name())) {
					Module module;
					module.name         = region->name();
					module.base_address = region->start();
					found_modules.insert(region->name());
					ret.push_back(module);
				}
			}
		}
	}

	return ret;
}

}

//------------------------------------------------------------------------------
// Name: PlatformProcess
// Desc:
//------------------------------------------------------------------------------
PlatformProcess::PlatformProcess(DebuggerCore *core, edb::pid_t pid) : core_(core), pid_(pid) {
	if (!core_->proc_mem_read_broken_) {
		auto memory_file = std::make_shared<QFile>(QString("/proc/%1/mem").arg(pid_));

		auto flags = QIODevice::ReadOnly | QIODevice::Unbuffered;
		if (!core_->proc_mem_write_broken_) {
			flags |= QIODevice::WriteOnly;
		}
		if (memory_file->open(flags)) {
			ro_mem_file_ = memory_file;
			if (!core_->proc_mem_write_broken_) {
				rw_mem_file_ = memory_file;
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: seek_addr
// Desc: seeks memory file to given address, taking possible negativity of the
// address into account
//------------------------------------------------------------------------------
void seek_addr(QFile& file, edb::address_t address) {
	if(address <= UINT64_MAX/2) {
		file.seek(address);
	} else {
		const int fd=file.handle();
		// Seek in two parts to avoid specifying negative offset: off64_t is a signed type
		const off64_t halfAddressTruncated=address>>1;
		lseek64(fd,halfAddressTruncated,SEEK_SET);
		const off64_t secondHalfAddress=address-halfAddressTruncated;
		lseek64(fd,secondHalfAddress,SEEK_CUR);
	}
}


//------------------------------------------------------------------------------
// Name: read_bytes
// Desc: reads <len> bytes into <buf> starting at <address>
// Note: returns the number of bytes read <N>
// Note: if the read is short, only the first <N> bytes are defined
//------------------------------------------------------------------------------
std::size_t PlatformProcess::read_bytes(edb::address_t address, void* buf, std::size_t len) const {
	quint64 read = 0;

	Q_ASSERT(buf);
	Q_ASSERT(core_->process_.get() == this);

	auto ptr = reinterpret_cast<char *>(buf);

	if(len != 0) {

		// small reads take the fast path
		if(len == 1) {

			auto it = core_->breakpoints_.find(address);
			if(it != core_->breakpoints_.end()) {
				*ptr = (*it)->original_bytes()[0];
				return 1;
			}

			if(ro_mem_file_) {
				seek_addr(*ro_mem_file_, address);
				read = ro_mem_file_->read(ptr, 1);
				if (read == 1) {
					return 1;
				}
				return 0;
			} else {
				bool ok;
				quint8 x = read_byte_via_ptrace(address, &ok);
				if(ok) {
					*ptr = x;
					return 1;
				}
				return 0;
			}
		}

		if(ro_mem_file_) {
			seek_addr(*ro_mem_file_, address);
			read = ro_mem_file_->read(ptr, len);
			if(read == 0 || read == quint64(-1)) {
				return 0;
			}
		} else {
			for(std::size_t index = 0; index < len; ++index) {

				// read a byte, if we failed, we are done
				bool ok;
				const quint8 x = read_byte_via_ptrace(address + index, &ok);
				if(!ok) {
					break;
				}

				// store it
				reinterpret_cast<char*>(buf)[index] = x;

				++read;
			}
		}

		// replace any breakpoints
		Q_FOREACH(const std::shared_ptr<IBreakpoint> &bp, core_->breakpoints_) {
			auto bpBytes = bp->original_bytes();
			const edb::address_t bpAddr = bp->address();
			// show the original bytes in the buffer..
			for(size_t i=0; i < bp->size(); ++i) {
				if(bpAddr + i >= address && bpAddr + i < address + read) {
					ptr[bpAddr + i - address] = bpBytes[i];
				}
			}
		}
	}

	return read;
}

//------------------------------------------------------------------------------
// Name: patch_bytes
// Desc: same as write_bytes, except that it also records the original data
//       that was found at the address being written to.
// Note: unlike the read_bytes, write_bytes functions, this will not apply the
//       write if we could not properly backup <len> bytes as requested.
// Note: on the off chance that we can READ <len> bytes, but can't WRITE <len>
//       bytes, we will return the number of bytes written, but record <len>
//       bytes of patch data.
//------------------------------------------------------------------------------
std::size_t PlatformProcess::patch_bytes(edb::address_t address, const void *buf, size_t len) {
	Q_ASSERT(buf);
	Q_ASSERT(core_->process_.get() == this);

	Patch patch;
	patch.address = address;
	patch.orig_bytes.resize(len);
	patch.new_bytes = QByteArray(static_cast<const char *>(buf), len);

	size_t read_ret = read_bytes(address, patch.orig_bytes.data(), len);
	if(read_ret != len) {
		return 0;
	}

	patches_.insert(address, patch);

	return write_bytes(address, buf, len);
}

//------------------------------------------------------------------------------
// Name: write_bytes
// Desc: writes <len> bytes from <buf> starting at <address>
//------------------------------------------------------------------------------
std::size_t PlatformProcess::write_bytes(edb::address_t address, const void *buf, std::size_t len) {
	quint64 written = 0;

	Q_ASSERT(buf);
	Q_ASSERT(core_->process_.get() == this);

	if(len != 0) {
		if(rw_mem_file_) {
			seek_addr(*rw_mem_file_,address);
			written = rw_mem_file_->write(reinterpret_cast<const char *>(buf), len);
			if(written == 0 || written == quint64(-1)) {
				return 0;
			}
		}
		else {
			// TODO write whole words at a time using ptrace_poke.
			for(std::size_t byteIndex=0;byteIndex<len;++byteIndex) {
				bool ok=false;
				write_byte_via_ptrace(address+byteIndex, *(reinterpret_cast<const char*>(buf)+byteIndex), &ok);
				if(!ok) return written;
				++written;
			}
		}
	}

	return written;
}

//------------------------------------------------------------------------------
// Name: read_pages
// Desc: reads <count> pages from the process starting at <address>
// Note: buf's size must be >= count * core_->page_size()
// Note: address should be page aligned.
//------------------------------------------------------------------------------
std::size_t PlatformProcess::read_pages(edb::address_t address, void *buf, std::size_t count) const {
	Q_ASSERT(buf);
	Q_ASSERT(core_->process_.get() == this);
	return read_bytes(address, buf, count * core_->page_size()) / core_->page_size();
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QDateTime PlatformProcess::start_time() const {
	QFileInfo info(QString("/proc/%1/stat").arg(pid_));
	return info.created();
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QList<QByteArray> PlatformProcess::arguments() const {
	QList<QByteArray> ret;

	if(pid_ != 0) {
		const QString command_line_file(QString("/proc/%1/cmdline").arg(pid_));
		QFile file(command_line_file);

		if(file.open(QIODevice::ReadOnly | QIODevice::Text)) {
			QTextStream in(&file);

			QByteArray s;
			QChar ch;

			while(in.status() == QTextStream::Ok) {
				in >> ch;
				if(ch.isNull()) {
					if(!s.isEmpty()) {
						ret << s;
					}
					s.clear();
				} else {
					s += ch;
				}
			}

			if(!s.isEmpty()) {
				ret << s;
			}
		}
	}
	return ret;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString PlatformProcess::current_working_directory() const {
	return edb::v1::symlink_target(QString("/proc/%1/cwd").arg(pid_));
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString PlatformProcess::executable() const {
	return edb::v1::symlink_target(QString("/proc/%1/exe").arg(pid_));
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
edb::pid_t PlatformProcess::pid() const {
	return pid_;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
std::shared_ptr<IProcess> PlatformProcess::parent() const {

	struct user_stat user_stat;
	int n = get_user_stat(pid_, &user_stat);
	if(n >= 4) {
		return std::make_shared<PlatformProcess>(core_, user_stat.ppid);
	}

	return nullptr;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
edb::address_t PlatformProcess::code_address() const {
	struct user_stat user_stat;
	int n = get_user_stat(pid_, &user_stat);
	if(n >= 26) {
		return user_stat.startcode;
	}
	return 0;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
edb::address_t PlatformProcess::data_address() const {
	struct user_stat user_stat;
	int n = get_user_stat(pid_, &user_stat);
	if(n >= 27) {
		return user_stat.endcode + 1; // endcode == startdata ?
	}
	return 0;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QList<std::shared_ptr<IRegion>> PlatformProcess::regions() const {

	static QList<std::shared_ptr<IRegion>> regions;
	const QString map_file(QString("/proc/%1/maps").arg(pid_));

	// hash the region file to see if it changed or not
	{
		static size_t totalHash = 0;

		std::ifstream mf(map_file.toStdString());
		size_t newHash = 0;
		std::string line;

		while(std::getline(mf,line)) {
			boost::hash_combine(newHash, line);
		}

		if(totalHash == newHash) {
			return regions;
		}

		totalHash = newHash;
		regions.clear();
	}

	// it changed, so let's process it
	QFile file(map_file);
    if(file.open(QIODevice::ReadOnly | QIODevice::Text)) {

		QTextStream in(&file);
		QString line = in.readLine();

		while(!line.isNull()) {
			if(std::shared_ptr<IRegion> region = process_map_line(line)) {
				regions.push_back(region);
			}
			line = in.readLine();
		}
	}

	return regions;
}

//------------------------------------------------------------------------------
// Name: read_byte
// Desc: the base implementation of reading a byte
//------------------------------------------------------------------------------
quint8 PlatformProcess::read_byte_via_ptrace(edb::address_t address, bool *ok) const {
	// TODO(eteran): assert that we are paused

	Q_ASSERT(ok);
	Q_ASSERT(core_->process_.get() == this);

	*ok = false;

	// if this spot is unreadable, then just return 0xff, otherwise
	// continue as normal.

	// core_->page_size() - 1 will always be 0xf* because pagesizes
	// are always 0x10*, so the masking works
	// range of nBytesToNextPage is [1..n] where n=pagesize, and we have to adjust
	// if nByteToNextPage < wordsize
	const size_t nBytesToNextPage = core_->page_size() - (address & (core_->page_size() - 1));

	// Avoid crossing page boundary, since next page may be unreadable
	const size_t addressShift = nBytesToNextPage < EDB_WORDSIZE ? EDB_WORDSIZE - nBytesToNextPage : 0;
	address -= addressShift;

	const long value = ptrace_peek(address, ok);

	if(*ok) {
		quint8 result;
		// We aren't interested in `value` as in number, it's just a buffer, so no endianness magic.
		// Just have to compensate for `addressShift` when reading it.
		std::memcpy(&result, reinterpret_cast<const char*>(&value) + addressShift, sizeof(result));
		return result;
	}

	return 0xff;
}


//------------------------------------------------------------------------------
// Name: write_byte
// Desc: writes a single byte at a given address via ptrace API.
// Note: assumes the this will not trample any breakpoints, must be handled
//       in calling code!
//------------------------------------------------------------------------------
void PlatformProcess::write_byte_via_ptrace(edb::address_t address, quint8 value, bool *ok) {
	// TODO(eteran): assert that we are paused

	Q_ASSERT(ok);
	Q_ASSERT(core_->process_.get() == this);

	*ok = false;

	// core_->page_size() - 1 will always be 0xf* because pagesizes
	// are always 0x10*, so the masking works
	// range of nBytesToNextPage is [1..n] where n=pagesize, and we have to adjust
	// if nBytesToNextPage < wordsize
	const size_t nBytesToNextPage = core_->page_size() - (address & (core_->page_size() - 1));

	// Avoid crossing page boundary, since next page may be inaccessible
	const size_t addressShift = nBytesToNextPage < EDB_WORDSIZE ? EDB_WORDSIZE - nBytesToNextPage : 0;
	address -= addressShift;

	long word = ptrace_peek(address, ok);
	if(!*ok) {
		return;
	}

	// We aren't interested in `value` as in number, it's just a buffer, so no endianness magic.
	// Just have to compensate for `addressShift` when writing it.
	std::memcpy(reinterpret_cast<char*>(&word) + addressShift, &value, sizeof(value));

	*ok = ptrace_poke(address, word);
}

//------------------------------------------------------------------------------
// Name: ptrace_peek
// Desc:
// Note: this will fail on newer versions of linux if called from a
//       different thread than the one which attached to process
//------------------------------------------------------------------------------
long PlatformProcess::ptrace_peek(edb::address_t address, bool *ok) const {
	Q_ASSERT(ok);
	Q_ASSERT(core_->process_.get() == this);

	if (EDB_IS_32_BIT && address > 0xffffffffULL) {
		// 32 bit ptrace can't handle such long addresses
		*ok = false;
		return 0;
	}

	errno = 0;
	// NOTE: on some Linux systems ptrace prototype has ellipsis instead of third and fourth arguments
	// Thus we can't just pass address as is on IA32 systems: it'd put 64 bit integer on stack and cause UB
	auto nativeAddress = reinterpret_cast<const void*>(address.toUint());
	const long v = ptrace(PTRACE_PEEKTEXT, pid_, nativeAddress, 0);
	set_ok(*ok, v);
	return v;
}

//------------------------------------------------------------------------------
// Name: ptrace_poke
// Desc:
//------------------------------------------------------------------------------
bool PlatformProcess::ptrace_poke(edb::address_t address, long value) {

	Q_ASSERT(core_->process_.get() == this);

	if (EDB_IS_32_BIT && address > 0xffffffffULL) {
		// 32 bit ptrace can't handle such long addresses
		return 0;
	}

	// NOTE: on some Linux systems ptrace prototype has ellipsis instead of third and fourth arguments
	// Thus we can't just pass address as is on IA32 systems: it'd put 64 bit integer on stack and cause UB
	auto nativeAddress = reinterpret_cast<const void*>(address.toUint());
	return ptrace(PTRACE_POKETEXT, pid_, nativeAddress, value) != -1;
}

//------------------------------------------------------------------------------
// Name: threads
// Desc:
//------------------------------------------------------------------------------
QList<std::shared_ptr<IThread>> PlatformProcess::threads() const {

	Q_ASSERT(core_->process_.get() == this);

	QList<std::shared_ptr<IThread>> threadList;
	threadList.reserve(core_->threads_.size());
	std::copy(core_->threads_.begin(), core_->threads_.end(), std::back_inserter(threadList));
	return threadList;
}

//------------------------------------------------------------------------------
// Name: current_thread
// Desc:
//------------------------------------------------------------------------------
std::shared_ptr<IThread> PlatformProcess::current_thread() const {

	Q_ASSERT(core_->process_.get() == this);

	auto it = core_->threads_.find(core_->active_thread_);
	if(it != core_->threads_.end()) {
		return it.value();
	}
	return nullptr;
}

//------------------------------------------------------------------------------
// Name: set_current_thread
// Desc:
//------------------------------------------------------------------------------
void PlatformProcess::set_current_thread(IThread& thread) {
	core_->active_thread_ = static_cast<PlatformThread*>(&thread)->tid();
	edb::v1::update_ui();
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
edb::uid_t PlatformProcess::uid() const {

	const QFileInfo info(QString("/proc/%1").arg(pid_));
	return info.ownerId();
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString PlatformProcess::user() const {
	if(const struct passwd *const pwd = ::getpwuid(uid())) {
		return pwd->pw_name;
	}

	return QString();
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString PlatformProcess::name() const {
	struct user_stat user_stat;
	const int n = get_user_stat(pid_, &user_stat);
	if(n >= 2) {
		return user_stat.comm;
	}

	return QString();
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QList<Module> PlatformProcess::loaded_modules() const {
	if(edb::v1::debuggeeIs64Bit()) {
		return get_loaded_modules<Elf64_Addr>(this);
	} else if(edb::v1::debuggeeIs32Bit()) {
		return get_loaded_modules<Elf32_Addr>(this);
	} else {
		return QList<Module>();
	}
}

//------------------------------------------------------------------------------
// Name: pause
// Desc: stops *all* threads of a process
//------------------------------------------------------------------------------
Status PlatformProcess::pause() {
	// belive it or not, I belive that this is sufficient for all threads
	// this is because in the debug event handler, a SIGSTOP is sent
	// to all threads when any event arrives, so no need to explicitly do
	// it here. We just need any thread to stop. So we'll just target the
	// pid_ which will send it to any one of the threads in the process.
	if(::kill(pid_, SIGSTOP) == -1) {
		const char *const strError = strerror(errno);
		qWarning() << "Unable to pause process" << pid_ << ": kill(SIGSTOP) failed:" << strError;
		return Status(strError);
	}

	return Status::Ok;
}

//------------------------------------------------------------------------------
// Name: resume
// Desc: resumes ALL threads
//------------------------------------------------------------------------------
Status PlatformProcess::resume(edb::EVENT_STATUS status) {

	// NOTE(eteran): OK, this is very tricky. When the user wants to resume
	// while ignoring a signal (DEBUG_CONTINUE), we need to know which thread
	// needs to have the signal ignored, and which need to have their signals
	// passed during the resume

	// TODO: assert that we are paused
	Q_ASSERT(core_->process_.get() == this);

	QString errorMessage;

	if(status != edb::DEBUG_STOP) {

		if(std::shared_ptr<IThread> thread = current_thread()) {
			const auto resumeStatus = thread->resume(status);
			if(!resumeStatus) {
				errorMessage += tr("Failed to resume thread %1: %2\n").arg(thread->tid()).arg(resumeStatus.error());
			}

			// resume the other threads passing the signal they originally reported had
			for(auto &other_thread : threads()) {
				if(util::contains(core_->waited_threads_, other_thread->tid())) {
					const auto resumeStatus = other_thread->resume();
					if(!resumeStatus) {
						errorMessage += tr("Failed to resume thread %1: %2\n").arg(thread->tid()).arg(resumeStatus.error());
					}
				}
			}
		}
	}

	if(errorMessage.isEmpty()) {
		return Status::Ok;
	}

	qWarning() << errorMessage.toStdString().c_str();
	return Status("\n" + errorMessage);
}

//------------------------------------------------------------------------------
// Name: step
// Desc: steps the currently active thread
//------------------------------------------------------------------------------
Status PlatformProcess::step(edb::EVENT_STATUS status) {
	// TODO: assert that we are paused
	Q_ASSERT(core_->process_.get() == this);

	if(status != edb::DEBUG_STOP) {
		if(std::shared_ptr<IThread> thread = current_thread()) {
			return thread->step(status);
		}
	}
	return Status::Ok;
}

//------------------------------------------------------------------------------
// Name: isPaused
// Desc: returns true if ALL threads are currently in the debugger's wait list
//------------------------------------------------------------------------------
bool PlatformProcess::isPaused() const {
	for(auto &thread : threads()) {
		if(!thread->isPaused()) {
			return false;
		}
	}

	return true;
}

//------------------------------------------------------------------------------
// Name: patches
// Desc: returns any patches applied to this process
//------------------------------------------------------------------------------
QMap<edb::address_t, Patch> PlatformProcess::patches() const {
	return patches_;
}

/**
 * @brief PlatformProcess::entry_point
 * @return
 */
edb::address_t PlatformProcess::entry_point() const  {

	QFile auxv(QString("/proc/%1/auxv").arg(pid_));
	if(auxv.open(QIODevice::ReadOnly)) {

		if(edb::v1::debuggeeIs64Bit()) {
			elf64_auxv_t entry;
			while(auxv.read(reinterpret_cast<char *>(&entry), sizeof(entry))) {
				if(entry.a_type == AT_ENTRY) {
					return entry.a_un.a_val;
				}
			}
		} else if(edb::v1::debuggeeIs32Bit()) {
			elf32_auxv_t entry;
			while(auxv.read(reinterpret_cast<char *>(&entry), sizeof(entry))) {
				if(entry.a_type == AT_ENTRY) {
					return entry.a_un.a_val;
				}
			}
		}
	}

	return edb::address_t{};
}

/**
 * @brief get_program_headers
 * @param process
 * @param phdr_memaddr
 * @param num_phdr
 * @return
 */
bool get_program_headers(const IProcess *process, edb::address_t *phdr_memaddr, int *num_phdr) {

	*phdr_memaddr = edb::address_t{};
	*num_phdr = 0;

	QFile auxv(QString("/proc/%1/auxv").arg(process->pid()));
	if(auxv.open(QIODevice::ReadOnly)) {

		if(edb::v1::debuggeeIs64Bit()) {
			elf64_auxv_t entry;
			while(auxv.read(reinterpret_cast<char *>(&entry), sizeof(entry))) {
				switch(entry.a_type) {
				case AT_PHDR:
					*phdr_memaddr = entry.a_un.a_val;
					break;
				case AT_PHNUM:
					*num_phdr = entry.a_un.a_val;
					break;
				}
			}
		} else if(edb::v1::debuggeeIs32Bit()) {
			elf32_auxv_t entry;
			while(auxv.read(reinterpret_cast<char *>(&entry), sizeof(entry))) {
				switch(entry.a_type) {
				case AT_PHDR:
					*phdr_memaddr = entry.a_un.a_val;
					break;
				case AT_PHNUM:
					*num_phdr = entry.a_un.a_val;
					break;
				}
			}
		}
	}

	return (*phdr_memaddr != 0 && *num_phdr != 0);
}

/**
 * @brief get_debug_pointer
 * @param process
 * @param phdr_memaddr
 * @param count
 * @param relocation
 * @return
 */
template <class Model>
edb::address_t get_debug_pointer(const IProcess *process, edb::address_t phdr_memaddr, int count, edb::address_t relocation) {

	using elf_phdr = typename Model::elf_phdr;

	elf_phdr phdr;
	for(int i = 0; i < count; ++i) {
		if(process->read_bytes(phdr_memaddr + i * sizeof(elf_phdr), &phdr, sizeof(elf_phdr))) {
			if(phdr.p_type == PT_DYNAMIC) {
				try {

					auto buf = std::make_unique<uint8_t[]>(phdr.p_memsz);

					if(process->read_bytes(phdr.p_vaddr + relocation, &buf[0], phdr.p_memsz)) {
						auto dynamic = reinterpret_cast<typename Model::elf_dyn *>(&buf[0]);
						while(dynamic->d_tag != DT_NULL) {
							if(dynamic->d_tag == DT_DEBUG) {
								return dynamic->d_un.d_val;
							}
							++dynamic;
						}
					}
				} catch(const std::bad_alloc &) {
					qDebug() << "[get_debug_pointer] no more memory";
					return 0;
				}
			}
		}
	}

	return 0;
}

/**
 * @brief get_relocation
 * @param process
 * @param phdr_memaddr
 * @param i
 * @return
 */
template <class Model>
edb::address_t get_relocation(const IProcess *process, edb::address_t phdr_memaddr, int i) {

	using elf_phdr = typename Model::elf_phdr;

	elf_phdr phdr;
	if(process->read_bytes(phdr_memaddr + i * sizeof(elf_phdr), &phdr, sizeof(elf_phdr))) {
		if (phdr.p_type == PT_PHDR) {
			return phdr_memaddr - phdr.p_vaddr;
		}
	}

	return -1;
}

/**
 * attempts to locate the ELF debug pointer in the target process and returns
 * it, 0 of not found
 *
 * @brief PlatformProcess::debug_pointer
 * @return
 */
edb::address_t PlatformProcess::debug_pointer() const {

	// NOTE(eteran): some of this code is from or inspired by code in
	// gdb/gdbserver/linux-low.c

	edb::address_t phdr_memaddr;
	int num_phdr;

	if(get_program_headers(this, &phdr_memaddr, &num_phdr)) {

		/* Compute relocation: it is expected to be 0 for "regular" executables,
		 * non-zero for PIE ones.  */
		edb::address_t relocation = -1;
		for (int i = 0; relocation == -1 && i < num_phdr; i++) {

			if(edb::v1::debuggeeIs64Bit()) {
				relocation = get_relocation<elf_model<64>>(this, phdr_memaddr, i);
			} else if(edb::v1::debuggeeIs32Bit()) {
				relocation = get_relocation<elf_model<32>>(this, phdr_memaddr, i);
			}
		}

		if (relocation == -1) {
			/* PT_PHDR is optional, but necessary for PIE in general.
			 * Fortunately any real world executables, including PIE
			 * executables, have always PT_PHDR present.  PT_PHDR is not
			 * present in some shared libraries or in fpc (Free Pascal 2.4)
			 * binaries but neither of those have a need for or present
			 * DT_DEBUG anyway (fpc binaries are statically linked).
			 *
			 * Therefore if there exists DT_DEBUG there is always also PT_PHDR.
			 *
			 * GDB could find RELOCATION also from AT_ENTRY - e_entry.  */
			return 0;
		}

		if(edb::v1::debuggeeIs64Bit()) {
			return get_debug_pointer<elf_model<64>>(this, phdr_memaddr, num_phdr, relocation);
		} else if(edb::v1::debuggeeIs32Bit()) {
			return get_debug_pointer<elf_model<32>>(this, phdr_memaddr, num_phdr, relocation);
		}
	}

	return edb::address_t{};
}

edb::address_t PlatformProcess::calculate_main() const {
	if(edb::v1::debuggeeIs64Bit()) {
		ByteShiftArray ba(14);

		edb::address_t entry_point = this->entry_point();

		for(int i = 0; i < 50; ++i) {
			quint8 byte;
			if(read_bytes(entry_point + i, &byte, sizeof(byte))) {
				ba << byte;

				edb::address_t address = 0;

				if(ba.size() >= 13) {
					// beginning of a call preceeded by a 64-bit mov and followed by a hlt
					if(ba[0] == 0x48 && ba[1] == 0xc7 && ba[7] == 0xe8 && ba[12] == 0xf4) {
						// Seems that this 64-bit mov still has a 32-bit immediate
						address = *reinterpret_cast<const edb::address_t *>(ba.data() + 3) & 0xffffffff;
					}

					// same heuristic except for PIC binaries
					else if (ba.size() >= 14 && ba[0] == 0x48 && ba[1] == 0x8d && ba[2] == 0x3d && ba[7] == 0xFF && ba[8] == 0x15 && ba[13] == 0xf4) {
						// It's signed relative!
						auto rel = *reinterpret_cast<const qint32 *>(ba.data() + 3);
						// ba[0] is entry_point + i - 13. instruction is 7 bytes long.
						address = rel + entry_point + i - 13 + 7;
					}

					if (address) {
						// TODO: make sure that this address resides in an executable region
						qDebug() << "No main symbol found, calculated it to be " << edb::v1::format_pointer(address) << " using heuristic";
						return address;
					}
				}
			} else {
				break;
			}
		}
	} else if(edb::v1::debuggeeIs32Bit()) {
		ByteShiftArray ba(11);


		edb::address_t entry_point = this->entry_point();

		for(int i = 0; i < 50; ++i) {
			quint8 byte;
			if(read_bytes(entry_point + i, &byte, sizeof(byte))) {
				ba << byte;

				if(ba.size() >= 11) {
					// beginning of a call preceeded by a push and followed by a hlt
					if(ba[0] == 0x68 && ba[5] == 0xe8 && ba[10] == 0xf4) {
						edb::address_t address(0);

						auto to = reinterpret_cast<char *>(&address);
						std::memcpy(to, ba.data() + 1, sizeof(uint32_t));

						// TODO: make sure that this address resides in an executable region
						qDebug() << "No main symbol found, calculated it to be " << edb::v1::format_pointer(address) << " using heuristic";
						return address;
					}
				}
			} else {
				break;
			}
		}
	}

	return 0;
}

}

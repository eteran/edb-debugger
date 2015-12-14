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

#include "PlatformProcess.h"
#include "DebuggerCore.h"
#include "PlatformCommon.h"
#include "PlatformRegion.h"
#include "MemoryRegions.h"
#include "edb.h"

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
#include <pwd.h>
#include <elf.h>

// auto-generated
#include "procPidMemWrites.h"

namespace DebuggerCore {
namespace {

// Used as size of ptrace word
#define EDB_WORDSIZE sizeof(long)

namespace BinaryInfo {
// Bitness-templated version of struct r_debug defined in link.h
template<class Addr>
struct r_debug
{
	int r_version;
	Addr r_map; // struct link_map*
	Addr r_brk;
	enum {
		RT_CONSISTENT,
		RT_ADD,
		RT_DELETE
	} r_state;
	Addr r_ldbase;
};

// Bitness-templated version of struct link_map defined in link.h
template<class Addr>
struct link_map
{
	Addr l_addr;
	Addr l_name; // char*
	Addr l_ld; // ElfW(Dyn)*
	Addr l_next, l_prev; // struct link_map*
};
}

void set_ok(bool &ok, long value) {
	ok = (value != -1) || (errno == 0);
}

//------------------------------------------------------------------------------
// Name: process_map_line
// Desc: parses the data from a line of a memory map file
//------------------------------------------------------------------------------
IRegion::pointer process_map_line(const QString &line) {

	edb::address_t start;
	edb::address_t end;
	edb::address_t base;
	IRegion::permissions_t permissions;
	QString name;

	const QStringList items = line.split(" ", QString::SkipEmptyParts);
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
QList<Module> loaded_modules_(const IProcess* process, const std::unique_ptr<IBinary> &binary_info_) {
	QList<Module> ret;

	if(binary_info_) {
		BinaryInfo::r_debug<Addr> dynamic_info;
		if(const edb::address_t debug_pointer = binary_info_->debug_pointer()) {
			if(process) {
				if(process->read_bytes(debug_pointer, &dynamic_info, sizeof(dynamic_info))) {
					if(dynamic_info.r_map) {

						auto link_address = edb::address_t::fromZeroExtended(dynamic_info.r_map);

						while(link_address) {

							BinaryInfo::link_map<Addr> map;
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
	}

	// fallback
	if(ret.isEmpty()) {
		const QList<IRegion::pointer> r = edb::v1::memory_regions().regions();
		QSet<QString> found_modules;

		for(const IRegion::pointer &region: r) {

			// we assume that modules will be listed by absolute path
			if(region->name().startsWith("/")) {
				if(!found_modules.contains(region->name())) {
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

}

//------------------------------------------------------------------------------
// Name: ~PlatformProcess
// Desc: 
//------------------------------------------------------------------------------
PlatformProcess::~PlatformProcess() {
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
	Q_ASSERT(core_->process_ == this);
	
	if(len != 0) {

		// small reads take the fast path
		if(len == 1) {

			auto it = core_->breakpoints_.find(address);
			if(it != core_->breakpoints_.end()) {
				*reinterpret_cast<char *>(buf) = (*it)->original_byte();
				return 1;
			}

			bool ok;
			quint8 x = read_byte(address, &ok);
			if(ok) {
				*reinterpret_cast<char *>(buf) = x;
				return 1;
			}
			return 0;
		}


		QFile memory_file(QString("/proc/%1/mem").arg(pid_));
		if(memory_file.open(QIODevice::ReadOnly)) {

			memory_file.seek(address);
			read = memory_file.read(reinterpret_cast<char *>(buf), len);
			if(read == 0 || read == quint64(-1))
				return 0;

			for(const IBreakpoint::pointer &bp: core_->breakpoints_) {
				if(bp->address() >= address && bp->address() < (address + read)) {
					// show the original bytes in the buffer..
					reinterpret_cast<quint8 *>(buf)[bp->address() - address] = bp->original_byte();
				}
			}

			memory_file.close();
		}
	}

	return read;
}

//------------------------------------------------------------------------------
// Name: write_bytes
// Desc: writes <len> bytes from <buf> starting at <address>
//------------------------------------------------------------------------------
std::size_t PlatformProcess::write_bytes(edb::address_t address, const void *buf, std::size_t len) {
	quint64 written = 0;

	Q_ASSERT(buf);
	Q_ASSERT(core_->process_ == this);	
	
	if(len != 0) {
	
		// small writes take the fast path
		if(len == 1) {
			bool ok;
			write_byte(address, *reinterpret_cast<const char *>(buf), &ok);
			return ok ? 1 : 0;		
		}
	
		QFile memory_file(QString("/proc/%1/mem").arg(pid_));
		// NOTE: If buffered, it may not report any write errors, behaving as if it succeeded
		if(!PROC_PID_MEM_WRITE_BROKEN && memory_file.open(QIODevice::WriteOnly|QIODevice::Unbuffered)) {

			memory_file.seek(address);
			written = memory_file.write(reinterpret_cast<const char *>(buf), len);
			if(written == 0 || written == quint64(-1)) {
				return 0;
			}

			memory_file.close();
		}
		else {
			for(std::size_t byteIndex=0;byteIndex<len;++byteIndex) {
				bool ok=false;
				write_byte(address+byteIndex, *(reinterpret_cast<const char*>(buf)+byteIndex), &ok);
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
	Q_ASSERT(core_->process_ == this);
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
				if(ch == '\0') {
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
IProcess::pointer PlatformProcess::parent() const {

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
QList<IRegion::pointer> PlatformProcess::regions() const {
	static QList<IRegion::pointer> regions;
	static size_t totalHash = 0;

	const QString map_file(QString("/proc/%1/maps").arg(pid_));

	// hash the region file to see if it changed or not
	{
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
			if(IRegion::pointer region = process_map_line(line)) {
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
quint8 PlatformProcess::read_byte(edb::address_t address, bool *ok) const {
	// TODO(eteran): assert that we are paused

	Q_ASSERT(ok);
	Q_ASSERT(core_->process_ == this);

	*ok = false;

	// if this spot is unreadable, then just return 0xff, otherwise
	// continue as normal.

	// core_->page_size() - 1 will always be 0xf* because pagesizes
	// are always 0x10*, so the masking works
	// range of nBytesToNextPage is [1..n] where n=pagesize, and we have to adjust
	// if nByteToNextPage < wordsize
	const edb::address_t nBytesToNextPage = core_->page_size() - (address & (core_->page_size() - 1));

	// Avoid crossing page boundary, since next page may be unreadable
	const edb::address_t addressShift = nBytesToNextPage < EDB_WORDSIZE ? EDB_WORDSIZE - nBytesToNextPage : 0;
	address -= addressShift;

	const long value = read_data(address, ok);

	if(*ok) {
		quint8 result;
		// We aren't interested in `value` as in number, it's just a buffer, so no endianness magic.
		// Just have to compensate for `addressShift` when reading it.
		std::memcpy(&result,reinterpret_cast<const char*>(&value)+addressShift,sizeof result);
		return result;
	}

	return 0xff;
}


//------------------------------------------------------------------------------
// Name: write_byte
// Desc: writes a single byte at a given address
// Note: assumes the this will not trample any breakpoints, must be handled
//       in calling code!
//------------------------------------------------------------------------------
void PlatformProcess::write_byte(edb::address_t address, quint8 value, bool *ok) {
	// TODO(eteran): assert that we are paused

	Q_ASSERT(ok);
	Q_ASSERT(core_->process_ == this);

	*ok = false;

	// core_->page_size() - 1 will always be 0xf* because pagesizes
	// are always 0x10*, so the masking works
	// range of nBytesToNextPage is [1..n] where n=pagesize, and we have to adjust
	// if nBytesToNextPage < wordsize
	const edb::address_t nBytesToNextPage = core_->page_size() - (address & (core_->page_size() - 1));

	// Avoid crossing page boundary, since next page may be inaccessible
	const edb::address_t addressShift = nBytesToNextPage < EDB_WORDSIZE ? EDB_WORDSIZE - nBytesToNextPage : 0;
	address -= addressShift;

	long word = read_data(address, ok);
	if(!*ok) return;

	// We aren't interested in `value` as in number, it's just a buffer, so no endianness magic.
	// Just have to compensate for `addressShift` when writing it.
	std::memcpy(reinterpret_cast<char*>(&word)+addressShift,&value,sizeof value);

	*ok = write_data(address, word);
}

//------------------------------------------------------------------------------
// Name: read_data
// Desc:
// Note: this will fail on newer versions of linux if called from a
//       different thread than the one which attached to process
//------------------------------------------------------------------------------
long PlatformProcess::read_data(edb::address_t address, bool *ok) const {

	Q_ASSERT(ok);
	Q_ASSERT(core_->process_ == this);

	if(EDB_IS_32_BIT && address>0xffffffffULL) {
		// 32 bit ptrace can't handle such long addresses, try reading /proc/$PID/mem
		// FIXME: this is slow. Try keeping the file open, not reopening it on each read.
		QFile memory_file(QString("/proc/%1/mem").arg(pid_));
		if(memory_file.open(QIODevice::ReadOnly)) {

			memory_file.seek(address);
			long value;
			if(memory_file.read(reinterpret_cast<char*>(&value), sizeof(long))==sizeof(long)) {
				*ok=true;
				return value;
			}
		}
		return 0;
	}

	errno = 0;
	// NOTE: on some Linux systems ptrace prototype has ellipsis instead of third and fourth arguments
	// Thus we can't just pass address as is on IA32 systems: it'd put 64 bit integer on stack and cause UB
	auto nativeAddress=reinterpret_cast<const void* const>(address.toUint());
	const long v = ptrace(PTRACE_PEEKTEXT, pid_, nativeAddress, 0);
	set_ok(*ok, v);
	return v;
}

//------------------------------------------------------------------------------
// Name: write_data
// Desc:
//------------------------------------------------------------------------------
bool PlatformProcess::write_data(edb::address_t address, long value) {

	Q_ASSERT(core_->process_ == this);

	if(EDB_IS_32_BIT && address>0xffffffffULL) {
		// 32 bit ptrace can't handle such long addresses
		QFile memory_file(QString("/proc/%1/mem").arg(pid_));
		if(memory_file.open(QIODevice::WriteOnly|QIODevice::Unbuffered)) { // If buffered, it may not report any errors as if it succeeded

			memory_file.seek(address);
			if(memory_file.write(reinterpret_cast<char*>(&value), sizeof(long))==sizeof(long))
				return true;
		}
		return false;
	}
	// NOTE: on some Linux systems ptrace prototype has ellipsis instead of third and fourth arguments
	// Thus we can't just pass address as is on IA32 systems: it'd put 64 bit integer on stack and cause UB
	auto nativeAddress=reinterpret_cast<const void* const>(address.toUint());
	return ptrace(PTRACE_POKETEXT, pid_, nativeAddress, value) != -1;
}

//------------------------------------------------------------------------------
// Name: threads
// Desc:
//------------------------------------------------------------------------------
QList<IThread::pointer> PlatformProcess::threads() const {

	Q_ASSERT(core_->process_ == this);

	QList<IThread::pointer> threadList;
	
	for(auto &thread : core_->threads_) {
		threadList.push_back(thread);
	}
	
	return threadList;
}

//------------------------------------------------------------------------------
// Name: current_thread
// Desc:
//------------------------------------------------------------------------------
IThread::pointer PlatformProcess::current_thread() const {

	Q_ASSERT(core_->process_ == this);

	auto it = core_->threads_.find(core_->active_thread_);
	if(it != core_->threads_.end()) {
		return it.value();
	}
	return IThread::pointer();
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
		return loaded_modules_<Elf64_Addr>(this, core_->binary_info_);
	} else if(edb::v1::debuggeeIs32Bit()) {
		return loaded_modules_<Elf32_Addr>(this, core_->binary_info_);
	} else {
		return QList<Module>();
	}
}

//------------------------------------------------------------------------------
// Name: pause
// Desc: stops *all* threads of a process
//------------------------------------------------------------------------------
void PlatformProcess::pause() {
	// belive it or not, I belive that this is sufficient for all threads
	// this is because in the debug event handler above, a SIGSTOP is sent
	// to all threads when any event arrives, so no need to explicitly do
	// it here. We just need any thread to stop. So we'll just target the
	// pid_ which will send it to any one of the threads in the process.
	::kill(pid_, SIGSTOP);
}

//------------------------------------------------------------------------------
// Name: resume
// Desc: resumes ALL threads
//------------------------------------------------------------------------------
void PlatformProcess::resume(edb::EVENT_STATUS status) {
	// TODO: assert that we are paused
	Q_ASSERT(core_->process_ == this);

	if(status != edb::DEBUG_STOP) {			
		if(IThread::pointer thread = current_thread()) {
			thread->resume(status);

			// resume the other threads passing the signal they originally reported had
			for(auto &other_thread : threads()) {
				if(core_->waited_threads_.contains(other_thread->tid())) {	
					other_thread->resume();
				}
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: step
// Desc: steps the currently active thread
//------------------------------------------------------------------------------
void PlatformProcess::step(edb::EVENT_STATUS status) {
	// TODO: assert that we are paused
	Q_ASSERT(core_->process_ == this);

	if(status != edb::DEBUG_STOP) {			
		if(IThread::pointer thread = current_thread()) {
			thread->step(status);
		}
	}
}

}

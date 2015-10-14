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
#include "edb.h"
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <boost/functional/hash.hpp>
#include <fstream>
#include <sys/mman.h>
#include <sys/ptrace.h>

namespace DebuggerCore {
namespace {

#ifdef EDB_X86_64
#define EDB_WORDSIZE sizeof(quint64)
#elif defined(EDB_X86)
#define EDB_WORDSIZE sizeof(quint32)
#endif

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
std::size_t PlatformProcess::read_bytes(edb::address_t address, void* buf, std::size_t len) {
	quint64 read = 0;

	Q_ASSERT(buf);
	
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
	
	if(len != 0) {
	
		// small writes take the fast path
		if(len == 1) {
			bool ok;
			write_byte(address, *reinterpret_cast<const char *>(buf), &ok);
			return ok ? 1 : 0;		
		}
	
		QFile memory_file(QString("/proc/%1/mem").arg(pid_));
		if(memory_file.open(QIODevice::WriteOnly)) {

			memory_file.seek(address);
			written = memory_file.write(reinterpret_cast<const char *>(buf), len);
			if(written == 0 || written == quint64(-1)) {
				return 0;
			}

			memory_file.close();
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
std::size_t PlatformProcess::read_pages(edb::address_t address, void *buf, std::size_t count) {
	Q_ASSERT(buf);
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
quint8 PlatformProcess::read_byte(edb::address_t address, bool *ok) {
	// TODO(eteran): assert that we are paused

	Q_ASSERT(ok);

	*ok = false;

	// if this spot is unreadable, then just return 0xff, otherwise
	// continue as normal.

	// core_->page_size() - 1 will always be 0xf* because pagesizes
	// are always 0x10*, so the masking works
	// range of a is [1..n] where n=pagesize, and we have to adjust
	// if a < wordsize
	const edb::address_t a = core_->page_size() - (address & (core_->page_size() - 1));

	if(a < EDB_WORDSIZE) {
		address -= (EDB_WORDSIZE - a); // LE + BE
	}

	long value = read_data(address, ok);

	if(*ok) {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
		if(a < EDB_WORDSIZE) {
			value >>= CHAR_BIT * (EDB_WORDSIZE - a); // LE
		}
#else
		if(a < EDB_WORDSIZE) {
			value >>= CHAR_BIT * (a - 1);            // BE
		} else {
			value >>= CHAR_BIT * (EDB_WORDSIZE - 1); // BE
		}
#endif
		return value & 0xff;
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

	*ok = false;

	long v;
	long mask;
	// core_->page_size() - 1 will always be 0xf* because pagesizes
	// are always 0x10*, so the masking works
	// range of a is [1..n] where n=pagesize, and we have to adjust
	// if a < wordsize
	const edb::address_t a = core_->page_size() - (address & (core_->page_size() - 1));

	v = value;
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
	if(a < EDB_WORDSIZE) {
		address -= (EDB_WORDSIZE - a);                       // LE + BE
		mask = ~(0xffUL << (CHAR_BIT * (EDB_WORDSIZE - a))); // LE
		v <<= CHAR_BIT * (EDB_WORDSIZE - a);                 // LE
	} else {
		mask = ~0xffUL; // LE
	}
#else /* BIG ENDIAN */
	if(a < EDB_WORDSIZE) {
		address -= (EDB_WORDSIZE - a);            // LE + BE
		mask = ~(0xffUL << (CHAR_BIT * (a - 1))); // BE
		v <<= CHAR_BIT * (a - 1);                 // BE
	} else {
		mask = ~(0xffUL << (CHAR_BIT * (EDB_WORDSIZE - 1))); // BE
		v <<= CHAR_BIT * (EDB_WORDSIZE - 1);                 // BE
	}
#endif

	v |= (read_data(address, ok) & mask);
	if(*ok) {
		*ok = write_data(address, v);
	}
}

//------------------------------------------------------------------------------
// Name: read_data
// Desc:
// Note: this will fail on newer versions of linux if called from a
//       different thread than the one which attached to process
//------------------------------------------------------------------------------
long PlatformProcess::read_data(edb::address_t address, bool *ok) {

	Q_ASSERT(ok);

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
	if(EDB_IS_32_BIT && address>0xffffffffULL) {
		// 32 bit ptrace can't handle such long addresses
		QFile memory_file(QString("/proc/%1/mem").arg(pid_));
		if(memory_file.open(QIODevice::WriteOnly)) {

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
	auto it = core_->threads_.find(core_->active_thread_);
	if(it != core_->threads_.end()) {
		return it.value();
	}
	return IThread::pointer();
}

}

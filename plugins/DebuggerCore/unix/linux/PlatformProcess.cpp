
#include "PlatformProcess.h"
#include "DebuggerCore.h"
#include "PlatformRegion.h"
#include "edb.h"
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <sys/mman.h>

using namespace DebuggerCore;

namespace {

#ifdef EDB_X86_64
#define EDB_WORDSIZE sizeof(quint64)
#elif defined(EDB_X86)
#define EDB_WORDSIZE sizeof(quint32)
#endif


struct user_stat {
/* 01 */ int pid;
/* 02 */ char comm[256];
/* 03 */ char state;
/* 04 */ int ppid;
/* 05 */ int pgrp;
/* 06 */ int session;
/* 07 */ int tty_nr;
/* 08 */ int tpgid;
/* 09 */ unsigned flags;
/* 10 */ unsigned long minflt;
/* 11 */ unsigned long cminflt;
/* 12 */ unsigned long majflt;
/* 13 */ unsigned long cmajflt;
/* 14 */ unsigned long utime;
/* 15 */ unsigned long stime;
/* 16 */ long cutime;
/* 17 */ long cstime;
/* 18 */ long priority;
/* 19 */ long nice;
/* 20 */ long num_threads;
/* 21 */ long itrealvalue;
/* 22 */ unsigned long long starttime;
/* 23 */ unsigned long vsize;
/* 24 */ long rss;
/* 25 */ unsigned long rsslim;
/* 26 */ unsigned long startcode;
/* 27 */ unsigned long endcode;
/* 28 */ unsigned long startstack;
/* 29 */ unsigned long kstkesp;
/* 30 */ unsigned long kstkeip;
/* 31 */ unsigned long signal;
/* 32 */ unsigned long blocked;
/* 33 */ unsigned long sigignore;
/* 34 */ unsigned long sigcatch;
/* 35 */ unsigned long wchan;
/* 36 */ unsigned long nswap;
/* 37 */ unsigned long cnswap;
/* 38 */ int exit_signal;
/* 39 */ int processor;
/* 40 */ unsigned rt_priority;
/* 41 */ unsigned policy;

// Linux 2.6.18
/* 42 */ unsigned long long delayacct_blkio_ticks;

// Linux 2.6.24
/* 43 */ unsigned long guest_time;
/* 44 */ long cguest_time;

// Linux 3.3
/* 45 */ unsigned long start_data;
/* 46 */ unsigned long end_data;
/* 47 */ unsigned long start_brk;

// Linux 3.5
/* 48 */ unsigned long arg_start;
/* 49 */ unsigned long arg_end;
/* 50 */ unsigned long env_start;
/* 51 */ unsigned long env_end;
/* 52 */ int exit_code;
};

//------------------------------------------------------------------------------
// Name: get_user_stat
// Desc: gets the contents of /proc/<pid>/stat and returns the number of elements
//       successfully parsed
//------------------------------------------------------------------------------
int get_user_stat(const QString &path, struct user_stat *user_stat) {
	Q_ASSERT(user_stat);

	int r = -1;
	QFile file(path);
	if(file.open(QIODevice::ReadOnly)) {
		QTextStream in(&file);
		const QString line = in.readLine();
		if(!line.isNull()) {
			char ch;
			r = sscanf(qPrintable(line), "%d %c%255[0-9a-zA-Z_ #~/-]%c %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu %ld %ld %ld %ld %ld %ld %llu %lu %ld %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %d %d %u %u %llu %lu %ld",
					&user_stat->pid,
					&ch, // consume the (
					user_stat->comm,
					&ch, // consume the )
					&user_stat->state,
					&user_stat->ppid,
					&user_stat->pgrp,
					&user_stat->session,
					&user_stat->tty_nr,
					&user_stat->tpgid,
					&user_stat->flags,
					&user_stat->minflt,
					&user_stat->cminflt,
					&user_stat->majflt,
					&user_stat->cmajflt,
					&user_stat->utime,
					&user_stat->stime,
					&user_stat->cutime,
					&user_stat->cstime,
					&user_stat->priority,
					&user_stat->nice,
					&user_stat->num_threads,
					&user_stat->itrealvalue,
					&user_stat->starttime,
					&user_stat->vsize,
					&user_stat->rss,
					&user_stat->rsslim,
					&user_stat->startcode,
					&user_stat->endcode,
					&user_stat->startstack,
					&user_stat->kstkesp,
					&user_stat->kstkeip,
					&user_stat->signal,
					&user_stat->blocked,
					&user_stat->sigignore,
					&user_stat->sigcatch,
					&user_stat->wchan,
					&user_stat->nswap,
					&user_stat->cnswap,
					&user_stat->exit_signal,
					&user_stat->processor,
					&user_stat->rt_priority,
					&user_stat->policy,
					&user_stat->delayacct_blkio_ticks,
					&user_stat->guest_time,
					&user_stat->cguest_time);
		}
		file.close();
	}

	return r;
}

//------------------------------------------------------------------------------
// Name: get_user_stat
// Desc: gets the contents of /proc/<pid>/stat and returns the number of elements
//       successfully parsed
//------------------------------------------------------------------------------
int get_user_stat(edb::pid_t pid, struct user_stat *user_stat) {

	return get_user_stat(QString("/proc/%1/stat").arg(pid), user_stat);
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
// Name: read_pages
// Desc: reads <count> pages from the process starting at <address>
// Note: buf's size must be >= count * core_->page_size()
// Note: address should be page aligned.
//------------------------------------------------------------------------------
bool PlatformProcess::read_pages(edb::address_t address, void *buf, std::size_t count) {

	Q_ASSERT(buf);

	if((address & (core_->page_size() - 1)) == 0) {
	
		const edb::address_t orig_address = address;
		auto ptr                          = reinterpret_cast<long *>(buf);
		auto orig_ptr                     = reinterpret_cast<quint8 *>(buf);

		const edb::address_t end_address  = orig_address + core_->page_size() * count;

		for(std::size_t c = 0; c < count; ++c) {
			for(edb::address_t i = 0; i < core_->page_size(); i += EDB_WORDSIZE) {
				bool ok;
				const long v = core_->read_data(address, &ok);
				if(!ok) {
					return false;
				}

				*ptr++ = v;
				address += EDB_WORDSIZE;
			}
		}

		for(const IBreakpoint::pointer &bp: core_->breakpoints_) {
			if(bp->address() >= orig_address && bp->address() < end_address) {
				// show the original bytes in the buffer..
				orig_ptr[bp->address() - orig_address] = bp->original_byte();
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------------
// Name: read_bytes
// Desc: reads <len> bytes into <buf> starting at <address>
// Note: if the read failed, the part of the buffer that could not be read will
//       be filled with 0xff bytes
//------------------------------------------------------------------------------
bool PlatformProcess::read_bytes(edb::address_t address, void *buf, std::size_t len) {

	Q_ASSERT(buf);

	if(len != 0) {
		bool ok;
		auto p = reinterpret_cast<quint8 *>(buf);
		quint8 ch = read_byte(address, &ok);

		while(ok && len) {
			*p++ = ch;
			if(--len) {
				++address;
				ch = read_byte(address, &ok);
			}
		}

		if(!ok) {
			while(len--) {
				*p++ = 0xff;
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------------
// Name: write_bytes
// Desc: writes <len> bytes from <buf> starting at <address>
//------------------------------------------------------------------------------
bool PlatformProcess::write_bytes(edb::address_t address, const void *buf, std::size_t len) {

	Q_ASSERT(buf);

	bool ok = false;

	auto p = reinterpret_cast<const quint8 *>(buf);

	while(len--) {
		write_byte(address++, *p++, &ok);
		if(!ok) {
			break;
		}
	}

	return ok;
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

	v |= (core_->read_data(address, ok) & mask);
	if(*ok) {
		*ok = core_->write_data(address, v);
	}

}

//------------------------------------------------------------------------------
// Name: read_byte
// Desc: reads a single bytes at a given address
//------------------------------------------------------------------------------
quint8 PlatformProcess::read_byte(edb::address_t address, bool *ok) {

	const quint8 ret = read_byte_base(address, ok);

	if(ok) {
		if(const IBreakpoint::pointer bp = core_->find_breakpoint(address)) {
			return bp->original_byte();
		}
	}

	return ret;
}

//------------------------------------------------------------------------------
// Name: read_byte_base
// Desc: the base implementation of reading a byte
//------------------------------------------------------------------------------
quint8 PlatformProcess::read_byte_base(edb::address_t address, bool *ok) {
	// TODO(eteran): assert that we are paused

	Q_ASSERT(ok);

	*ok = false;
	errno = -1;

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

	long value = core_->read_data(address, ok);

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
	int n = get_user_stat(pid(), &user_stat);
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
	int n = get_user_stat(pid(), &user_stat);
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
	QList<IRegion::pointer> regions;

	const QString map_file(QString("/proc/%1/maps").arg(pid_));

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

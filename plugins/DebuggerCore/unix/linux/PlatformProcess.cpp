
#include "PlatformProcess.h"
#include "DebuggerCore.h"

using namespace DebuggerCore;

namespace {

#ifdef EDB_X86_64
#define EDB_WORDSIZE sizeof(quint64)
#elif defined(EDB_X86)
#define EDB_WORDSIZE sizeof(quint32)
#endif

}

PlatformProcess::PlatformProcess(DebuggerCore *core, edb::pid_t pid) : core_(core), pid_(pid) {

}

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
		long *ptr                         = reinterpret_cast<long *>(buf);
		quint8 *const orig_ptr            = reinterpret_cast<quint8 *>(buf);

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

		Q_FOREACH(const IBreakpoint::pointer &bp, core_->breakpoints_) {
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
		quint8 *p = reinterpret_cast<quint8 *>(buf);
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

	const quint8 *p = reinterpret_cast<const quint8 *>(buf);

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

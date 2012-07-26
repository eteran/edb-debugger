/*
Copyright (C) 2006 - 2011 Evan Teran
                          eteran@alum.rit.edu

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

#include "MemoryRegions.h"
#include "SymbolManager.h"
#include "IDebuggerCore.h"
#include "Util.h"
#include "State.h"
#include "Debugger.h"

#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include <QtGlobal>
#include <QApplication>
#include <QDebug>
#include <QStringList>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/exec.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/lock.h>

#include <kvm.h>
#include <uvm/uvm.h>

#include <fcntl.h>
#include <unistd.h>

#if 0
/*
 * Download vmmap_entries from the kernel into our address space.
 * We fix up the addr tree while downloading.
 *
 * Returns: the size of the tree on success, or -1 on failure.
 * On failure, *rptr needs to be passed to unload_vmmap_entries to free
 * the lot.
 */
ssize_t load_vmmap_entries(kvm_t *kd, u_long kptr, struct vm_map_entry **rptr, struct vm_map_entry *parent) {
	struct vm_map_entry *entry;
	u_long left_kptr;
	u_long right_kptr;
	ssize_t left_sz;
	ssize_t right_sz;

	if (kptr == 0)
		return 0;

	/* Need space. */
	entry = (struct vm_map_entry *)malloc(sizeof(*entry));
	if (entry == NULL)
		return -1;

	/* Download entry at kptr. */
	if (!kvm_read(kd, kptr, (char *)entry, sizeof(*entry))) {
		free(entry);
		return -1;
	}

	/*
	 * Update addr pointers to have sane values in this address space.
	 * We save the kernel pointers in {left,right}_kptr, so we have them
	 * available to download children.
	 */
	left_kptr = (u_long) RB_LEFT(entry, daddrs.addr_entry);
	right_kptr = (u_long) RB_RIGHT(entry, daddrs.addr_entry);
	RB_LEFT(entry, daddrs.addr_entry) =
	    RB_RIGHT(entry, daddrs.addr_entry) = NULL;
	/* Fill in parent pointer. */
	RB_PARENT(entry, daddrs.addr_entry) = parent;

	/*
	 * Consistent state reached, fill in *rptr.
	 */
	*rptr = entry;

	/*
	 * Download left, right.
	 * On failure, our map is in a state that can be handled by
	 * unload_vmmap_entries.
	 */
	left_sz = load_vmmap_entries(kd, left_kptr, &RB_LEFT(entry, daddrs.addr_entry), entry);
	if (left_sz == -1)
		return -1;
	right_sz = load_vmmap_entries(kd, right_kptr, &RB_RIGHT(entry, daddrs.addr_entry), entry);
	if (right_sz == -1)
		return -1;

	return 1 + left_sz + right_sz;
}

/*
 * Free the vmmap entries in the given tree.
 */
void
unload_vmmap_entries(struct vm_map_entry *entry)
{
	if (entry == NULL)
		return;

	unload_vmmap_entries(RB_LEFT(entry, daddrs.addr_entry));
	unload_vmmap_entries(RB_RIGHT(entry, daddrs.addr_entry));
	free(entry);
}

/*
 * Don't implement address comparison.
 */
static __inline int
no_impl(void *p, void *q)
{
	abort(); /* Should not be called. */
	return 0;
}

RB_GENERATE(uvm_map_addr, vm_map_entry, daddrs.addr_entry, no_impl)
#endif

//------------------------------------------------------------------------------
// Name: MemoryRegions()
// Desc: constructor
//------------------------------------------------------------------------------
MemoryRegions::MemoryRegions() : QAbstractItemModel(0), pid_(0) {

}

//------------------------------------------------------------------------------
// Name: ~MemoryRegions()
// Desc: destructor
//------------------------------------------------------------------------------
MemoryRegions::~MemoryRegions() {
}

//------------------------------------------------------------------------------
// Name: set_pid(edb::pid_t pid)
// Desc:
//------------------------------------------------------------------------------
void MemoryRegions::set_pid(edb::pid_t pid) {
	pid_ = pid;
	regions_.clear();
	sync();
}

//------------------------------------------------------------------------------
// Name: clear()
// Desc:
//------------------------------------------------------------------------------
void MemoryRegions::clear() {
	pid_ = 0;
	regions_.clear();
}

//------------------------------------------------------------------------------
// Name: sync()
// Desc: reads a memory map using the kvm api
//------------------------------------------------------------------------------
void MemoryRegions::sync() {

	QList<MemoryRegion> regions;

	if(pid_ != 0) {

		char err_buf[_POSIX2_LINE_MAX];
		kvm_t *const kd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, err_buf);
		if(kd != 0) {
			int rc;
			struct kinfo_proc *const proc = kvm_getprocs(kd, KERN_PROC_PID, pid_, sizeof *proc, &rc);
			Q_ASSERT(proc != 0);

			struct vmspace vmsp;
			struct vm_map_entry e;
			
			kvm_read(kd, proc->p_vmspace, &vmsp, sizeof(vmsp));
			if(vmsp.vm_map.header.next != 0) {
				kvm_read(kd, (u_long)vmsp.vm_map.header.next, &e, sizeof(e));
				while(e.next != vmsp.vm_map.header.next) {
				
					const edb::address_t start               = e.start;
					const edb::address_t end                 = e.end;
					const edb::address_t base                = e.offset;
					const QString name                       = QString();
					const IRegion::permissions_t permissions = 
						((e.protection & VM_PROT_READ)    ? PROT_READ  : 0) |
						((e.protection & VM_PROT_WRITE)   ? PROT_WRITE : 0) |
						((e.protection & VM_PROT_EXECUTE) ? PROT_EXEC  : 0);

					regions.push_back(MemoryRegion(start, end, base, name, permissions));
					kvm_read(kd, (u_long)e.next, &e, sizeof(e));
				}
			}
			
			
			
#if 0
			uvm_map_addr root;

			kvm_read(kd, proc->p_vmspace, &vmsp, sizeof vmsp);

			RB_INIT(&root);
			if (load_vmmap_entries(kd, 
			    (u_long)RB_ROOT(&vmsp.vm_map.addr),
			    &RB_ROOT(&root), NULL) == -1)
				goto do_unload;

			RB_FOREACH(e, uvm_map_addr, &root) {
				MemoryRegion region;
				region.start        = e->start;
				region.end          = e->end;
				region.base         = e->offset;
				region.name         = QString();
				region.permissions_ =
					((e->protection & VM_PROT_READ)    ? PROT_READ  : 0) |
					((e->protection & VM_PROT_WRITE)   ? PROT_WRITE : 0) |
					((e->protection & VM_PROT_EXECUTE) ? PROT_EXEC  : 0);

				regions.push_back(region);
			}
			
do_unload:
			unload_vmmap_entries(RB_ROOT(&root));
#endif
			kvm_close(kd);
		} else {
			fprintf(stderr, "sync: %s\n", err_buf);
			return;
		}
	}

	qSwap(regions_, regions);
	reset();
}

//------------------------------------------------------------------------------
// Name: find_region(edb::address_t address) const
// Desc:
//------------------------------------------------------------------------------
bool MemoryRegions::find_region(edb::address_t address) const {
	Q_FOREACH(const MemoryRegion &i, regions_) {
		if(i.contains(address)) {
			return true;
		}
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: find_region(edb::address_t address, MemoryRegion &region) const
// Desc:
//------------------------------------------------------------------------------
bool MemoryRegions::find_region(edb::address_t address, MemoryRegion &region) const {
	Q_FOREACH(const MemoryRegion &i, regions_) {
		if(i.contains(address)) {
			region = i;
			return true;
		}
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: data(const QModelIndex &index, int role) const
// Desc:
//------------------------------------------------------------------------------
QVariant MemoryRegions::data(const QModelIndex &index, int role) const {

	if(index.isValid() && role == Qt::DisplayRole) {

		const MemoryRegion &region = regions_[index.row()];

		switch(index.column()) {
		case 0:
			return edb::v1::format_pointer(region.start());
		case 1:
			return edb::v1::format_pointer(region.end());
		case 2:
			return QString("%1%2%3").arg(region.readable() ? 'r' : '-').arg(region.writable() ? 'w' : '-').arg(region.executable() ? 'x' : '-' );
		case 3:
			return region.name();
		}
	}

	return QVariant();
}

//------------------------------------------------------------------------------
// Name: index(int row, int column, const QModelIndex &parent) const
// Desc:
//------------------------------------------------------------------------------
QModelIndex MemoryRegions::index(int row, int column, const QModelIndex &parent) const {
	Q_UNUSED(parent);

	if(row >= rowCount(parent) || column >= columnCount(parent)) {
		return QModelIndex();
	}

	return createIndex(row, column, const_cast<MemoryRegion *>(&regions_[row]));
}

//------------------------------------------------------------------------------
// Name: parent(const QModelIndex &index) const
// Desc:
//------------------------------------------------------------------------------
QModelIndex MemoryRegions::parent(const QModelIndex &index) const {
	Q_UNUSED(index);
	return QModelIndex();
}

//------------------------------------------------------------------------------
// Name: rowCount(const QModelIndex &parent) const
// Desc:
//------------------------------------------------------------------------------
int MemoryRegions::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent);
	return regions_.size();
}

//------------------------------------------------------------------------------
// Name: columnCount(const QModelIndex &parent) const
// Desc:
//------------------------------------------------------------------------------
int MemoryRegions::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent);
	return 4;
}

//------------------------------------------------------------------------------
// Name: headerData(int section, Qt::Orientation orientation, int role) const
// Desc:
//------------------------------------------------------------------------------
QVariant MemoryRegions::headerData(int section, Qt::Orientation orientation, int role) const {
	if(role == Qt::DisplayRole && orientation == Qt::Horizontal) {
		switch(section) {
		case 0:
			return tr("Start Address");
		case 1:
			return tr("End Address");
		case 2:
			return tr("Permissions");
		case 3:
			return tr("Name");
		}
	}

	return QVariant();
}


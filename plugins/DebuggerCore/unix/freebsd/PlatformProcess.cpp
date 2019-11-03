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
#include <fcntl.h>
#include <kvm.h>
#include <machine/reg.h>
#include <paths.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/ptrace.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

namespace DebuggerCorePlugin {

QString PlatformProcess::executable() const {
	// TODO: implement this
	return QString();
}

QString PlatformProcess::current_working_directory() const {
	// TODO(eteran): implement this
	return QString();
}

QDateTime PlatformProcess::start_time() const {
	// TODO(eteran): implement this
	return QDateTime();
}

QList<Module> PlatformProcess::loaded_modules() const {
	QList<Module> modules;
	// TODO(eteran): implement this
	return modules;
}

edb::address_t PlatformProcess::code_address() const {
	// TODO(eteran): implement this
	return 0;
}

edb::address_t PlatformProcess::data_address() const {
	// TODO(eteran): implement this
	return 0;
}

QList<QByteArray> PlatformProcess::arguments() const {
	QList<QByteArray> ret;
	// TODO(eteran): implement this
	return ret;
}

QList<std::shared_ptr<IRegion>> PlatformProcess::regions() const {
	QList<std::shared_ptr<IRegion>> regions;

	if (pid_ != 0) {
		char buffer[PATH_MAX] = {};
		struct ptrace_vm_entry vm_entry;
		memset(&vm_entry, 0, sizeof(vm_entry));
		vm_entry.pve_entry = 0;

		while (ptrace(PT_VM_ENTRY, pid_, reinterpret_cast<char *>(&vm_entry), NULL) == 0) {
			vm_entry.pve_path    = buffer;
			vm_entry.pve_pathlen = sizeof(buffer);

			const edb::address_t start               = vm_entry.pve_start;
			const edb::address_t end                 = vm_entry.pve_end;
			const edb::address_t base                = vm_entry.pve_start - vm_entry.pve_offset;
			const QString name                       = vm_entry.pve_path;
			const IRegion::permissions_t permissions = vm_entry.pve_prot;

			regions.push_back(std::make_shared<PlatformRegion>(start, end, base, name, permissions));
			memset(buffer, 0, sizeof(buffer));
		}
	}

	return regions;
}

}

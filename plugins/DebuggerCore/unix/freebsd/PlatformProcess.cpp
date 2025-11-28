/*
 * Copyright (C) 2015 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "PlatformProcess.h"
#include <csignal>
#include <fcntl.h>
#include <kvm.h>
#include <machine/reg.h>
#include <paths.h>
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

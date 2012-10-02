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

#include "Debugger.h"
#include "IDebuggerCore.h"
#include "MemoryRegions.h"

#include <QtDebug>
#include <QTextStream>
#include <QFile>

#include <link.h>

//------------------------------------------------------------------------------
// Name: primary_code_region()
// Desc:
//------------------------------------------------------------------------------
EDB_EXPORT MemoryRegion edb::v1::primary_code_region() {

	const QString process_executable = debugger_core->process_exe(debugger_core->pid());
	
	memory_regions().sync();

	const QList<MemoryRegion> r = memory_regions().regions();
	Q_FOREACH(const MemoryRegion &region, r) {
		if(region.executable() && region.name() == process_executable) {
			return region;
		}
	}
	return MemoryRegion();
}

//------------------------------------------------------------------------------
// Name: loaded_libraries()
// Desc:
//------------------------------------------------------------------------------
QList<Module> edb::v1::loaded_libraries() {
	QList<Module> ret;

	if(IBinary *const binary_info = edb::v1::get_binary_info(edb::v1::primary_code_region())) {
		
		struct r_debug dynamic_info;			
		if(binary_info->debug_pointer()) {
			if(edb::v1::debugger_core->read_bytes(binary_info->debug_pointer(), &dynamic_info, sizeof(dynamic_info))) {
				if(dynamic_info.r_map) {

					edb::address_t link_address = (edb::address_t)dynamic_info.r_map;

					while(link_address) {

						struct link_map map;

						if(edb::v1::debugger_core->read_bytes(link_address, &map, sizeof(map))) {
							char path[PATH_MAX];
							if(!edb::v1::debugger_core->read_bytes(reinterpret_cast<edb::address_t>(map.l_name), &path, sizeof(path))) {
								path[0] = '\0';
							}
							
							if(map.l_addr) {
								Module module;
								module.name         = path;
								module.base_address = map.l_addr;							
								ret.push_back(module);
							}

							link_address = (edb::address_t)map.l_next;
						} else {
							break;
						}
					}
				}
			}
		}
		delete binary_info;
	}
	
	// fallback
	if(ret.isEmpty()) {
		const QList<MemoryRegion> r = memory_regions().regions();
		QMap<QString, Module> modules_temp;
		Q_FOREACH(const MemoryRegion &region, r) {
			// modules seem to have full paths
			if(region.name().startsWith("/")) {
				if(!modules_temp.contains(region.name())) {
					Module module;
					module.name         = region.name();
					module.base_address = region.start();
					modules_temp.insert(region.name(), module);
				}
			}
		}
		
		for(QMap<QString, Module>::const_iterator it = modules_temp.begin(); it != modules_temp.end(); ++it) {
			ret.push_back(it.value());
		}
	}

	return ret;
}

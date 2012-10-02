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
	qDebug() << "TODO: implement edb::v1::loaded_libraries";
	return ret;
}

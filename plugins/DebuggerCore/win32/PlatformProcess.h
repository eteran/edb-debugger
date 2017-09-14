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


#ifndef PLATOFORM_PROCESS_20150517_H_
#define PLATOFORM_PROCESS_20150517_H_

#include "IProcess.h"
#include "Module.h"

#include <QDateTime>

namespace DebuggerCorePlugin {

	class PlatformProcess : public IProcess {

	public:
		// legal to call when not attached
		virtual QDateTime                       start_time() const {
			qDebug("TODO: implement PlatformProcess::start_time"); return QDateTime();
		};
		virtual QList<QByteArray>               arguments() const {
			qDebug("TODO: implement PlatformProcess::arguments"); return QList<QByteArray>();
		};
		virtual QString                         current_working_directory() const {
			qDebug("TODO: implement PlatformProcess::current_working_directory"); return "";
		};
		virtual QString                         executable() const {
			qDebug("TODO: implement PlatformProcess::executable"); return "";
		};
		virtual edb::pid_t                      pid() const {
			qDebug("TODO: implement PlatformProcess::pid"); return edb::pid_t();
		};
		virtual std::shared_ptr<IProcess>       parent() const {
			qDebug("TODO: implement PlatformProcess::parent"); return std::shared_ptr<IProcess>();
		};
		virtual edb::address_t                  code_address() const {
			qDebug("TODO: implement PlatformProcess::code_address"); return edb::address_t();
		};
		virtual edb::address_t                  data_address() const {
			qDebug("TODO: implement PlatformProcess::data_address"); return edb::address_t();
		};
		virtual QList<std::shared_ptr<IRegion>> regions() const {
			qDebug("TODO: implement PlatformProcess::regions"); return QList<std::shared_ptr<IRegion>>();
		};
		virtual edb::uid_t                      uid() const {
			qDebug("TODO: implement PlatformProcess::uid"); return edb::uid_t();
		};
		virtual QString                         user() const {
			qDebug("TODO: implement PlatformProcess::user"); return "";
		};
		virtual QString                         name() const {
			qDebug("TODO: implement PlatformProcess::name"); return "";
		};
		virtual QList<Module>                   loaded_modules() const {
			qDebug("TODO: implement PlatformProcess::loaded_modules"); return QList<Module>();
		};

	public:
		// only legal to call when attached
		virtual QList<std::shared_ptr<IThread>>  threads() const {
			qDebug("TODO: implement PlatformProcess::threads"); return QList<std::shared_ptr<IThread>>();
		};
		virtual std::shared_ptr<IThread>         current_thread() const {
			qDebug("TODO: implement PlatformProcess::current_thread"); return std::shared_ptr<IThread>();
		};
		virtual void                             set_current_thread(IThread& thread) {
			qDebug("TODO: implement PlatformProcess::set_current_thread");
		};
		virtual std::size_t                      write_bytes(edb::address_t address, const void *buf, size_t len) {
			qDebug("TODO: implement PlatformProcess::write_bytes"); return 0;
		};
		virtual std::size_t                      patch_bytes(edb::address_t address, const void *buf, size_t len) {
			qDebug("TODO: implement PlatformProcess::patch_bytes"); return 0;
		};
		virtual std::size_t                      read_bytes(edb::address_t address, void *buf, size_t len) const {
			qDebug("TODO: implement PlatformProcess::read_bytes"); return 0;
		};
		virtual std::size_t                      read_pages(edb::address_t address, void *buf, size_t count) const {
			qDebug("TODO: implement PlatformProcess::read_pages"); return 0;
		};
		virtual Status                           pause() {
			qDebug("TODO: implement PlatformProcess::pause"); return Status("Not implemented");
		};
		virtual Status                           resume(edb::EVENT_STATUS status) {
			qDebug("TODO: implement PlatformProcess::resume"); return Status("Not implemented");
		};
		virtual Status                           step(edb::EVENT_STATUS status) {
			qDebug("TODO: implement PlatformProcess::step"); return Status("Not implemented");
		};
		virtual bool                             isPaused() const {
			qDebug("TODO: implement PlatformProcess::isPaused"); return true;
		};
		virtual QMap<edb::address_t, Patch>      patches() const {
			qDebug("TODO: implement PlatformProcess::patches"); return QMap<edb::address_t, Patch>();
		};

	};

}

#endif

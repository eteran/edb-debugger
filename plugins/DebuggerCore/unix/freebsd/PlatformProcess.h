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

#ifndef PLATFORM_PROCESS_H_20150517_
#define PLATFORM_PROCESS_H_20150517_

#include "IProcess.h"

class PlatformProcess : public IProcess {
public:
	// legal to call when not attached
	QDateTime start_time() const override;
	QList<QByteArray> arguments() const override;
	QString current_working_directory() const override;
	QString executable() const override;
	edb::pid_t pid() const override;
	std::shared_ptr<IProcess> parent() const override;
	edb::address_t code_address() const override;
	edb::address_t data_address() const override;
	edb::address_t entry_point() const override;
	QList<std::shared_ptr<IRegion>> regions() const override;
	edb::uid_t uid() const override;
	QString user() const override;
	QString name() const override;
	QList<Module> loaded_modules() const override;

public:
	edb::address_t debug_pointer() const override;
	edb::address_t calculate_main() const override;

public:
	// only legal to call when attached
	QList<std::shared_ptr<IThread>> threads() const override;
	std::shared_ptr<IThread> current_thread() const override;
	void set_current_thread(IThread &thread) override;
	std::size_t write_bytes(edb::address_t address, const void *buf, size_t len) override;
	std::size_t patch_bytes(edb::address_t address, const void *buf, size_t len) override;
	std::size_t read_bytes(edb::address_t address, void *buf, size_t len) const override;
	std::size_t read_pages(edb::address_t address, void *buf, size_t count) const override;
	Status pause() override;
	Status resume(edb::EVENT_STATUS status) override;
	Status step(edb::EVENT_STATUS status) override;
	bool isPaused() const override;
	QMap<edb::address_t, Patch> patches() const override;

private:
	edb::pid_t pid_;
};

#endif

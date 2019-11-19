/*
Copyright (C) 2006 - 2015 Evan Teran
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

#ifndef PLATFORM_REGION_H_20120330_
#define PLATFORM_REGION_H_20120330_

#include "IRegion.h"
#include <QCoreApplication>
#include <QString>

namespace DebuggerCorePlugin {

class PlatformRegion final : public IRegion {
	Q_DECLARE_TR_FUNCTIONS(PlatformRegion)

	template <size_t N>
	friend class BackupInfo;

public:
	PlatformRegion(edb::address_t start, edb::address_t end, edb::address_t base, const QString &name, permissions_t permissions);
	~PlatformRegion() override = default;

public:
	IRegion *clone() const override;

public:
	bool accessible() const override;
	bool readable() const override;
	bool writable() const override;
	bool executable() const override;
	size_t size() const override;

public:
	void setPermissions(bool read, bool write, bool execute) override;
	void setStart(edb::address_t address) override;
	void setEnd(edb::address_t address) override;

public:
	edb::address_t start() const override;
	edb::address_t end() const override;
	edb::address_t base() const override;
	QString name() const override;
	permissions_t permissions() const override;

private:
	void setPermissions(bool read, bool write, bool execute, edb::address_t temp_address);

private:
	edb::address_t start_;
	edb::address_t end_;
	edb::address_t base_;
	QString name_;
	permissions_t permissions_;
};

}

#endif

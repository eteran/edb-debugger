/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
	PlatformRegion(edb::address_t start, edb::address_t end, edb::address_t base, QString name, permissions_t permissions);
	~PlatformRegion() override = default;

public:
	[[nodiscard]] IRegion *clone() const override;

public:
	[[nodiscard]] bool accessible() const override;
	[[nodiscard]] bool readable() const override;
	[[nodiscard]] bool writable() const override;
	[[nodiscard]] bool executable() const override;
	[[nodiscard]] size_t size() const override;

public:
	void setPermissions(bool read, bool write, bool execute) override;
	void setStart(edb::address_t address) override;
	void setEnd(edb::address_t address) override;

public:
	[[nodiscard]] edb::address_t start() const override;
	[[nodiscard]] edb::address_t end() const override;
	[[nodiscard]] edb::address_t base() const override;
	[[nodiscard]] QString name() const override;
	[[nodiscard]] permissions_t permissions() const override;

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

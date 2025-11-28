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

namespace DebuggerCore {

class PlatformRegion : public IRegion {
	Q_DECLARE_TR_FUNCTIONS(PlatformRegion)

public:
	PlatformRegion(edb::address_t start, edb::address_t end, edb::address_t base, QString name, permissions_t permissions);
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
	void set_permissions(bool read, bool write, bool execute) override;
	void set_start(edb::address_t address) override;
	void set_end(edb::address_t address) override;

public:
	edb::address_t start() const override;
	edb::address_t end() const override;
	edb::address_t base() const override;
	QString name() const override;
	permissions_t permissions() const override;

private:
	edb::address_t start_;
	edb::address_t end_;
	edb::address_t base_;
	QString name_;
	permissions_t permissions_;
};

}

#endif

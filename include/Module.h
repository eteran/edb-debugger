/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MODULE_H_20191119_
#define MODULE_H_20191119_

#include "Types.h"
#include <QString>

struct Module {
	QString name;
	edb::address_t baseAddress;
};

#endif

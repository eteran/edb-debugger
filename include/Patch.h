/*
 * Copyright (C) 2017 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PATCH_H_20191119_
#define PATCH_H_20191119_

#include "Types.h"
#include <QByteArray>

struct Patch {
	edb::address_t address;
	QByteArray origBytes;
	QByteArray newBytes;
};

#endif

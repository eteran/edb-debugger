/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PROTOTYPE_H_20070320_
#define PROTOTYPE_H_20070320_

#include <QString>
#include <vector>

namespace edb {

struct Argument {
	QString name;
	QString type;
};

struct Prototype {
	QString name;
	QString type;
	bool noreturn;
	std::vector<Argument> arguments;
};

}

#endif

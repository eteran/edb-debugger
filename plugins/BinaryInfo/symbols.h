/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SYMBOLS_H_20110312_
#define SYMBOLS_H_20110312_

class QString;
#include <iostream>

namespace BinaryInfoPlugin {

bool generate_symbols(const QString &filename, std::ostream &os = std::cout);

}

#endif

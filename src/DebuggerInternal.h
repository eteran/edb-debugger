/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DEBUGGER_INTERNAL_H_20100301_
#define DEBUGGER_INTERNAL_H_20100301_

class QString;
class QObject;

// these are global utility functions which are not part of the exported API

namespace edb::internal {

bool register_plugin(const QString &filename, QObject *plugin);
void load_function_db();

}

#endif

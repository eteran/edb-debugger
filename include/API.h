/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef API_H_20090529_
#define API_H_20090529_

#include <QtGlobal>

#ifdef QT_PLUGIN
#define EDB_EXPORT Q_DECL_IMPORT
#else
#define EDB_EXPORT Q_DECL_EXPORT
#endif

#endif

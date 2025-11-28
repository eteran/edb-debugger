/*
 * Copyright (C) 2016- 2025 Evan Teran
 *                          evan.teran@gmail.com
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FEATURE_DETECT_H_20191119_
#define FEATURE_DETECT_H_20191119_

namespace DebuggerCorePlugin::feature {

bool detect_proc_access(bool *read_broken, bool *write_broken);

}

#endif

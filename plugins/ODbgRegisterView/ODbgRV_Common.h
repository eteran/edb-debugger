/*
 * Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ODBG_REGISTER_VIEW_COMMON_H_20170817_
#define ODBG_REGISTER_VIEW_COMMON_H_20170817_

#include <QKeySequence>

namespace ODbgRegisterView {

constexpr Qt::Key SetToZeroKey = Qt::Key_Z;
constexpr Qt::Key DecrementKey = Qt::Key_Minus;
constexpr Qt::Key IncrementKey = Qt::Key_Plus;

static constexpr const char *GprCategoryName = "General Purpose";

}

#endif

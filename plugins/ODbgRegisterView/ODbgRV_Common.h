/*
Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ODBG_REGISTER_VIEW_COMMON_H_20170817
#define ODBG_REGISTER_VIEW_COMMON_H_20170817

#include <QKeySequence>

namespace ODbgRegisterView {

const QKeySequence CopyFieldShortcut(Qt::CTRL | Qt::Key_C);
constexpr Qt::Key SetToZeroKey = Qt::Key_Z;
constexpr Qt::Key DecrementKey = Qt::Key_Minus;
constexpr Qt::Key IncrementKey = Qt::Key_Plus;

static constexpr const char *GprCategoryName = "General Purpose";

}

#endif

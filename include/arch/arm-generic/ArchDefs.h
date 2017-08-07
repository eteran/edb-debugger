/*
Copyright (C) 2006 - 2015 Evan Teran
                          evan.teran@gmail.com
Copyright (C) 2017 Ruslan Kabatsayev
                   b7.10110111@gmail.com

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

#ifndef ARCH_DEFS_20170807_H_
#define ARCH_DEFS_20170807_H_

#include <cstdint>

#if INTPTR_MAX == INT32_MAX
#define EDB_ARM32
static constexpr bool EDB_IS_64_BIT = false;
static constexpr bool EDB_IS_32_BIT = true;

#elif INTPTR_MAX == INT64_MAX

#define EDB_ARM64
static constexpr bool EDB_IS_64_BIT = true;
static constexpr bool EDB_IS_32_BIT = false;
#endif

#endif

/*
Copyright (C) 2006 - 2015 Evan Teran
                          evan.teran@gmail.com

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

#ifndef OSTYPES_H_20070116_
#define OSTYPES_H_20070116_

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

namespace edb {
using pid_t = DWORD;
using uid_t = DWORD; // TODO(eteran): I think this needs to be an SID to make any sense
using tid_t = DWORD;
}

#endif

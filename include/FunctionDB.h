/*
Copyright (C) 2006 - 2011 Evan Teran
                          eteran@alum.rit.edu

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

#ifndef FUNCTIONDB_20080211_H_
#define FUNCTIONDB_20080211_H_

#include "FunctionInfo.h"
#include "Types.h"
#include <cstring>

class QString;

struct EDB_EXPORT Function {
	const char *name;
	FunctionInfo f;

	bool operator<(const Function &rhs) const  { return std::strcmp(name, rhs.name) < 0; }
	bool operator==(const Function &rhs) const { return std::strcmp(name, rhs.name) == 0; }
	bool operator==(const QString &s) const    { return s == name; }
};

class EDB_EXPORT FunctionDB {
public:
	virtual ~FunctionDB() {}

public:
	virtual const FunctionInfo *find(const QString &name) const = 0;
};

#endif


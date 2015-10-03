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


#ifndef SYMBOL_GENERATOR_20130808_H_
#define SYMBOL_GENERATOR_20130808_H_

#include "API.h"

class QString;

class EDB_EXPORT ISymbolGenerator {
public:
	virtual ~ISymbolGenerator() {}

public:
	virtual bool generate_symbol_file(const QString &filename, const QString &symbol_file) = 0;
};

#endif

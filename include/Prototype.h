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

#ifndef PROTOTYPE_20070320_H_
#define PROTOTYPE_20070320_H_

#include "API.h"
#include <QVector>
#include <QString>

namespace edb {

struct EDB_EXPORT Argument {
	QString name;
	QString type;
};

struct EDB_EXPORT Prototype {
	QString           name;
	QString           type;
	QVector<Argument> arguments;
};

}

#endif


/*
Copyright (C) 2017 - 2017 Ruslan Kabatsayev
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

#ifndef SIMPLE_REG_VIEW_H_20170815
#define SIMPLE_REG_VIEW_H_20170815

#include <QTreeView>
#include "Types.h"

namespace SimpleRegView
{

class RegView : public QTreeView
{
	Q_OBJECT
public:
	RegView(QWidget *parent = 0);
};

}

#endif

/*
Copyright (C) 2017 Ruslan Kabatsayev <b7.10110111@gmail.com>

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

#include "armGroups.h"
#include "ODbgRV_Util.h"

namespace ODbgRegisterView {

RegisterGroup *createCPSR(RegisterViewModelBase::Model *model, QWidget *parent)
{
	const auto catIndex = findModelCategory(model, "General Status");
	if (!catIndex.isValid())
		return nullptr;
	auto nameIndex = findModelRegister(catIndex, "CPSR");
	if (!nameIndex.isValid())
		return nullptr;
	const auto group     = new RegisterGroup("CPS", parent);
	const int   nameWidth = 3;
	int         column    = 0;
	group->insert(0, column, new FieldWidget("CPS", group));
	const auto valueWidth = 8;
	const auto valueIndex = nameIndex.sibling(nameIndex.row(), MODEL_VALUE_COLUMN);
	column += nameWidth + 1;
	group->insert(0, column, new ValueField(valueWidth, valueIndex, group, [](QString const &v) { return v.right(8); }));
	const auto commentIndex = nameIndex.sibling(nameIndex.row(), MODEL_COMMENT_COLUMN);
	column += valueWidth + 1;
	group->insert(0, column, new FieldWidget(0, commentIndex, group));

	return group;
}

}

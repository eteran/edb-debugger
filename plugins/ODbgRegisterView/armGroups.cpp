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
#include <unordered_map>

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

RegisterGroup *createExpandedCPSR(RegisterViewModelBase::Model *model, QWidget *parent) {
	using namespace RegisterViewModelBase;

	const auto catIndex = findModelCategory(model, "General Status");
	if (!catIndex.isValid())
		return nullptr;
	auto regNameIndex = findModelRegister(catIndex, "CPSR");
	if (!regNameIndex.isValid())
		return nullptr;
	const auto   group     = new RegisterGroup(QObject::tr("Expanded CPSR"), parent);
	static const std::unordered_map<char, QString> flagTooltips = {
		{'N', QObject::tr("Negative result flag")},
		{'Z', QObject::tr("Zero result flag")},
		{'C', QObject::tr("Carry flag")},
		{'V', QObject::tr("Overflow flag")},
		{'Q', QObject::tr("Sticky saturation/overflow flag")},
		{'J', QObject::tr("Jazelle state flag")},
		{'E', QObject::tr("Big endian state flag")},
		{'T', QObject::tr("Thumb state flag")},
	};
	// NOTE: NZCV is intended to align with corresponding name/value fields in FPSCR
	for (int row = 0, groupCol = 28; row < model->rowCount(regNameIndex); ++row) {
		const auto flagNameIndex  = model->index(row, MODEL_NAME_COLUMN, regNameIndex);
		const auto flagValueIndex = model->index(row, MODEL_VALUE_COLUMN, regNameIndex);
		const auto flagName       = model->data(flagNameIndex).toString().toUpper();
		if (flagName.length() != 1)
			continue;
		static const int flagNameWidth = 1;
		static const int valueWidth    = 1;
		const char       name          = flagName[0].toLatin1();

		switch (name) {
		case 'N':
		case 'Z':
		case 'C':
		case 'V':
		case 'Q':
		case 'J':
		case 'E':
		case 'T': {
			const auto nameField = new FieldWidget(QChar(name), group);
			group->insert(0, groupCol, nameField);
			const auto valueField = new ValueField(valueWidth, flagValueIndex, group);
			group->insert(1, groupCol, valueField);
			groupCol-=2;

			const auto tooltipStr = flagTooltips.at(name);
			nameField->setToolTip(tooltipStr);
			valueField->setToolTip(tooltipStr);

			break;
		}
		default:
			continue;
		}
	}
	return group;
}
}

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
#include "BitFieldDescription.h"
#include "MultiBitFieldWidget.h"
#include "ODbgRV_Util.h"
#include "QtHelper.h"
#include "ValueField.h"
#include <QDebug>
#include <unordered_map>

namespace ODbgRegisterView {

Q_DECLARE_NAMESPACE_TR(ODbgRegisterView)

namespace {

const BitFieldDescription itBaseCondDescription = {
	2,
	{
		"EQ",
		"HS",
		"MI",
		"VS",
		"HI",
		"GE",
		"GT",
		"AL",
	},
	{
		tr("Set EQ"),
		tr("Set HS"),
		tr("Set MI"),
		tr("Set VS"),
		tr("Set HI"),
		tr("Set GE"),
		tr("Set GT"),
		tr("Set AL"),
	},
};

const BitFieldDescription fpscrSTRDescription = {
	3,
	{
		" 1 ",
		"D=1",
		"D=2",
		" 2 ",
	},
	{
		tr("Set stride to 1"),
		"",
		"",
		tr("Set stride to 2"),
	},
};

const BitFieldDescription fpscrLENDescription = {
	1,
	{
		// FPSCR[18:16] = LEN-1, while we want to show LEN value itself
		"1",
		"2",
		"3",
		"4",
		"5",
		"6",
		"7",
		"8",
	},
	{
		// FIXME: this is ugly. Maybe edit it as a number?
		tr("Set LEN to 1"),
		tr("Set LEN to 2"),
		tr("Set LEN to 3"),
		tr("Set LEN to 4"),
		tr("Set LEN to 5"),
		tr("Set LEN to 6"),
		tr("Set LEN to 7"),
		tr("Set LEN to 8"),
	},
};

const BitFieldDescription roundControlDescription = {
	4,
	{
		"NEAR",
		"DOWN",
		"  UP",
		"ZERO",
	},
	{
		tr("Round to nearest"),
		tr("Round down"),
		tr("Round up"),
		tr("Round toward zero"),
	},
};
}

RegisterGroup *createCPSR(RegisterViewModelBase::Model *model, QWidget *parent) {
	const auto catIndex = find_model_category(model, "General Status");
	if (!catIndex.isValid())
		return nullptr;
	auto nameIndex = find_model_register(catIndex, "CPSR");
	if (!nameIndex.isValid())
		return nullptr;
	const auto group    = new RegisterGroup("CPS", parent);
	const int nameWidth = 3;
	int column          = 0;
	group->insert(0, column, new FieldWidget("CPS", group));
	const auto valueWidth = 8;
	const auto valueIndex = nameIndex.sibling(nameIndex.row(), ModelValueColumn);
	column += nameWidth + 1;
	group->insert(0, column, new ValueField(
								 valueWidth, valueIndex, [](QString const &v) { return v.right(8); }, group));
	const auto commentIndex = nameIndex.sibling(nameIndex.row(), ModelCommentColumn);
	column += valueWidth + 1;
	group->insert(0, column, new FieldWidget(0, commentIndex, group));

	return group;
}

RegisterGroup *createExpandedCPSR(RegisterViewModelBase::Model *model, QWidget *parent) {
	using namespace RegisterViewModelBase;

	const auto catIndex = find_model_category(model, "General Status");
	if (!catIndex.isValid())
		return nullptr;
	auto regNameIndex = find_model_register(catIndex, "CPSR");
	if (!regNameIndex.isValid())
		return nullptr;
	const auto group                                            = new RegisterGroup(tr("Expanded CPSR"), parent);
	static const std::unordered_map<char, QString> flagTooltips = {
		{'N', tr("Negative result flag")},
		{'Z', tr("Zero result flag")},
		{'C', tr("Carry flag")},
		{'V', tr("Overflow flag")},
		{'Q', tr("Sticky saturation/overflow flag")},
		{'J', tr("Jazelle state flag")},
		{'E', tr("Big endian state flag")},
		{'T', tr("Thumb state flag")},
	};
	// NOTE: NZCV is intended to align with corresponding name/value fields in FPSCR
	for (int row = 0, groupCol = 28; row < model->rowCount(regNameIndex); ++row) {
		const auto flagNameIndex  = model->index(row, ModelNameColumn, regNameIndex);
		const auto flagValueIndex = model->index(row, ModelValueColumn, regNameIndex);
		const auto flagName       = model->data(flagNameIndex).toString().toUpper();
		if (flagName.length() != 1)
			continue;
		static const int flagNameWidth = 1;
		static const int valueWidth    = 1;
		const char name                = flagName[0].toLatin1();

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
			groupCol -= 2;

			const auto tooltipStr = flagTooltips.at(name);
			nameField->setToolTip(tooltipStr);
			valueField->setToolTip(tooltipStr);

			break;
		}
		default:
			continue;
		}
	}
	{
		const auto geNameField = new FieldWidget(QLatin1String("GE"), group);
		geNameField->setToolTip(tr("Greater than or Equal flags"));
		group->insert(1, 0, geNameField);
		for (int geIndex = 3; geIndex > -1; --geIndex) {
			const int groupCol    = 5 + 2 * (3 - geIndex);
			const auto tooltipStr = QString("GE%1").arg(geIndex);
			{
				const auto nameField = new FieldWidget(QString::number(geIndex), group);
				group->insert(0, groupCol, nameField);
				nameField->setToolTip(tooltipStr);
			}
			const auto indexInModel = find_model_register(regNameIndex, QString("GE%1").arg(geIndex));
			if (!indexInModel.isValid())
				continue;
			const auto valueIndex = indexInModel.sibling(indexInModel.row(), ModelValueColumn);
			if (!valueIndex.isValid())
				continue;
			const auto valueField = new ValueField(1, valueIndex, group);
			group->insert(1, groupCol, valueField);
			valueField->setToolTip(tooltipStr);
		}
	}
	{
		int column = 0;
		enum { labelRow = 2,
			   valueRow };
		{
			const auto itNameField = new FieldWidget(QLatin1String("IT"), group);
			itNameField->setToolTip(tr("If-Then block state"));
			group->insert(valueRow, column, itNameField);
			column += 3;
		}
		{
			// Using textual names for instructions numbering to avoid confusion between base-0 and base-1 counting
			static const QString tooltips[] =
				{
					tr("Lowest bit of IT-block condition for first instruction"),
					tr("Lowest bit of IT-block condition for second instruction"),
					tr("Lowest bit of IT-block condition for third instruction"),
					tr("Lowest bit of IT-block condition for fourth instruction"),
					tr("Flag marking active four-instruction IT-block"),
				};
			for (int i = 4; i >= 0; --i, column += 2) {
				const auto nameIndex  = find_model_register(regNameIndex, QString("IT%1").arg(i));
				const auto valueIndex = nameIndex.sibling(nameIndex.row(), ModelValueColumn);
				if (!valueIndex.isValid())
					continue;
				const auto valueField = new ValueField(1, valueIndex, group);
				group->insert(valueRow, column, valueField);
				const auto tooltip = tooltips[4 - i];
				valueField->setToolTip(tooltip);
				const auto nameField = new FieldWidget(QString::number(i), group);
				group->insert(labelRow, column, nameField);
				nameField->setToolTip(tooltip);
			}
		}
		{
			const auto itBaseCondNameIndex  = find_model_register(regNameIndex, QString("ITbcond").toUpper());
			const auto itBaseCondValueIndex = itBaseCondNameIndex.sibling(itBaseCondNameIndex.row(), ModelValueColumn);
			if (itBaseCondValueIndex.isValid()) {
				const auto itBaseCondField = new MultiBitFieldWidget(itBaseCondValueIndex, itBaseCondDescription, group);
				group->insert(valueRow, column, itBaseCondField);
				const auto tooltip = tr("IT base condition");
				itBaseCondField->setToolTip(tooltip);
				const auto labelField = new FieldWidget("BC", group);
				group->insert(labelRow, column, labelField);
				labelField->setToolTip(tooltip);
			} else
				qWarning() << "Failed to find IT base condition index in the model";
			column += 3;
		}
	}
	return group;
}

void addDXUOZI(RegisterGroup *const group, QModelIndex const &fpscrIndex, int const startRow, int const startColumn) {

	static const QString exceptions = "DXUOZI";

	static const std::unordered_map<char, QPair<QString, QString>> excNames = {
		{'D', {"ID", tr("Input Denormal")}},
		{'X', {"IX", tr("Inexact")}},
		{'U', {"UF", tr("Underflow")}},
		{'O', {"OF", tr("Overflow")}},
		{'Z', {"DZ", tr("Zero Divide")}},
		{'I', {"IO", tr("Invalid Operation")}},
	};

	for (int exN = 0; exN < exceptions.length(); ++exN) {
		const QString ex          = exceptions[exN];
		const auto excAbbrevStart = excNames.at(ex[0].toLatin1()).first;
		const auto exAbbrev       = excAbbrevStart + "C";
		const auto enabAbbrev     = excAbbrevStart + "E";
		const auto excIndex       = valid_index(find_model_register(fpscrIndex, exAbbrev));
		const auto enabIndex      = valid_index(find_model_register(fpscrIndex, enabAbbrev));
		const int column          = startColumn + exN * 2;
		const auto nameField      = new FieldWidget(ex, group);
		group->insert(startRow, column, nameField);
		const auto excValueField = new ValueField(1, value_index(excIndex), group);
		group->insert(startRow + 1, column, excValueField);
		const auto enabValueField = new ValueField(1, value_index(enabIndex), group);
		group->insert(startRow + 2, column, enabValueField);

		const auto excName = excNames.at(ex[0].toLatin1()).second;
		nameField->setToolTip(excName);
		excValueField->setToolTip(excName + ' ' + tr("Exception flag") + " (" + exAbbrev + ")");
		enabValueField->setToolTip(excName + ' ' + tr("Exception Enable flag") + " (" + enabAbbrev + ")");
	}
}

RegisterGroup *createFPSCR(RegisterViewModelBase::Model *model, QWidget *parent) {
	using namespace RegisterViewModelBase;

	const auto catIndex = find_model_category(model, "VFP");
	if (!catIndex.isValid())
		return nullptr;
	const auto group        = new RegisterGroup("FSC", parent);
	const QString fpscrName = "FSC";
	const int fpscrRow = 0, nzcvLabelRow = fpscrRow;
	const int nzcvRow = fpscrRow, nzcvValueRow = nzcvRow + 1;
	int column = 0;

	const auto fpscrLabelField = new FieldWidget(fpscrName, group);
	fpscrLabelField->setToolTip(tr("Floating-point status and control register") + " (FPSCR)");
	group->insert(fpscrRow, column, fpscrLabelField);
	column += fpscrName.length() + 1;
	const auto fpscrIndex      = find_model_register(catIndex, "FPSCR", ModelValueColumn);
	const auto fpscrValueWidth = fpscrIndex.data(Model::FixedLengthRole).toInt();
	assert(fpscrValueWidth > 0);
	group->insert(fpscrRow, column, new ValueField(fpscrValueWidth, fpscrIndex, group));
	column += fpscrValueWidth + 2;

	{
		static const std::unordered_map<char, QString> nzcvDescriptions = {
			{'N', tr("LessThan flag")},
			{'Z', tr("Equal operands flag")},
			{'C', tr("GreaterThen/Equal/Unordered operands flag")},
			{'V', tr("Unordered operands flag")},
		};

		static const QString nzcv = "NZCV";

		for (int i = 0; i < nzcv.length(); ++i) {
			const auto flag      = nzcv[i];
			const auto flagIndex = valid_index(find_model_register(fpscrIndex, flag, ModelValueColumn));
			const auto nameField = new FieldWidget(flag, group);
			group->insert(nzcvRow, column, nameField);
			const auto flagValueField = new ValueField(1, value_index(flagIndex), group);
			group->insert(nzcvValueRow, column, flagValueField);

			const auto descr = nzcvDescriptions.at(flag.toLatin1());
			nameField->setToolTip(descr);
			flagValueField->setToolTip(descr);
			column += 2;
		}
	}

	column += 1;
	const auto excRow = fpscrRow + 1, enabRow = excRow + 1;
	group->insert(excRow, column, new FieldWidget("Err", group));
	group->insert(enabRow, column, new FieldWidget("Enab", group));
	column += 5;
	addDXUOZI(group, fpscrIndex, fpscrRow, column);

	{
		const int DXUOZIWidth = 6 * 2 - 1;
		group->insert(nzcvValueRow, column + DXUOZIWidth + 1, new FieldWidget(0, comment_index(fpscrIndex), group));
	}

	const QString dnName = "DN", fzName = "FZ", strName = "STR", lenName = "LEN";
	{
		column                  = fpscrName.length() - 1;
		const auto strNameField = new FieldWidget(strName, group);
		const auto strRow       = excRow;
		group->insert(strRow, column, strNameField);
		const auto strIndex      = find_model_register(fpscrIndex, "STR", ModelValueColumn);
		const auto strValueField = new MultiBitFieldWidget(strIndex, fpscrSTRDescription, group);
		column += strName.length();
		group->insert(strRow, column, strValueField);
		const auto strTooltip = tr("Stride (distance between successive values in a vector)");
		strNameField->setToolTip(strTooltip);
		strValueField->setToolTip(strTooltip);
		column += 3;
	}
	{
		const auto fzNameField = new FieldWidget(fzName, group);
		const auto fzRow       = excRow;
		group->insert(fzRow, column, fzNameField);
		const auto fzIndex      = find_model_register(fpscrIndex, "FZ", ModelValueColumn);
		const auto fzValueWidth = 1;
		const auto fzValueField = new ValueField(fzValueWidth, fzIndex, group);
		column += fzName.length() + 1;
		group->insert(fzRow, column, fzValueField);
		const auto fzTooltip = tr("Flush Denormals To Zero");
		fzNameField->setToolTip(fzTooltip);
		fzValueField->setToolTip(fzTooltip);
	}
	{
		column                  = fpscrName.length() - 1;
		const auto lenNameField = new FieldWidget(lenName, group);
		const auto lenRow       = enabRow;
		group->insert(lenRow, column, lenNameField);
		const auto lenIndex      = find_model_register(fpscrIndex, "LEN-1", ModelValueColumn);
		const auto lenValueField = new MultiBitFieldWidget(lenIndex, fpscrLENDescription, group);
		column += lenName.length() + 1;
		group->insert(lenRow, column, lenValueField);
		const auto lenTooltip = tr("Number of registers used by each vector");
		lenNameField->setToolTip(lenTooltip);
		lenValueField->setToolTip(lenTooltip);
		column += 2;
	}
	{
		const auto dnNameField = new FieldWidget(dnName, group);
		const auto dnRow       = enabRow;
		group->insert(dnRow, column, dnNameField);
		const auto dnIndex      = find_model_register(fpscrIndex, "DN", ModelValueColumn);
		const auto dnValueWidth = 1;
		const auto dnValueField = new ValueField(dnValueWidth, dnIndex, group);
		column += dnName.length() + 1;
		group->insert(dnRow, column, dnValueField);
		const auto dnTooltip = tr("Enable default NaN mode");
		dnNameField->setToolTip(dnTooltip);
		dnValueField->setToolTip(dnTooltip);
		column += 2;
	}

	{
		column += 1;
		const QString rndName = "Rnd";
		const auto rndRow     = enabRow;
		group->insert(rndRow, column, new FieldWidget(rndName, group));
		column += rndName.length() + 1;
		const auto rndValueField = new MultiBitFieldWidget(
			find_model_register(fpscrIndex, "RC", ModelValueColumn),
			roundControlDescription, group);
		group->insert(rndRow, column, rndValueField);
		rndValueField->setToolTip(tr("Rounding mode"));
	}

	return group;
}

}

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

#include "x86Groups.h"
#include "BitFieldDescription.h"
#include "FpuValueField.h"
#include "MultiBitFieldWidget.h"
#include "ODbgRV_Util.h"
#include "ODbgRV_x86Common.h"
#include "QtHelper.h"
#include "RegisterGroup.h"
#include "VolatileNameField.h"

#include <QDebug>
#include <unordered_map>

namespace ODbgRegisterView {

Q_DECLARE_NAMESPACE_TR(ODbgRegisterView)

namespace {

const BitFieldDescription fpuTagDescription = {
	7,
	{
		tr("valid"),
		tr("zero"),
		tr("special"),
		tr("empty"),
	},
	{
		tr("Tag as used"),
		tr(""),
		tr(""),
		tr("Tag as empty"),
	},
	[](unsigned a, unsigned b) {
		return (a == 3 || b == 3) ? (a == b) : true;
	},
};

const BitFieldDescription roundControlDescription = {
	4,
	{
		tr("NEAR"),
		tr("DOWN"),
		tr("  UP"),
		tr("ZERO"),
	},
	{
		tr("Round to nearest"),
		tr("Round down"),
		tr("Round up"),
		tr("Round toward zero"),
	},
};

const BitFieldDescription precisionControlDescription = {
	2,
	{
		tr("24"),
		tr("??"),
		tr("53"),
		tr("64"),
	},
	{
		tr("Set 24-bit precision"),
		tr(""),
		tr("Set 53-bit precision"),
		tr("Set 64-bit precision"),
	},
};

const BitFieldDescription debugRWDescription = {
	5,
	{
		tr("EXEC"),
		tr("WRITE"),
		tr("  IO"),
		tr(" R/W"),
	},
	{
		tr("Break on execution"),
		tr("Break on data write"),
		tr(""),
		tr("Break on data read/write"),
	},
};

const BitFieldDescription debugLenDescription = {
	1,
	{
		tr("1"),
		tr("2"),
		tr("8"),
		tr("4"),
	},
	{
		tr("Set 1-byte length"),
		tr("Set 2-byte length"),
		tr("Set 8-byte length"),
		tr("Set 4-byte length"),
	},
};

// Checks that FOP is in not in compatibility mode, i.e. is updated only on unmasked exception
// This function would return false for e.g. Pentium III or Atom, but returns true since Pentium 4.
// This can be made return false for such CPUs by setting bit 2 in IA32_MISC_ENABLE MSR.
bool fop_is_compatible() {
#ifdef __GNUG__
	char fenv[28];
	asm volatile("fldz\n"
				 "fstp %%st(0)\n"
				 "fstenv %0\n"
				 : "=m"(fenv)::"%st");
	std::uint16_t fop;
	std::memcpy(&fop, fenv + 18, sizeof(fop));
	return fop == 0;
#else
	// TODO(eteran): figure out a way to implement this for other compilers
	return true;
#endif
}

void add_rounding_mode(RegisterGroup *group, const QModelIndex &index, int row, int column) {
	assert(index.isValid());
	const auto rndValueField = new MultiBitFieldWidget(index, roundControlDescription, group);
	group->insert(row, column, rndValueField);
	rndValueField->setToolTip(tr("Rounding mode"));
}

void add_precision_mode(RegisterGroup *group, const QModelIndex &index, int row, int column) {
	assert(index.isValid());
	const auto precValueField = new MultiBitFieldWidget(index, precisionControlDescription, group);
	group->insert(row, column, precValueField);
	precValueField->setToolTip(tr("Precision mode: effective mantissa length"));
}

void add_puozdi(RegisterGroup *group, const QModelIndex &excRegIndex, const QModelIndex &maskRegIndex, int startRow, int startColumn) {

	static const QString exceptions = tr("PUOZDI");

	static const std::unordered_map<char, QString> excNames = {
		{'P', tr("Precision")},
		{'U', tr("Underflow")},
		{'O', tr("Overflow")},
		{'Z', tr("Zero Divide")},
		{'D', tr("Denormalized Operand")},
		{'I', tr("Invalid Operation")},
	};

	for (int exN = 0; exN < exceptions.length(); ++exN) {
		const QString ex         = exceptions[exN];
		const QString exAbbrev   = ex + "E";
		const QString maskAbbrev = ex + "M";

		const auto excIndex  = valid_index(find_model_register(excRegIndex, exAbbrev));
		const auto maskIndex = valid_index(find_model_register(maskRegIndex, maskAbbrev));

		const int column = startColumn + exN * 2;

		const auto nameField = new FieldWidget(ex, group);
		group->insert(startRow, column, nameField);

		const auto excValueField = new ValueField(1, value_index(excIndex), group);
		group->insert(startRow + 1, column, excValueField);

		const auto maskValueField = new ValueField(1, value_index(maskIndex), group);
		group->insert(startRow + 2, column, maskValueField);

		const QString excName = excNames.at(ex[0].toLatin1());
		nameField->setToolTip(excName);
		excValueField->setToolTip(excName + ' ' + tr("Exception") + " (" + exAbbrev + ")");
		maskValueField->setToolTip(excName + ' ' + tr("Exception Mask") + " (" + maskAbbrev + ")");
	}
}

}

RegisterGroup *create_eflags(RegisterViewModelBase::Model *model, QWidget *parent) {
	const auto catIndex = find_model_category(model, tr("General Status"));
	if (!catIndex.isValid())
		return nullptr;

	auto nameIndex = find_model_register(catIndex, tr("RFLAGS"));
	if (!nameIndex.isValid())
		nameIndex = find_model_register(catIndex, tr("EFLAGS"));

	if (!nameIndex.isValid())
		return nullptr;

	const auto group        = new RegisterGroup(tr("EFL"), parent);
	constexpr int NameWidth = 3;
	int column              = 0;
	group->insert(0, column, new FieldWidget(tr("EFL"), group));

	constexpr int ValueWidth = 8;
	const auto valueIndex    = nameIndex.sibling(nameIndex.row(), ModelValueColumn);
	column += NameWidth + 1;
	group->insert(0, column, new ValueField(
								 ValueWidth, valueIndex, [](const QString &v) {
									 return v.right(8);
								 },
								 group));

	const auto commentIndex = nameIndex.sibling(nameIndex.row(), ModelCommentColumn);
	column += ValueWidth + 1;
	group->insert(0, column, new FieldWidget(0, commentIndex, group));

	return group;
}

RegisterGroup *create_expanded_eflags(RegisterViewModelBase::Model *model, QWidget *parent) {
	const auto catIndex = find_model_category(model, tr("General Status"));
	if (!catIndex.isValid())
		return nullptr;

	auto regNameIndex = find_model_register(catIndex, "RFLAGS");
	if (!regNameIndex.isValid())
		regNameIndex = find_model_register(catIndex, "EFLAGS");

	if (!regNameIndex.isValid())
		return nullptr;

	const auto group = new RegisterGroup(tr("Expanded EFL"), parent);

	static const std::unordered_map<char, QString> flagTooltips = {
		{'C', tr("Carry flag") + " (CF)"},
		{'P', tr("Parity flag") + " (PF)"},
		{'A', tr("Auxiliary carry flag") + " (AF)"},
		{'Z', tr("Zero flag") + " (ZF)"},
		{'S', tr("Sign flag") + " (SF)"},
		{'T', tr("Trap flag") + " (TF)"},
		{'D', tr("Direction flag") + " (DF)"},
		{'O', tr("Overflow flag") + " (OF)"},
	};
	for (int row = 0, groupRow = 0; row < model->rowCount(regNameIndex); ++row) {
		const auto flagNameIndex  = model->index(row, ModelNameColumn, regNameIndex);
		const auto flagValueIndex = model->index(row, ModelValueColumn, regNameIndex);
		const auto flagName       = model->data(flagNameIndex).toString().toUpper();

		if (flagName.length() != 2 || flagName[1] != 'F') {
			continue;
		}

		constexpr int FlagNameWidth = 1;
		constexpr int ValueWidth    = 1;

		const char name = flagName[0].toLatin1();
		switch (name) {
		case 'C':
		case 'P':
		case 'A':
		case 'Z':
		case 'S':
		case 'T':
		case 'D':
		case 'O': {
			const auto nameField = new FieldWidget(QChar(name), group);
			group->insert(groupRow, 0, nameField);

			const auto valueField = new ValueField(ValueWidth, flagValueIndex, group);
			group->insert(groupRow, FlagNameWidth + 1, valueField);

			++groupRow;

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

RegisterGroup *create_fpu_data(RegisterViewModelBase::Model *model, QWidget *parent) {
	using RegisterViewModelBase::Model;

	const auto catIndex = find_model_category(model, "FPU");
	if (!catIndex.isValid()) {
		return nullptr;
	}

	const auto tagsIndex = find_model_register(catIndex, FtrName);
	if (!tagsIndex.isValid()) {
		qWarning() << "Warning: failed to find FTR in the model, refusing to continue making FPUData group";
		return nullptr;
	}

	const auto group          = new RegisterGroup(tr("FPU Data Registers"), parent);
	constexpr int FpuRegCount = 8;
	constexpr int NameWidth   = 3;
	constexpr int TagWidth    = 7;
	const auto fsrIndex       = valid_index(find_model_register(catIndex, FsrName));

	const QPersistentModelIndex topIndex = valid_index(find_model_register(fsrIndex, tr("TOP"), ModelValueColumn));

	for (int row = 0; row < FpuRegCount; ++row) {
		int column           = 0;
		const auto nameIndex = model->index(row, ModelNameColumn, catIndex);
		{
			const auto STiFormatter = [row, topIndex]() {
				const auto topByteArray = topIndex.data(Model::RawValueRole).toByteArray();
				if (topByteArray.isEmpty())
					return QString("R%1").arg(row);
				const auto top = topByteArray[0];
				assert(top >= 0);
				Q_ASSERT(top < 8);
				const auto stI = (row + 8 - top) % 8;
				return QString("ST%1").arg(stI);
			};
			const auto field = new VolatileNameField(NameWidth, STiFormatter, group);
			QObject::connect(model, &RegisterViewModelBase::Model::dataChanged, field, &VolatileNameField::adjustToData);
			group->insert(row, column, field);
			column += NameWidth + 1;
		}

		const auto tagValueIndex = valid_index(model->index(row, ModelValueColumn, tagsIndex));
		group->insert(row, column, new MultiBitFieldWidget(tagValueIndex, fpuTagDescription, group));
		column += TagWidth + 1;
		const auto regValueIndex = nameIndex.sibling(nameIndex.row(), ModelValueColumn);
		const int regValueWidth  = regValueIndex.data(Model::FixedLengthRole).toInt();
		Q_ASSERT(regValueWidth > 0);
		const auto regCommentIndex = model->index(row, ModelCommentColumn, catIndex);
		new FpuValueField(regValueWidth, regValueIndex, tagValueIndex, group, new FieldWidget(0, regCommentIndex, group), row, column);
	}

	return group;
}

RegisterGroup *create_fpu_words(RegisterViewModelBase::Model *model, QWidget *parent) {
	const auto catIndex = find_model_category(model, "FPU");
	if (!catIndex.isValid()) {
		return nullptr;
	}

	const auto group = new RegisterGroup(tr("FPU Status&&Control Registers"), parent);
	group->appendNameValueComment(find_model_register(catIndex, FtrName), tr("FPU Tag Register"), false);

	constexpr int FsrRow = 1;
	const auto fsrIndex  = find_model_register(catIndex, FsrName);
	group->appendNameValueComment(fsrIndex, tr("FPU Status Register"), false);

	constexpr int FcrRow = 2;
	const auto fcrIndex  = find_model_register(catIndex, FcrName);
	group->appendNameValueComment(fcrIndex, tr("FPU Control Register"), false);

	constexpr int wordNameWidth       = 3;
	constexpr int wordValWidth        = 4;
	constexpr int condPrecLabelColumn = wordNameWidth + 1 + wordValWidth + 1 + 1;
	constexpr int condPrecLabelWidth  = 4;

	group->insert(FsrRow, condPrecLabelColumn, new FieldWidget("Cond", group));
	group->insert(FcrRow, condPrecLabelColumn, new FieldWidget("Prec", group));

	constexpr int condPrecValColumn = condPrecLabelColumn + condPrecLabelWidth + 1;
	constexpr int roundModeWidth = 4, precModeWidth = 2;
	constexpr int roundModeColumn = condPrecValColumn;
	constexpr int precModeColumn  = roundModeColumn + roundModeWidth + 1;

	// This must be inserted before precision&rounding value fields, since they overlap this label
	group->insert(FcrRow, precModeColumn - 1, new FieldWidget(",", group));

	for (int condN = 3; condN >= 0; --condN) {
		const auto name           = QString("C%1").arg(condN);
		const auto condNNameIndex = valid_index(find_model_register(fsrIndex, name));
		const auto condNIndex     = valid_index(condNNameIndex.sibling(condNNameIndex.row(), ModelValueColumn));
		const int column          = condPrecValColumn + 2 * (3 - condN);
		const auto nameField      = new FieldWidget(QString("%1").arg(condN), group);
		group->insert(FsrRow - 1, column, nameField);

		const auto valueField = new ValueField(1, condNIndex, group);
		group->insert(FsrRow, column, valueField);

		nameField->setToolTip(name);
		valueField->setToolTip(name);
	}

	add_rounding_mode(group, find_model_register(fcrIndex, "RC", ModelValueColumn), FcrRow, roundModeColumn);
	add_precision_mode(group, find_model_register(fcrIndex, "PC", ModelValueColumn), FcrRow, precModeColumn);

	constexpr int ErrMaskColumn = precModeColumn + precModeWidth + 2;
	constexpr int ErrLabelWidth = 3;
	group->insert(FsrRow, ErrMaskColumn, new FieldWidget("Err", group));
	group->insert(FcrRow, ErrMaskColumn, new FieldWidget("Mask", group));

	constexpr int ESColumn = ErrMaskColumn + ErrLabelWidth + 1;
	constexpr int SFColumn = ESColumn + 2;
	const auto ESNameField = new FieldWidget("E", group);

	group->insert(FsrRow - 1, ESColumn, ESNameField);

	const auto SFNameField = new FieldWidget("S", group);
	group->insert(FsrRow - 1, SFColumn, SFNameField);

	const auto ESValueField = new ValueField(1, find_model_register(fsrIndex, "ES", ModelValueColumn), group);
	group->insert(FsrRow, ESColumn, ESValueField);

	const auto SFValueField = new ValueField(1, find_model_register(fsrIndex, "SF", ModelValueColumn), group);
	group->insert(FsrRow, SFColumn, SFValueField);

	{
		const auto ESTooltip = tr("Error Summary Status") + " (ES)";
		ESNameField->setToolTip(ESTooltip);
		ESValueField->setToolTip(ESTooltip);
	}

	{
		const auto SFTooltip = tr("Stack Fault") + " (SF)";
		SFNameField->setToolTip(SFTooltip);
		SFValueField->setToolTip(SFTooltip);
	}

	constexpr int PEPMColumn = SFColumn + 2;
	add_puozdi(group, fsrIndex, fcrIndex, FsrRow - 1, PEPMColumn);

	constexpr int PUOZDIWidth = 6 * 2 - 1;
	group->insert(FsrRow, PEPMColumn + PUOZDIWidth + 1, new FieldWidget(0, comment_index(fsrIndex), group));

	return group;
}

RegisterGroup *create_fpu_last_op(RegisterViewModelBase::Model *model, QWidget *parent) {
	using RegisterViewModelBase::Model;

	const auto catIndex = find_model_category(model, "FPU");
	if (!catIndex.isValid())
		return nullptr;

	const auto FIPIndex = find_model_register(catIndex, "FIP", ModelValueColumn);
	if (!FIPIndex.isValid())
		return nullptr;

	const auto FDPIndex = find_model_register(catIndex, "FDP", ModelValueColumn);
	if (!FDPIndex.isValid())
		return nullptr;

	const auto group = new RegisterGroup(tr("FPU Last Operation Registers"), parent);
	enum { lastInsnRow,
		   lastDataRow,
		   lastOpcodeRow };
	const QString lastInsnLabel   = "Last insn";
	const QString lastDataLabel   = "Last data";
	const QString lastOpcodeLabel = "Last opcode";
	const auto lastInsnLabelField = new FieldWidget(lastInsnLabel, group);
	group->insert(lastInsnRow, 0, lastInsnLabelField);
	const auto lastDataLabelField = new FieldWidget(lastDataLabel, group);
	group->insert(lastDataRow, 0, lastDataLabelField);
	const auto lastOpcodeLabelField = new FieldWidget(lastOpcodeLabel, group);
	group->insert(lastOpcodeRow, 0, lastOpcodeLabelField);

	lastInsnLabelField->setToolTip(tr("Last FPU instruction address"));
	lastDataLabelField->setToolTip(tr("Last FPU memory operand address"));

	// FIS & FDS are not maintained in 64-bit mode; Linux64 always saves state from
	// 64-bit mode, losing the values for 32-bit apps even if the CPU doesn't deprecate them
	// We'll show zero offsets in 32 bit mode for consistency with 32-bit kernels
	// In 64-bit mode, since segments are not maintained, we'll just show offsets
	const auto FIPwidth  = FDPIndex.data(Model::FixedLengthRole).toInt();
	const auto segWidth  = FIPwidth == 8 /*8chars=>32bit*/ ? 4 : 0;
	const auto segColumn = lastInsnLabel.length() + 1;

	if (segWidth) {
		// these two must be inserted first, because seg & offset value fields overlap these labels
		group->insert(lastInsnRow, segColumn + segWidth, new FieldWidget(":", group));
		group->insert(lastDataRow, segColumn + segWidth, new FieldWidget(":", group));

		const auto FISField = new ValueField(segWidth, find_model_register(catIndex, "FIS", ModelValueColumn), group);
		group->insert(lastInsnRow, segColumn, FISField);
		const auto FDSField = new ValueField(segWidth, find_model_register(catIndex, "FDS", ModelValueColumn), group);
		group->insert(lastDataRow, segColumn, FDSField);

		FISField->setToolTip(tr("Last FPU instruction selector"));
		FDSField->setToolTip(tr("Last FPU memory operand selector"));
	}

	const auto offsetWidth = FIPIndex.data(Model::FixedLengthRole).toInt();
	assert(offsetWidth > 0);
	const auto offsetColumn  = segColumn + segWidth + (segWidth ? 1 : 0);
	const auto FIPValueField = new ValueField(offsetWidth, FIPIndex, group);
	group->insert(lastInsnRow, offsetColumn, FIPValueField);
	const auto FDPValueField = new ValueField(offsetWidth, FDPIndex, group);
	group->insert(lastDataRow, offsetColumn, FDPValueField);

	FIPValueField->setToolTip(tr("Last FPU instruction offset"));
	FDPValueField->setToolTip(tr("Last FPU memory operand offset"));

	QPersistentModelIndex const FOPIndex = find_model_register(catIndex, "FOP", ModelValueColumn);
	QPersistentModelIndex const FSRIndex = find_model_register(catIndex, FsrName, ModelValueColumn);
	QPersistentModelIndex const FCRIndex = find_model_register(catIndex, FcrName, ModelValueColumn);
	bool fopRarelyUpdated                = fop_is_compatible();

	const auto FOPFormatter = [FOPIndex, FSRIndex, FCRIndex, FIPIndex, fopRarelyUpdated](const QString &str) -> QString {
		if (str.isEmpty() || str[0] == '?')
			return str;

		const auto rawFCR = FCRIndex.data(Model::RawValueRole).toByteArray();
		assert(rawFCR.size() <= long(sizeof(edb::value16)));
		if (rawFCR.isEmpty())
			return str;
		edb::value16 fcr(0);
		std::memcpy(&fcr, rawFCR.constData(), rawFCR.size());

		const auto rawFSR = FSRIndex.data(Model::RawValueRole).toByteArray();
		assert(rawFSR.size() <= long(sizeof(edb::value16)));
		if (rawFSR.isEmpty())
			return str;
		edb::value16 fsr(0);
		std::memcpy(&fsr, rawFSR.constData(), rawFSR.size());

		const auto rawFOP = FOPIndex.data(Model::RawValueRole).toByteArray();
		edb::value16 fop(0);
		assert(rawFOP.size() <= long(sizeof(edb::value16)));
		if (rawFOP.isEmpty())
			return str;
		if (rawFOP.size() != sizeof(edb::value16))
			return QString("????");
		std::memcpy(&fop, rawFOP.constData(), rawFOP.size());

		const auto rawFIP = FIPIndex.data(Model::RawValueRole).toByteArray();
		if (rawFIP.isEmpty())
			return str;
		edb::address_t fip(0);
		assert(rawFIP.size() <= long(sizeof(fip)));
		std::memcpy(&fip, rawFIP.constData(), rawFIP.size());

		const auto excMask           = fcr & 0x3f;
		const auto excActive         = fsr & 0x3f;
		const auto excActiveUnmasked = excActive & ~excMask;
		if (fop == 0 && ((fopRarelyUpdated && !excActiveUnmasked) || fip == 0))
			return QString("00 00");
		return edb::value8(0xd8 + rawFOP[1]).toHexString() + ' ' + edb::value8(rawFOP[0]).toHexString();
	};

	const auto FOPValueField = new ValueField(5, FOPIndex, FOPFormatter, group);
	group->insert(lastOpcodeRow, lastOpcodeLabel.length() + 1, FOPValueField);

	static const auto FOPTooltip = tr("Last FPU opcode");
	lastOpcodeLabelField->setToolTip(FOPTooltip);
	FOPValueField->setToolTip(FOPTooltip);

	return group;
}

RegisterGroup *create_debug_group(RegisterViewModelBase::Model *model, QWidget *parent) {
	using RegisterViewModelBase::Model;

	const auto catIndex = find_model_category(model, "Debug");
	if (!catIndex.isValid())
		return nullptr;

	const auto group = new RegisterGroup(tr("Debug Registers"), parent);

	const auto dr6Index   = valid_index(find_model_register(catIndex, "DR6"));
	const auto dr7Index   = valid_index(find_model_register(catIndex, "DR7"));
	const auto nameWidth  = 3;
	const auto valueWidth = value_index(dr6Index).data(Model::FixedLengthRole).toInt();
	assert(valueWidth > 0);

	const auto bitsSpacing   = 1;
	const auto BTooltip      = tr("Breakpoint Condition Detected");
	const auto LTooltip      = tr("Local Breakpoint Enable");
	const auto GTooltip      = tr("Global Breakpoint Enable");
	const auto typeTooltip   = tr("Breakpoint condition");
	const auto lenTooltip    = tr("Data breakpoint length");
	const auto lenDecodedStr = tr(" (bytes count from %1)");
	int row                  = 0;

	{
		int column = nameWidth + 1 + valueWidth + 2;

		const auto BLabelField = new FieldWidget("B", group);
		BLabelField->setToolTip(BTooltip + " (B0..B3)");
		group->insert(row, column, BLabelField);
		column += bitsSpacing + 1;

		const auto LLabelField = new FieldWidget("L", group);
		LLabelField->setToolTip(LTooltip + " (L0..L3)");
		group->insert(row, column, LLabelField);
		column += bitsSpacing + 1;

		const auto GLabelField = new FieldWidget("G", group);
		GLabelField->setToolTip(GTooltip + " (G0..G3)");
		group->insert(row, column, GLabelField);
		column += bitsSpacing + 1;

		const auto typeLabelField = new FieldWidget("Type", group);
		typeLabelField->setToolTip(typeTooltip + " (R/W0..R/W3)");
		group->insert(row, column, typeLabelField);
		column += bitsSpacing + 4;

		const auto lenLabelField = new FieldWidget("Len", group);
		lenLabelField->setToolTip(lenTooltip + lenDecodedStr.arg("LEN0..LEN3"));
		group->insert(row, column, lenLabelField);
		column += bitsSpacing + 3;

		++row;
	}

	for (int drI = 0; drI < 4; ++drI, ++row) {
		const auto name          = QString("DR%1").arg(drI);
		const auto DRiValueIndex = valid_index(find_model_register(catIndex, name, ModelValueColumn));
		int column               = 0;

		group->insert(row, column, new FieldWidget(name, group));
		column += nameWidth + 1;
		group->insert(row, column, new ValueField(valueWidth, DRiValueIndex, group));
		column += valueWidth + 2;
		{
			const auto BiName       = QString("B%1").arg(drI);
			const auto BiIndex      = valid_index(find_model_register(dr6Index, BiName, ModelValueColumn));
			const auto BiValueField = new ValueField(1, BiIndex, group);
			BiValueField->setToolTip(BTooltip + " (" + BiName + ")");
			group->insert(row, column, BiValueField);
			column += bitsSpacing + 1;
		}
		{
			const auto LiName       = QString("L%1").arg(drI);
			const auto LiIndex      = valid_index(find_model_register(dr7Index, LiName, ModelValueColumn));
			const auto LiValueField = new ValueField(1, LiIndex, group);
			LiValueField->setToolTip(LTooltip + " (" + LiName + ")");
			group->insert(row, column, LiValueField);
			column += bitsSpacing + 1;
		}
		{
			const auto GiName       = QString("G%1").arg(drI);
			const auto GiIndex      = valid_index(find_model_register(dr7Index, GiName, ModelValueColumn));
			const auto GiValueField = new ValueField(1, GiIndex, group);
			GiValueField->setToolTip(GTooltip + " (" + GiName + ")");
			group->insert(row, column, GiValueField);
			column += bitsSpacing + 1;
		}
		{
			const auto RWiName                   = QString("R/W%1").arg(drI);
			const QPersistentModelIndex RWiIndex = valid_index(find_model_register(dr7Index, RWiName, ModelValueColumn));
			const auto width                     = 5;
			const auto RWiValueField             = new MultiBitFieldWidget(RWiIndex, debugRWDescription, group);
			RWiValueField->setToolTip(typeTooltip + " (" + RWiName + ")");
			group->insert(row, column, RWiValueField);
			column += bitsSpacing + width;
		}
		{
			const auto LENiName                   = QString("LEN%1").arg(drI);
			const QPersistentModelIndex LENiIndex = valid_index(find_model_register(dr7Index, LENiName, ModelValueColumn));
			const auto LENiValueField             = new MultiBitFieldWidget(LENiIndex, debugLenDescription, group);
			LENiValueField->setToolTip(lenTooltip + lenDecodedStr.arg(LENiName));
			group->insert(row, column, LENiValueField);
		}
	}

	{
		int column = 0;
		group->insert(row, column, new FieldWidget("DR6", group));
		column += nameWidth + 1;
		group->insert(row, column, new ValueField(valueWidth, value_index(dr6Index), group));
		column += valueWidth + 2;
		const QString bsName   = "BS";
		const auto bsWidth     = bsName.length();
		const auto BSNameField = new FieldWidget(bsName, group);
		const auto BSTooltip   = tr("Single Step") + " (BS)";
		BSNameField->setToolTip(BSTooltip);
		group->insert(row, column, BSNameField);
		column += bsWidth + 1;
		const auto bsIndex      = find_model_register(dr6Index, bsName, ModelValueColumn);
		const auto BSValueField = new ValueField(1, bsIndex, group);
		BSValueField->setToolTip(BSTooltip);
		group->insert(row, column, BSValueField);

		++row;
	}

	{
		int column = 0;
		group->insert(row, column, new FieldWidget("DR7", group));
		column += nameWidth + 1;
		group->insert(row, column, new ValueField(valueWidth, value_index(dr7Index), group));
		column += valueWidth + 2;
		{
			const QString leName   = "LE";
			const auto leWidth     = leName.length();
			const auto LENameField = new FieldWidget(leName, group);
			const auto LETooltip   = tr("Local Exact Breakpoint Enable");
			LENameField->setToolTip(LETooltip);
			group->insert(row, column, LENameField);
			column += leWidth + 1;
			const auto leIndex      = find_model_register(dr7Index, leName, ModelValueColumn);
			const auto leValueWidth = 1;
			const auto LEValueField = new ValueField(leValueWidth, leIndex, group);
			LEValueField->setToolTip(LETooltip);
			group->insert(row, column, LEValueField);
			column += leValueWidth + 1;
		}
		{
			const QString geName   = "GE";
			const auto geWidth     = geName.length();
			const auto GENameField = new FieldWidget(geName, group);
			const auto GETooltip   = tr("Global Exact Breakpoint Enable");
			GENameField->setToolTip(GETooltip);
			group->insert(row, column, GENameField);
			column += geWidth + 1;
			const auto geIndex      = find_model_register(dr7Index, geName, ModelValueColumn);
			const auto geValueWidth = 1;
			const auto GEValueField = new ValueField(geValueWidth, geIndex, group);
			GEValueField->setToolTip(GETooltip);
			group->insert(row, column, GEValueField);
			column += geValueWidth + 1;
		}
	}

	return group;
}

RegisterGroup *create_mxcsr(RegisterViewModelBase::Model *model, QWidget *parent) {
	using namespace RegisterViewModelBase;

	const auto catIndex = find_model_category(model, "SSE");
	if (!catIndex.isValid())
		return nullptr;
	const auto group        = new RegisterGroup("MXCSR", parent);
	const QString mxcsrName = "MXCSR";

	int column         = 0;
	const int mxcsrRow = 1, fzRow = mxcsrRow, dazRow = mxcsrRow, excRow = mxcsrRow;
	const int rndRow  = fzRow + 1;
	const int maskRow = rndRow;

	group->insert(mxcsrRow, column, new FieldWidget(mxcsrName, group));
	column += mxcsrName.length() + 1;
	const auto mxcsrIndex      = find_model_register(catIndex, "MXCSR", ModelValueColumn);
	const auto mxcsrValueWidth = mxcsrIndex.data(Model::FixedLengthRole).toInt();
	assert(mxcsrValueWidth > 0);
	group->insert(mxcsrRow, column, new ValueField(mxcsrValueWidth, mxcsrIndex, group));
	column += mxcsrValueWidth + 2;
	// XXX: Sacrificing understandability of DAZ->DZ to align PUOZDI with FPU's.
	// Also FZ value is one char away from DAZ name, which is also no good.
	// Maybe following OllyDbg example here isn't a good idea.
	const QString fzName = "FZ", dazName = "DZ";
	const auto fzColumn    = column;
	const auto fzNameField = new FieldWidget(fzName, group);
	group->insert(fzRow, fzColumn, fzNameField);
	column += fzName.length() + 1;
	const auto fzIndex      = find_model_register(mxcsrIndex, "FZ", ModelValueColumn);
	const auto fzValueWidth = 1;
	const auto fzValueField = new ValueField(fzValueWidth, fzIndex, group);
	group->insert(fzRow, column, fzValueField);
	column += fzValueWidth + 1;
	const auto dazNameField = new FieldWidget(dazName, group);
	group->insert(dazRow, column, dazNameField);
	column += dazName.length() + 1;
	const auto dazIndex      = find_model_register(mxcsrIndex, "DAZ", ModelValueColumn);
	const auto dazValueWidth = 1;
	const auto dazValueField = new ValueField(dazValueWidth, dazIndex, group);
	group->insert(dazRow, column, dazValueField);
	column += dazValueWidth + 2;
	const QString excName = "Err";
	group->insert(excRow, column, new FieldWidget(excName, group));
	const QString maskName = "Mask";
	group->insert(maskRow, column, new FieldWidget(maskName, group));
	column += maskName.length() + 1;
	add_puozdi(group, mxcsrIndex, mxcsrIndex, excRow - 1, column);
	const auto rndNameColumn = fzColumn;
	const QString rndName    = "Rnd";
	group->insert(rndRow, rndNameColumn, new FieldWidget(rndName, group));
	const auto rndColumn = rndNameColumn + rndName.length() + 1;
	add_rounding_mode(group, find_model_register(mxcsrIndex, "RC", ModelValueColumn), rndRow, rndColumn);

	{
		const auto fzTooltip = tr("Flush Denormals To Zero (FTZ)");
		fzNameField->setToolTip(fzTooltip);
		fzValueField->setToolTip(fzTooltip);
	}
	{
		const auto dazTooltip = tr("Denormals Are Zeros (DAZ)");
		dazNameField->setToolTip(dazTooltip);
		dazValueField->setToolTip(dazTooltip);
	}

	return group;
}

}

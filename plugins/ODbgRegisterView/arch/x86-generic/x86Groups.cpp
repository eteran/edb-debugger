/*
 * Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
		QStringLiteral("valid"),
		QStringLiteral("zero"),
		QStringLiteral("special"),
		QStringLiteral("empty"),
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
		QStringLiteral("NEAR"),
		QStringLiteral("DOWN"),
		QStringLiteral("  UP"),
		QStringLiteral("ZERO"),
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
		QStringLiteral("24"),
		QStringLiteral("??"),
		QStringLiteral("53"),
		QStringLiteral("64"),
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
		QStringLiteral("EXEC"),
		QStringLiteral("WRITE"),
		QStringLiteral("  IO"),
		QStringLiteral(" R/W"),
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
		QStringLiteral("1"),
		QStringLiteral("2"),
		QStringLiteral("8"),
		QStringLiteral("4"),
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

	static const auto exceptions = QStringLiteral("PUOZDI");

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
		const QString exAbbrev   = ex + QStringLiteral("E");
		const QString maskAbbrev = ex + QStringLiteral("M");

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
		excValueField->setToolTip(excName + QLatin1Char(' ') + tr("Exception") + QStringLiteral(" (") + exAbbrev + QStringLiteral(")"));
		maskValueField->setToolTip(excName + QLatin1Char(' ') + tr("Exception Mask") + QStringLiteral(" (") + maskAbbrev + QStringLiteral(")"));
	}
}

}

RegisterGroup *create_eflags(RegisterViewModelBase::Model *model, QWidget *parent) {
	const auto catIndex = find_model_category(model, QStringLiteral("General Status"));
	if (!catIndex.isValid()) {
		return nullptr;
	}

	auto nameIndex = find_model_register(catIndex, QStringLiteral("RFLAGS"));
	if (!nameIndex.isValid()) {
		nameIndex = find_model_register(catIndex, QStringLiteral("EFLAGS"));
	}

	if (!nameIndex.isValid()) {
		return nullptr;
	}

	const auto group        = new RegisterGroup(tr("EFL"), parent);
	constexpr int NameWidth = 3;
	int column              = 0;
	group->insert(0, column, new FieldWidget(QStringLiteral("EFL"), group));

	constexpr int ValueWidth = 8;
	const auto valueIndex    = nameIndex.sibling(nameIndex.row(), ModelValueColumn);
	column += NameWidth + 1;
	group->insert(0, column, new ValueField(ValueWidth, valueIndex, [](const QString &v) { return v.right(8); }, group));

	const auto commentIndex = nameIndex.sibling(nameIndex.row(), ModelCommentColumn);
	column += ValueWidth + 1;
	group->insert(0, column, new FieldWidget(0, commentIndex, group));

	return group;
}

RegisterGroup *create_expanded_eflags(RegisterViewModelBase::Model *model, QWidget *parent) {
	const auto catIndex = find_model_category(model, QStringLiteral("General Status"));
	if (!catIndex.isValid()) {
		return nullptr;
	}

	auto regNameIndex = find_model_register(catIndex, QStringLiteral("RFLAGS"));
	if (!regNameIndex.isValid()) {
		regNameIndex = find_model_register(catIndex, QStringLiteral("EFLAGS"));
	}

	if (!regNameIndex.isValid()) {
		return nullptr;
	}

	const auto group = new RegisterGroup(tr("Expanded EFL"), parent);

	static const std::unordered_map<char, QString> flagTooltips = {
		{'C', tr("Carry flag") + QStringLiteral(" (CF)")},
		{'P', tr("Parity flag") + QStringLiteral(" (PF)")},
		{'A', tr("Auxiliary carry flag") + QStringLiteral(" (AF)")},
		{'Z', tr("Zero flag") + QStringLiteral(" (ZF)")},
		{'S', tr("Sign flag") + QStringLiteral(" (SF)")},
		{'T', tr("Trap flag") + QStringLiteral(" (TF)")},
		{'D', tr("Direction flag") + QStringLiteral(" (DF)")},
		{'O', tr("Overflow flag") + QStringLiteral(" (OF)")},
	};
	for (int row = 0, groupRow = 0; row < model->rowCount(regNameIndex); ++row) {
		const auto flagNameIndex  = model->index(row, ModelNameColumn, regNameIndex);
		const auto flagValueIndex = model->index(row, ModelValueColumn, regNameIndex);
		const auto flagName       = model->data(flagNameIndex).toString().toUpper();

		if (flagName.length() != 2 || flagName[1] != QLatin1Char('F')) {
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
			const auto nameField = new FieldWidget(QChar(static_cast<char16_t>(name)), group);
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

	const auto catIndex = find_model_category(model, QStringLiteral("FPU"));
	if (!catIndex.isValid()) {
		return nullptr;
	}

	const auto tagsIndex = find_model_register(catIndex, QString::fromLatin1(FtrName));
	if (!tagsIndex.isValid()) {
		qWarning() << "Warning: failed to find FTR in the model, refusing to continue making FPUData group";
		return nullptr;
	}

	const auto group          = new RegisterGroup(tr("FPU Data Registers"), parent);
	constexpr int FpuRegCount = 8;
	constexpr int NameWidth   = 3;
	constexpr int TagWidth    = 7;
	const auto fsrIndex       = valid_index(find_model_register(catIndex, QString::fromLatin1(FsrName)));

	const QPersistentModelIndex topIndex = valid_index(find_model_register(fsrIndex, QStringLiteral("TOP"), ModelValueColumn));

	for (int row = 0; row < FpuRegCount; ++row) {
		int column           = 0;
		const auto nameIndex = model->index(row, ModelNameColumn, catIndex);
		{
			const auto STiFormatter = [row, topIndex]() {
				const auto topByteArray = topIndex.data(Model::RawValueRole).toByteArray();
				if (topByteArray.isEmpty()) {
					return QStringLiteral("R%1").arg(row);
				}
				const auto top = topByteArray[0];
				assert(top >= 0);
				Q_ASSERT(top < 8);
				const auto stI = (row + 8 - top) % 8;
				return QStringLiteral("ST%1").arg(stI);
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
	const auto catIndex = find_model_category(model, QStringLiteral("FPU"));
	if (!catIndex.isValid()) {
		return nullptr;
	}

	const auto group = new RegisterGroup(tr("FPU Status&&Control Registers"), parent);
	group->appendNameValueComment(find_model_register(catIndex, QString::fromLatin1(FtrName)), tr("FPU Tag Register"), false);

	constexpr int FsrRow = 1;
	const auto fsrIndex  = find_model_register(catIndex, QString::fromLatin1(FsrName));
	group->appendNameValueComment(fsrIndex, tr("FPU Status Register"), false);

	constexpr int FcrRow = 2;
	const auto fcrIndex  = find_model_register(catIndex, QString::fromLatin1(FcrName));
	group->appendNameValueComment(fcrIndex, tr("FPU Control Register"), false);

	constexpr int wordNameWidth       = 3;
	constexpr int wordValWidth        = 4;
	constexpr int condPrecLabelColumn = wordNameWidth + 1 + wordValWidth + 1 + 1;
	constexpr int condPrecLabelWidth  = 4;

	group->insert(FsrRow, condPrecLabelColumn, new FieldWidget(QStringLiteral("Cond"), group));
	group->insert(FcrRow, condPrecLabelColumn, new FieldWidget(QStringLiteral("Prec"), group));

	constexpr int condPrecValColumn = condPrecLabelColumn + condPrecLabelWidth + 1;
	constexpr int roundModeWidth    = 4;
	constexpr int precModeWidth     = 2;
	constexpr int roundModeColumn   = condPrecValColumn;
	constexpr int precModeColumn    = roundModeColumn + roundModeWidth + 1;

	// This must be inserted before precision&rounding value fields, since they overlap this label
	group->insert(FcrRow, precModeColumn - 1, new FieldWidget(QStringLiteral(","), group));

	for (int condN = 3; condN >= 0; --condN) {
		const auto name           = QStringLiteral("C%1").arg(condN);
		const auto condNNameIndex = valid_index(find_model_register(fsrIndex, name));
		const auto condNIndex     = valid_index(condNNameIndex.sibling(condNNameIndex.row(), ModelValueColumn));
		const int column          = condPrecValColumn + 2 * (3 - condN);
		const auto nameField      = new FieldWidget(QString::number(condN), group);
		group->insert(FsrRow - 1, column, nameField);

		const auto valueField = new ValueField(1, condNIndex, group);
		group->insert(FsrRow, column, valueField);

		nameField->setToolTip(name);
		valueField->setToolTip(name);
	}

	add_rounding_mode(group, find_model_register(fcrIndex, QStringLiteral("RC"), ModelValueColumn), FcrRow, roundModeColumn);
	add_precision_mode(group, find_model_register(fcrIndex, QStringLiteral("PC"), ModelValueColumn), FcrRow, precModeColumn);

	constexpr int ErrMaskColumn = precModeColumn + precModeWidth + 2;
	constexpr int ErrLabelWidth = 3;
	group->insert(FsrRow, ErrMaskColumn, new FieldWidget(QStringLiteral("Err"), group));
	group->insert(FcrRow, ErrMaskColumn, new FieldWidget(QStringLiteral("Mask"), group));

	constexpr int ESColumn = ErrMaskColumn + ErrLabelWidth + 1;
	constexpr int SFColumn = ESColumn + 2;
	const auto ESNameField = new FieldWidget(QStringLiteral("E"), group);

	group->insert(FsrRow - 1, ESColumn, ESNameField);

	const auto SFNameField = new FieldWidget(QStringLiteral("S"), group);
	group->insert(FsrRow - 1, SFColumn, SFNameField);

	const auto ESValueField = new ValueField(1, find_model_register(fsrIndex, QStringLiteral("ES"), ModelValueColumn), group);
	group->insert(FsrRow, ESColumn, ESValueField);

	const auto SFValueField = new ValueField(1, find_model_register(fsrIndex, QStringLiteral("SF"), ModelValueColumn), group);
	group->insert(FsrRow, SFColumn, SFValueField);

	{
		const auto ESTooltip = tr("Error Summary Status") + QStringLiteral(" (ES)");
		ESNameField->setToolTip(ESTooltip);
		ESValueField->setToolTip(ESTooltip);
	}

	{
		const auto SFTooltip = tr("Stack Fault") + QStringLiteral(" (SF)");
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

	const auto catIndex = find_model_category(model, QStringLiteral("FPU"));
	if (!catIndex.isValid()) {
		return nullptr;
	}

	const auto FIPIndex = find_model_register(catIndex, QStringLiteral("FIP"), ModelValueColumn);
	if (!FIPIndex.isValid()) {
		return nullptr;
	}

	const auto FDPIndex = find_model_register(catIndex, QStringLiteral("FDP"), ModelValueColumn);
	if (!FDPIndex.isValid()) {
		return nullptr;
	}

	const auto group = new RegisterGroup(tr("FPU Last Operation Registers"), parent);
	enum { lastInsnRow,
		   lastDataRow,
		   lastOpcodeRow };
	const auto lastInsnLabel      = QStringLiteral("Last insn");
	const auto lastDataLabel      = QStringLiteral("Last data");
	const auto lastOpcodeLabel    = QStringLiteral("Last opcode");
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
	const auto segColumn = static_cast<int>(lastInsnLabel.size()) + 1;

	if (segWidth) {
		// these two must be inserted first, because seg & offset value fields overlap these labels
		group->insert(lastInsnRow, segColumn + segWidth, new FieldWidget(QStringLiteral(":"), group));
		group->insert(lastDataRow, segColumn + segWidth, new FieldWidget(QStringLiteral(":"), group));

		const auto FISField = new ValueField(segWidth, find_model_register(catIndex, QStringLiteral("FIS"), ModelValueColumn), group);
		group->insert(lastInsnRow, segColumn, FISField);
		const auto FDSField = new ValueField(segWidth, find_model_register(catIndex, QStringLiteral("FDS"), ModelValueColumn), group);
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

	QPersistentModelIndex const FOPIndex = find_model_register(catIndex, QStringLiteral("FOP"), ModelValueColumn);
	QPersistentModelIndex const FSRIndex = find_model_register(catIndex, QString::fromLatin1(FsrName), ModelValueColumn);
	QPersistentModelIndex const FCRIndex = find_model_register(catIndex, QString::fromLatin1(FcrName), ModelValueColumn);
	bool fopRarelyUpdated                = fop_is_compatible();

	const auto FOPFormatter = [FOPIndex, FSRIndex, FCRIndex, FIPIndex, fopRarelyUpdated](const QString &str) -> QString {
		if (str.isEmpty() || str[0] == QLatin1Char('?')) {
			return str;
		}

		const auto rawFCR = FCRIndex.data(Model::RawValueRole).toByteArray();
		assert(rawFCR.size() <= long(sizeof(edb::value16)));
		if (rawFCR.isEmpty()) {
			return str;
		}
		edb::value16 fcr(0);
		std::memcpy(&fcr, rawFCR.constData(), rawFCR.size());

		const auto rawFSR = FSRIndex.data(Model::RawValueRole).toByteArray();
		assert(rawFSR.size() <= long(sizeof(edb::value16)));
		if (rawFSR.isEmpty()) {
			return str;
		}
		edb::value16 fsr(0);
		std::memcpy(&fsr, rawFSR.constData(), rawFSR.size());

		const auto rawFOP = FOPIndex.data(Model::RawValueRole).toByteArray();
		edb::value16 fop(0);
		assert(rawFOP.size() <= long(sizeof(edb::value16)));
		if (rawFOP.isEmpty()) {
			return str;
		}
		if (rawFOP.size() != sizeof(edb::value16)) {
			return QStringLiteral("????");
		}
		std::memcpy(&fop, rawFOP.constData(), rawFOP.size());

		const auto rawFIP = FIPIndex.data(Model::RawValueRole).toByteArray();
		if (rawFIP.isEmpty()) {
			return str;
		}
		edb::address_t fip(0);
		assert(rawFIP.size() <= long(sizeof(fip)));
		std::memcpy(&fip, rawFIP.constData(), rawFIP.size());

		const auto excMask           = fcr & 0x3f;
		const auto excActive         = fsr & 0x3f;
		const auto excActiveUnmasked = excActive & ~excMask;
		if (fop == 0 && ((fopRarelyUpdated && !excActiveUnmasked) || fip == 0)) {
			return QStringLiteral("00 00");
		}
		return edb::value8(0xd8 + rawFOP[1]).toHexString() + QLatin1Char(' ') + edb::value8(rawFOP[0]).toHexString();
	};

	const auto FOPValueField = new ValueField(5, FOPIndex, FOPFormatter, group);
	group->insert(lastOpcodeRow, static_cast<int>(lastOpcodeLabel.size()) + 1, FOPValueField);

	static const auto FOPTooltip = tr("Last FPU opcode");
	lastOpcodeLabelField->setToolTip(FOPTooltip);
	FOPValueField->setToolTip(FOPTooltip);

	return group;
}

RegisterGroup *create_debug_group(RegisterViewModelBase::Model *model, QWidget *parent) {
	using RegisterViewModelBase::Model;

	const auto catIndex = find_model_category(model, QStringLiteral("Debug"));
	if (!catIndex.isValid()) {
		return nullptr;
	}

	const auto group = new RegisterGroup(tr("Debug Registers"), parent);

	const auto dr6Index   = valid_index(find_model_register(catIndex, QStringLiteral("DR6")));
	const auto dr7Index   = valid_index(find_model_register(catIndex, QStringLiteral("DR7")));
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

		const auto BLabelField = new FieldWidget(QStringLiteral("B"), group);
		BLabelField->setToolTip(BTooltip + QStringLiteral(" (B0..B3)"));
		group->insert(row, column, BLabelField);
		column += bitsSpacing + 1;

		const auto LLabelField = new FieldWidget(QStringLiteral("L"), group);
		LLabelField->setToolTip(LTooltip + QStringLiteral(" (L0..L3)"));
		group->insert(row, column, LLabelField);
		column += bitsSpacing + 1;

		const auto GLabelField = new FieldWidget(QStringLiteral("G"), group);
		GLabelField->setToolTip(GTooltip + QStringLiteral(" (G0..G3)"));
		group->insert(row, column, GLabelField);
		column += bitsSpacing + 1;

		const auto typeLabelField = new FieldWidget(QStringLiteral("Type"), group);
		typeLabelField->setToolTip(typeTooltip + QStringLiteral(" (R/W0..R/W3)"));
		group->insert(row, column, typeLabelField);
		column += bitsSpacing + 4;

		const auto lenLabelField = new FieldWidget(QStringLiteral("Len"), group);
		lenLabelField->setToolTip(lenTooltip + lenDecodedStr.arg(QStringLiteral("LEN0..LEN3")));
		group->insert(row, column, lenLabelField);

		++row;
	}

	for (int drI = 0; drI < 4; ++drI, ++row) {
		const auto name          = QStringLiteral("DR%1").arg(drI);
		const auto DRiValueIndex = valid_index(find_model_register(catIndex, name, ModelValueColumn));
		int column               = 0;

		group->insert(row, column, new FieldWidget(name, group));
		column += nameWidth + 1;
		group->insert(row, column, new ValueField(valueWidth, DRiValueIndex, group));
		column += valueWidth + 2;
		{
			const auto BiName       = QStringLiteral("B%1").arg(drI);
			const auto BiIndex      = valid_index(find_model_register(dr6Index, BiName, ModelValueColumn));
			const auto BiValueField = new ValueField(1, BiIndex, group);
			BiValueField->setToolTip(BTooltip + QStringLiteral(" (") + BiName + QStringLiteral(")"));
			group->insert(row, column, BiValueField);
			column += bitsSpacing + 1;
		}
		{
			const auto LiName       = QStringLiteral("L%1").arg(drI);
			const auto LiIndex      = valid_index(find_model_register(dr7Index, LiName, ModelValueColumn));
			const auto LiValueField = new ValueField(1, LiIndex, group);
			LiValueField->setToolTip(LTooltip + QStringLiteral(" (") + LiName + QStringLiteral(")"));
			group->insert(row, column, LiValueField);
			column += bitsSpacing + 1;
		}
		{
			const auto GiName       = QStringLiteral("G%1").arg(drI);
			const auto GiIndex      = valid_index(find_model_register(dr7Index, GiName, ModelValueColumn));
			const auto GiValueField = new ValueField(1, GiIndex, group);
			GiValueField->setToolTip(GTooltip + QStringLiteral(" (") + GiName + QStringLiteral(")"));
			group->insert(row, column, GiValueField);
			column += bitsSpacing + 1;
		}
		{
			const auto RWiName                   = QStringLiteral("R/W%1").arg(drI);
			const QPersistentModelIndex RWiIndex = valid_index(find_model_register(dr7Index, RWiName, ModelValueColumn));
			const auto width                     = 5;
			const auto RWiValueField             = new MultiBitFieldWidget(RWiIndex, debugRWDescription, group);
			RWiValueField->setToolTip(typeTooltip + QStringLiteral(" (") + RWiName + QStringLiteral(")"));
			group->insert(row, column, RWiValueField);
			column += bitsSpacing + width;
		}
		{
			const auto LENiName                   = QStringLiteral("LEN%1").arg(drI);
			const QPersistentModelIndex LENiIndex = valid_index(find_model_register(dr7Index, LENiName, ModelValueColumn));
			const auto LENiValueField             = new MultiBitFieldWidget(LENiIndex, debugLenDescription, group);
			LENiValueField->setToolTip(lenTooltip + lenDecodedStr.arg(LENiName));
			group->insert(row, column, LENiValueField);
		}
	}

	{
		int column = 0;
		group->insert(row, column, new FieldWidget(QStringLiteral("DR6"), group));
		column += nameWidth + 1;

		group->insert(row, column, new ValueField(valueWidth, value_index(dr6Index), group));
		column += valueWidth + 2;

		const auto bsName      = QStringLiteral("BS");
		const auto bsWidth     = static_cast<int>(bsName.size());
		const auto BSNameField = new FieldWidget(bsName, group);
		const auto BSTooltip   = tr("Single Step") + QStringLiteral(" (BS)");
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
		group->insert(row, column, new FieldWidget(QStringLiteral("DR7"), group));
		column += nameWidth + 1;

		group->insert(row, column, new ValueField(valueWidth, value_index(dr7Index), group));
		column += valueWidth + 2;
		{
			const auto leName      = QStringLiteral("LE");
			const auto leWidth     = static_cast<int>(leName.size());
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
			const auto geName      = QStringLiteral("GE");
			const auto geWidth     = static_cast<int>(geName.size());
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
		}
	}

	return group;
}

RegisterGroup *create_mxcsr(RegisterViewModelBase::Model *model, QWidget *parent) {
	using namespace RegisterViewModelBase;

	const auto catIndex = find_model_category(model, QStringLiteral("SSE"));
	if (!catIndex.isValid()) {
		return nullptr;
	}
	const auto group     = new RegisterGroup(QStringLiteral("MXCSR"), parent);
	const auto mxcsrName = QStringLiteral("MXCSR");

	int column         = 0;
	const int mxcsrRow = 1;
	const int fzRow    = mxcsrRow;
	const int dazRow   = mxcsrRow;
	const int excRow   = mxcsrRow;
	const int rndRow   = fzRow + 1;
	const int maskRow  = rndRow;

	group->insert(mxcsrRow, column, new FieldWidget(mxcsrName, group));
	column += static_cast<int>(mxcsrName.size()) + 1;
	const auto mxcsrIndex      = find_model_register(catIndex, QStringLiteral("MXCSR"), ModelValueColumn);
	const auto mxcsrValueWidth = mxcsrIndex.data(Model::FixedLengthRole).toInt();
	assert(mxcsrValueWidth > 0);
	group->insert(mxcsrRow, column, new ValueField(mxcsrValueWidth, mxcsrIndex, group));
	column += mxcsrValueWidth + 2;
	// XXX: Sacrificing understandability of DAZ->DZ to align PUOZDI with FPU's.
	// Also FZ value is one char away from DAZ name, which is also no good.
	// Maybe following OllyDbg example here isn't a good idea.
	const auto fzName      = QStringLiteral("FZ");
	const auto dazName     = QStringLiteral("DZ");
	const auto fzColumn    = column;
	const auto fzNameField = new FieldWidget(fzName, group);
	group->insert(fzRow, fzColumn, fzNameField);
	column += static_cast<int>(fzName.size()) + 1;
	const auto fzIndex      = find_model_register(mxcsrIndex, QStringLiteral("FZ"), ModelValueColumn);
	const auto fzValueWidth = 1;
	const auto fzValueField = new ValueField(fzValueWidth, fzIndex, group);
	group->insert(fzRow, column, fzValueField);
	column += fzValueWidth + 1;
	const auto dazNameField = new FieldWidget(dazName, group);
	group->insert(dazRow, column, dazNameField);
	column += static_cast<int>(dazName.size()) + 1;
	const auto dazIndex      = find_model_register(mxcsrIndex, QStringLiteral("DAZ"), ModelValueColumn);
	const auto dazValueWidth = 1;
	const auto dazValueField = new ValueField(dazValueWidth, dazIndex, group);
	group->insert(dazRow, column, dazValueField);
	column += dazValueWidth + 2;
	const auto excName = QStringLiteral("Err");
	group->insert(excRow, column, new FieldWidget(excName, group));
	const auto maskName = QStringLiteral("Mask");
	group->insert(maskRow, column, new FieldWidget(maskName, group));
	column += static_cast<int>(maskName.size()) + 1;
	add_puozdi(group, mxcsrIndex, mxcsrIndex, excRow - 1, column);
	const auto rndNameColumn = fzColumn;
	const auto rndName       = QStringLiteral("Rnd");
	group->insert(rndRow, rndNameColumn, new FieldWidget(rndName, group));
	const auto rndColumn = rndNameColumn + static_cast<int>(rndName.size()) + 1;
	add_rounding_mode(group, find_model_register(mxcsrIndex, QStringLiteral("RC"), ModelValueColumn), rndRow, rndColumn);

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

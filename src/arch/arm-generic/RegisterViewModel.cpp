/*
Copyright (C) 2017 - 2017 Evan Teran
                          evan.teran@gmail.com
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

#include "RegisterViewModel.h"
#include <QDebug>
#include <typeinfo>

namespace {

using GPR   = RegisterViewModelBase::SimpleRegister<edb::value32>;
using CPSR  = RegisterViewModelBase::FlagsRegister<edb::value32>;
using FPSCR = RegisterViewModelBase::FlagsRegister<edb::value32>;

std::vector<RegisterViewModelBase::BitFieldDescriptionEx> cpsrDescription = {
	{QLatin1String("M"), 0, 5},
	{QLatin1String("T"), 5, 1},
	{QLatin1String("F"), 6, 1},
	{QLatin1String("I"), 7, 1},
	{QLatin1String("A"), 8, 1},
	{QLatin1String("E"), 9, 1},
	{QLatin1String("IT2"), 10, 1},
	{QLatin1String("IT3"), 11, 1},
	{QLatin1String("IT4"), 12, 1},
	{QLatin1String("ITbcond"), 13, 3},
	{QLatin1String("GE0"), 16, 1},
	{QLatin1String("GE1"), 17, 1},
	{QLatin1String("GE2"), 18, 1},
	{QLatin1String("GE3"), 19, 1},
	{QLatin1String("J"), 24, 1},
	{QLatin1String("IT0"), 25, 1},
	{QLatin1String("IT1"), 26, 1},
	{QLatin1String("Q"), 27, 1},
	{QLatin1String("V"), 28, 1},
	{QLatin1String("C"), 29, 1},
	{QLatin1String("Z"), 30, 1},
	{QLatin1String("N"), 31, 1},
};

static const std::vector<QString> roundingStrings = {
	QObject::tr("Rounding to nearest"),
	QObject::tr("Rounding to Plus infinity"),
	QObject::tr("Rounding to Minus infinity"),
	QObject::tr("Rounding toward zero"),
};

std::vector<RegisterViewModelBase::BitFieldDescriptionEx> fpscrDescription = {
	{QLatin1String("IOC"), 0, 1},
	{QLatin1String("DZC"), 1, 1},
	{QLatin1String("OFC"), 2, 1},
	{QLatin1String("UFC"), 3, 1},
	{QLatin1String("IXC"), 4, 1},
	{QLatin1String("IDC"), 7, 1},
	{QLatin1String("IOE"), 8, 1},
	{QLatin1String("DZE"), 9, 1},
	{QLatin1String("OFE"), 10, 1},
	{QLatin1String("UFE"), 11, 1},
	{QLatin1String("IXE"), 12, 1},
	{QLatin1String("IDE"), 15, 1},
	{QLatin1String("LEN-1"), 16, 3},
	{QLatin1String("STR"), 20, 2},
	{QLatin1String("RC"), 22, 2, roundingStrings},
	{QLatin1String("FZ"), 24, 1},
	{QLatin1String("DN"), 25, 1},
	{QLatin1String("V"), 28, 1},
	{QLatin1String("C"), 29, 1},
	{QLatin1String("Z"), 30, 1},
	{QLatin1String("N"), 31, 1},
};

enum {
	CPSR_ROW,
	FPSCR_ROW = 0,
};

}

QVariant RegisterViewModel::data(QModelIndex const &index, int role) const {
	if (role == FixedLengthRole) {
		using namespace RegisterViewModelBase;
		const auto reg  = static_cast<RegisterViewItem *>(index.internalPointer());
		const auto name = reg->data(NAME_COLUMN).toString();
		if (index.column() == NAME_COLUMN && name.length() >= 2) {
			const QString nameLower = name.toLower();
			if ((nameLower[0] == 'r' && '0' <= nameLower[1] && nameLower[1] <= '9') ||
				nameLower == "sp" || nameLower == "lr" || nameLower == "pc")
				return 3;
		}
	}
	return Model::data(index, role);
}

void addGPRs(RegisterViewModelBase::Category *gprs) {
	gprs->addRegister(std::make_unique<GPR>("R0"));
	gprs->addRegister(std::make_unique<GPR>("R1"));
	gprs->addRegister(std::make_unique<GPR>("R2"));
	gprs->addRegister(std::make_unique<GPR>("R3"));
	gprs->addRegister(std::make_unique<GPR>("R4"));
	gprs->addRegister(std::make_unique<GPR>("R5"));
	gprs->addRegister(std::make_unique<GPR>("R6"));
	gprs->addRegister(std::make_unique<GPR>("R7"));
	gprs->addRegister(std::make_unique<GPR>("R8"));
	gprs->addRegister(std::make_unique<GPR>("R9"));
	gprs->addRegister(std::make_unique<GPR>("R10"));
	gprs->addRegister(std::make_unique<GPR>("R11"));
	gprs->addRegister(std::make_unique<GPR>("R12"));
	gprs->addRegister(std::make_unique<GPR>("SP"));
	gprs->addRegister(std::make_unique<GPR>("LR"));
	gprs->addRegister(std::make_unique<GPR>("PC"));
}

void addGenStatusRegs(RegisterViewModelBase::Category *cat) {
	cat->addRegister(std::make_unique<CPSR>("CPSR", cpsrDescription));
}

void addVFPRegs(RegisterViewModelBase::Category *cat) {
	cat->addRegister(std::make_unique<FPSCR>("FPSCR", fpscrDescription));
	// TODO: add data registers: Sn, Dn, Qn...
}

RegisterViewModel::RegisterViewModel(int cpuSuppFlags, QObject *parent)
	: RegisterViewModelBase::Model(parent),
	  gprs(addCategory(tr("General Purpose"))),
	  genStatusRegs(addCategory(tr("General Status"))),
	  vfpRegs(addFPUCategory(tr("VFP"))) {

	addGPRs(gprs);
	addGenStatusRegs(genStatusRegs);
	addVFPRegs(vfpRegs);

	setCpuMode(CpuMode::UNKNOWN);
}

void invalidate(RegisterViewModelBase::Category *cat, int row, const char *nameToCheck) {
	if (!cat) return;
	Q_ASSERT(row < cat->childCount());
	const auto reg = cat->getRegister(row);
	Q_ASSERT(!nameToCheck || reg->name() == nameToCheck);
	Q_UNUSED(nameToCheck)
	reg->invalidate();
}

template <typename RegType, typename ValueType>
void updateRegister(RegisterViewModelBase::Category *cat, int row, ValueType value, QString const &comment, const char *nameToCheck = 0) {
	const auto reg = cat->getRegister(row);
	if (!dynamic_cast<RegType *>(reg)) {
		qWarning() << "Failed to update register " << reg->name() << ": failed to convert register passed to expected type " << typeid(RegType).name();
		invalidate(cat, row, nameToCheck);
		return;
	}
	Q_ASSERT(!nameToCheck || reg->name() == nameToCheck);
	Q_UNUSED(nameToCheck)
	static_cast<RegType *>(reg)->update(value, comment);
}

void RegisterViewModel::updateGPR(std::size_t i, edb::value32 val, QString const &comment) {
	Q_ASSERT(int(i) < gprs->childCount());
	updateRegister<GPR>(gprs, i, val, comment);
}

void RegisterViewModel::updateCPSR(edb::value32 val, QString const &comment) {
	updateRegister<CPSR>(genStatusRegs, CPSR_ROW, val, comment);
}

void RegisterViewModel::updateFPSCR(edb::value32 val, QString const &comment) {
	updateRegister<FPSCR>(vfpRegs, FPSCR_ROW, val, comment);
}

void RegisterViewModel::showAll() {
	gprs->show();
	genStatusRegs->show();
	vfpRegs->show();
}

void RegisterViewModel::setCpuMode(CpuMode newMode) {
	if (mode == newMode) return;

	beginResetModel();
	mode = newMode;
	switch (newMode) {
	case CpuMode::UNKNOWN:
		hideAll();
		break;
	case CpuMode::Defined:
		showAll();
		break;
	default:
		EDB_PRINT_AND_DIE("Invalid newMode value ", (long)newMode);
	}
	endResetModel();
}

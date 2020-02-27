/*
Copyright (C) 2017 - 2017 Evan Teran
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

#include "RegisterViewModel.h"
#include "Util.h"

#include <cstdint>
#include <typeinfo>

#include <QDebug>
#include <QList>

void invalidate(RegisterViewModelBase::Category *cat, int row, const char *nameToCheck = nullptr);

namespace {

template <std::size_t bitSize>
struct Regs;
template <>
struct Regs<32> {
	using GPR   = RegisterViewModelBase::SimpleRegister<edb::value32>;
	using IP    = GPR;
	using FLAGS = RegisterViewModelBase::FlagsRegister<edb::value32>;
	static constexpr char namePrefix() { return 'E'; }
};

template <>
struct Regs<64> {
	using GPR   = RegisterViewModelBase::SimpleRegister<edb::value64>;
	using IP    = GPR;
	using FLAGS = RegisterViewModelBase::FlagsRegister<edb::value64>;
	static constexpr char namePrefix() { return 'R'; }
};

constexpr std::size_t FPU_REG_COUNT   = 8;
constexpr std::size_t MMX_REG_COUNT   = FPU_REG_COUNT;
constexpr std::size_t SSE_REG_COUNT32 = 8;
constexpr std::size_t SSE_REG_COUNT64 = 16;
constexpr std::size_t AVX_REG_COUNT32 = 8;
constexpr std::size_t AVX_REG_COUNT64 = 16;
constexpr std::size_t DBG_REG_COUNT   = 8;

enum {
	RIP_ROW,
	EIP_ROW = RIP_ROW,
	RFLAGS_ROW,
	EFLAGS_ROW = RFLAGS_ROW,
	FCR_ROW    = FPU_REG_COUNT + 0,
	FSR_ROW,
	FTR_ROW,
	FOP_ROW,
	FIS_ROW,
	FIP_ROW,
	FDS_ROW,
	FDP_ROW,
};

using GPR32      = typename Regs<32>::GPR;
using GPR64      = typename Regs<64>::GPR;
using EFLAGS     = typename Regs<32>::FLAGS;
using RFLAGS     = typename Regs<64>::FLAGS;
using EIP        = typename Regs<32>::IP;
using RIP        = typename Regs<64>::IP;
using FPUReg     = RegisterViewModelBase::FPURegister<edb::value80>;
using FPUWord    = RegisterViewModelBase::FlagsRegister<edb::value16>;
using FOPCReg    = RegisterViewModelBase::SimpleRegister<edb::value16>;
using SegmentReg = RegisterViewModelBase::SimpleRegister<edb::value16>;
using MMXReg     = RegisterViewModelBase::SIMDRegister<edb::value64>;
using SSEReg     = RegisterViewModelBase::SIMDRegister<edb::value128>;
using AVXReg     = RegisterViewModelBase::SIMDRegister<edb::value256>;
using MXCSR      = RegisterViewModelBase::FlagsRegister<edb::value32>;

const std::vector<RegisterViewModelBase::BitFieldDescriptionEx> flagsDescription = {
	{QLatin1String("CF"), 0, 1},
	{QLatin1String("PF"), 2, 1},
	{QLatin1String("AF"), 4, 1},
	{QLatin1String("ZF"), 6, 1},
	{QLatin1String("SF"), 7, 1},
	{QLatin1String("TF"), 8, 1},
	{QLatin1String("IF"), 9, 1},
	{QLatin1String("DF"), 10, 1},
	{QLatin1String("OF"), 11, 1},
	// these commented out system flags are always zeroed by the kernel before giving out to debugger
	//	{QLatin1String("IOPL"),12,2},
	//	{QLatin1String("NT"),14,1},
	{QLatin1String("RF"), 16, 1}, // is debugger-visibly set on e.g. CLI instruction segfault
								  //	{QLatin1String("VM"),17,1},
	{QLatin1String("AC"), 18, 1}, // this one can be set by the application and is visible to debugger
								  //	{QLatin1String("VIF"),19,1},
								  //	{QLatin1String("VIP"),20,1},
	{QLatin1String("ID"), 21, 1}, // this one can be set by the application and is visible to debugger
};

const std::vector<QString> roundingStrings = {
	QObject::tr("Rounding to nearest"),
	QObject::tr("Rounding down"),
	QObject::tr("Rounding up"),
	QObject::tr("Rounding toward zero"),
};

const std::vector<RegisterViewModelBase::BitFieldDescriptionEx> FCRDescription = {
	{QLatin1String("IM"), 0, 1},
	{QLatin1String("DM"), 1, 1},
	{QLatin1String("ZM"), 2, 1},
	{QLatin1String("OM"), 3, 1},
	{QLatin1String("UM"), 4, 1},
	{QLatin1String("PM"), 5, 1},
	{QLatin1String("PC"), 8, 2, {QObject::tr("Single precision (24 bit complete mantissa)"), QString(), QObject::tr("Double precision (53 bit complete mantissa)"), QObject::tr("Extended precision (64 bit mantissa)")}},
	{QLatin1String("RC"), 10, 2, roundingStrings},
	{QLatin1String("X"), 12, 1},
};

const std::vector<RegisterViewModelBase::BitFieldDescriptionEx> FSRDescription = {
	{QLatin1String("IE"), 0, 1},
	{QLatin1String("DE"), 1, 1},
	{QLatin1String("ZE"), 2, 1},
	{QLatin1String("OE"), 3, 1},
	{QLatin1String("UE"), 4, 1},
	{QLatin1String("PE"), 5, 1},
	{QLatin1String("SF"), 6, 1},
	{QLatin1String("ES"), 7, 1},
	{QLatin1String("C0"), 8, 1},
	{QLatin1String("C1"), 9, 1},
	{QLatin1String("C2"), 10, 1},
	{QLatin1String("TOP"), 11, 3},
	{QLatin1String("C3"), 14, 1},
	{QLatin1String("B"), 15, 1},
};

const std::vector<QString> tagStrings = {
	QObject::tr("Valid"),
	QObject::tr("Zero"),
	QObject::tr("Special"),
	QObject::tr("Empty"),
};

const std::vector<RegisterViewModelBase::BitFieldDescriptionEx> FTRDescription = {
	{QLatin1String("T0"), 0, 2, tagStrings},
	{QLatin1String("T1"), 2, 2, tagStrings},
	{QLatin1String("T2"), 4, 2, tagStrings},
	{QLatin1String("T3"), 6, 2, tagStrings},
	{QLatin1String("T4"), 8, 2, tagStrings},
	{QLatin1String("T5"), 10, 2, tagStrings},
	{QLatin1String("T6"), 12, 2, tagStrings},
	{QLatin1String("T7"), 14, 2, tagStrings},
};

const std::vector<RegisterViewModelBase::BitFieldDescriptionEx> MXCSRDescription = {
	{QLatin1String("IE"), 0, 1},
	{QLatin1String("DE"), 1, 1},
	{QLatin1String("ZE"), 2, 1},
	{QLatin1String("OE"), 3, 1},
	{QLatin1String("UE"), 4, 1},
	{QLatin1String("PE"), 5, 1},
	{QLatin1String("DAZ"), 6, 1},
	{QLatin1String("IM"), 7, 1},
	{QLatin1String("DM"), 8, 1},
	{QLatin1String("ZM"), 9, 1},
	{QLatin1String("OM"), 10, 1},
	{QLatin1String("UM"), 11, 1},
	{QLatin1String("PM"), 12, 1},
	{QLatin1String("RC"), 13, 2, roundingStrings},
	{QLatin1String("FZ"), 15, 1},
};

const std::vector<RegisterViewModelBase::BitFieldDescriptionEx> DR6Description = {
	{"B0", 0, 1},
	{"B1", 1, 1},
	{"B2", 2, 1},
	{"B3", 3, 1},
	{"BD", 13, 1},
	{"BS", 14, 1},
	{"BT", 15, 1},
	{"RTM", 16, 1},
};

const std::vector<QString> DR7_RW = {
	QObject::tr("Execution"),
	QObject::tr("Data Writes"),
	QObject::tr("I/O"),
	QObject::tr("Data R/W"),
};

const std::vector<QString> DR7_LEN = {
	QObject::tr("1 byte"),
	QObject::tr("2 bytes"),
	QObject::tr("8 bytes"), // sic!
	QObject::tr("4 bytes"),
};

std::vector<RegisterViewModelBase::BitFieldDescriptionEx> DR7Description = {
	{QLatin1String("L0"), 0, 1},
	{QLatin1String("G0"), 1, 1},
	{QLatin1String("L1"), 2, 1},
	{QLatin1String("G1"), 3, 1},
	{QLatin1String("L2"), 4, 1},
	{QLatin1String("G2"), 5, 1},
	{QLatin1String("L3"), 6, 1},
	{QLatin1String("G3"), 7, 1},
	{QLatin1String("LE"), 8, 1},
	{QLatin1String("GE"), 9, 1},
	{QLatin1String("RTM"), 11, 1},
	{QLatin1String("GD"), 13, 1},
	{QLatin1String("R/W0"), 16, 2, DR7_RW},
	{QLatin1String("LEN0"), 18, 2, DR7_LEN},
	{QLatin1String("R/W1"), 20, 2, DR7_RW},
	{QLatin1String("LEN1"), 22, 2, DR7_LEN},
	{QLatin1String("R/W2"), 24, 2, DR7_RW},
	{QLatin1String("LEN2"), 26, 2, DR7_LEN},
	{QLatin1String("R/W3"), 28, 2, DR7_RW},
	{QLatin1String("LEN3"), 30, 2, DR7_LEN},
};

void addGPRs32(RegisterViewModelBase::Category *gprs32) {
	gprs32->addRegister(std::make_unique<Regs<32>::GPR>("EAX"));
	gprs32->addRegister(std::make_unique<Regs<32>::GPR>("ECX"));
	gprs32->addRegister(std::make_unique<Regs<32>::GPR>("EDX"));
	gprs32->addRegister(std::make_unique<Regs<32>::GPR>("EBX"));
	gprs32->addRegister(std::make_unique<Regs<32>::GPR>("ESP"));
	gprs32->addRegister(std::make_unique<Regs<32>::GPR>("EBP"));
	gprs32->addRegister(std::make_unique<Regs<32>::GPR>("ESI"));
	gprs32->addRegister(std::make_unique<Regs<32>::GPR>("EDI"));
}

void addGPRs64(RegisterViewModelBase::Category *gprs64) {
	gprs64->addRegister(std::make_unique<Regs<64>::GPR>("RAX"));
	gprs64->addRegister(std::make_unique<Regs<64>::GPR>("RCX"));
	gprs64->addRegister(std::make_unique<Regs<64>::GPR>("RDX"));
	gprs64->addRegister(std::make_unique<Regs<64>::GPR>("RBX"));
	gprs64->addRegister(std::make_unique<Regs<64>::GPR>("RSP"));
	gprs64->addRegister(std::make_unique<Regs<64>::GPR>("RBP"));
	gprs64->addRegister(std::make_unique<Regs<64>::GPR>("RSI"));
	gprs64->addRegister(std::make_unique<Regs<64>::GPR>("RDI"));
	gprs64->addRegister(std::make_unique<Regs<64>::GPR>("R8"));
	gprs64->addRegister(std::make_unique<Regs<64>::GPR>("R9"));
	gprs64->addRegister(std::make_unique<Regs<64>::GPR>("R10"));
	gprs64->addRegister(std::make_unique<Regs<64>::GPR>("R11"));
	gprs64->addRegister(std::make_unique<Regs<64>::GPR>("R12"));
	gprs64->addRegister(std::make_unique<Regs<64>::GPR>("R13"));
	gprs64->addRegister(std::make_unique<Regs<64>::GPR>("R14"));
	gprs64->addRegister(std::make_unique<Regs<64>::GPR>("R15"));
}

template <std::size_t bitSize>
void addGenStatusRegs(RegisterViewModelBase::Category *cat) {
	using Rs = Regs<bitSize>;
	cat->addRegister(std::make_unique<typename Rs::IP>(Rs::namePrefix() + QString("IP")));
	cat->addRegister(std::make_unique<typename Rs::FLAGS>(Rs::namePrefix() + QString("FLAGS"), flagsDescription));
}

void addSegRegs(RegisterViewModelBase::Category *cat) {
	cat->addRegister(std::make_unique<SegmentReg>("ES"));
	cat->addRegister(std::make_unique<SegmentReg>("CS"));
	cat->addRegister(std::make_unique<SegmentReg>("SS"));
	cat->addRegister(std::make_unique<SegmentReg>("DS"));
	cat->addRegister(std::make_unique<SegmentReg>("FS"));
	cat->addRegister(std::make_unique<SegmentReg>("GS"));
}

template <std::size_t bitSize>
void addFPURegs(RegisterViewModelBase::Category *fpuRegs) {
	for (std::size_t i = 0; i < FPU_REG_COUNT; ++i) {
		fpuRegs->addRegister(std::make_unique<FPUReg>(QString("R%1").arg(i)));
	}
	fpuRegs->addRegister(std::make_unique<FPUWord>("FCR", FCRDescription));
	fpuRegs->addRegister(std::make_unique<FPUWord>("FSR", FSRDescription));
	fpuRegs->addRegister(std::make_unique<FPUWord>("FTR", FTRDescription));
	fpuRegs->addRegister(std::make_unique<FOPCReg>("FOP"));
	using Rs = Regs<bitSize>;
	fpuRegs->addRegister(std::make_unique<SegmentReg>("FIS"));
	fpuRegs->addRegister(std::make_unique<typename Rs::IP>("FIP"));
	fpuRegs->addRegister(std::make_unique<SegmentReg>("FDS"));
	fpuRegs->addRegister(std::make_unique<typename Rs::IP>("FDP"));
}

template <std::size_t bitSize>
void addDebugRegs(RegisterViewModelBase::Category *dbgRegs) {
	using Rs = Regs<bitSize>;
	for (std::size_t i = 0; i < 4; ++i) {
		dbgRegs->addRegister(std::make_unique<typename Rs::IP>(QString("DR%1").arg(i)));
	}

	dbgRegs->addRegister(std::make_unique<typename Rs::FLAGS>(QString("DR6"), DR6Description));
	dbgRegs->addRegister(std::make_unique<typename Rs::FLAGS>(QString("DR7"), DR7Description));
}

const std::vector<NumberDisplayMode> MMXFormats = {
	NumberDisplayMode::Hex,
	NumberDisplayMode::Signed,
	NumberDisplayMode::Unsigned,
};

const std::vector<NumberDisplayMode> SSEAVXFormats = {
	NumberDisplayMode::Hex,
	NumberDisplayMode::Signed,
	NumberDisplayMode::Unsigned,
	NumberDisplayMode::Float,
};

void addMMXRegs(RegisterViewModelBase::SIMDCategory *mmxRegs) {
	using namespace RegisterViewModelBase;
	// TODO: MMXReg should have possibility to be shown in byte/word/dword signed/unsigned/hex formats
	for (std::size_t i = 0; i < MMX_REG_COUNT; ++i) {
		mmxRegs->addRegister(std::make_unique<MMXReg>(QString("MM%1").arg(i), MMXFormats));
	}
}

void addSSERegs(RegisterViewModelBase::SIMDCategory *sseRegs, unsigned regCount) {
	for (std::size_t i = 0; i < regCount; ++i) {
		sseRegs->addRegister(std::make_unique<SSEReg>(QString("XMM%1").arg(i), SSEAVXFormats));
	}

	sseRegs->addRegister(std::make_unique<MXCSR>("MXCSR", MXCSRDescription));
}

void addAVXRegs(RegisterViewModelBase::SIMDCategory *avxRegs, unsigned regCount) {
	for (std::size_t i = 0; i < regCount; ++i) {
		avxRegs->addRegister(std::make_unique<AVXReg>(QString("YMM%1").arg(i), SSEAVXFormats));
	}
	avxRegs->addRegister(std::make_unique<MXCSR>("MXCSR", MXCSRDescription));
}

}

/**
 * @brief RegisterViewModel::data
 * @param index
 * @param role
 * @return
 */
QVariant RegisterViewModel::data(const QModelIndex &index, int role) const {
	if (role == FixedLengthRole) {

		using namespace RegisterViewModelBase;

		const auto reg  = static_cast<RegisterViewItem *>(index.internalPointer());
		const auto name = reg->data(NAME_COLUMN).toString();

		if (index.column() == NAME_COLUMN) {
			if (name == "R8" || name == "R9") {
				return 3;
			}

			if (name.startsWith("XMM") || name.startsWith("YMM")) {
				if (mode == CpuMode::IA32) return 4;
				if (mode == CpuMode::AMD64) return 5;
				return {};
			}
		}
	}
	return Model::data(index, role);
}

/**
 * @brief RegisterViewModel::RegisterViewModel
 * @param cpuSuppFlags
 * @param parent
 */
RegisterViewModel::RegisterViewModel(int cpuSuppFlags, QObject *parent)
	: RegisterViewModelBase::Model(parent),
	  gprs32(addCategory(tr("General Purpose"))),
	  gprs64(addCategory(tr("General Purpose"))),
	  genStatusRegs32(addCategory(tr("General Status"))),
	  genStatusRegs64(addCategory(tr("General Status"))),
	  segRegs(addCategory(tr("Segment"))),
	  dbgRegs32(addCategory(tr("Debug"))),
	  dbgRegs64(addCategory(tr("Debug"))),
	  fpuRegs32(addFPUCategory(tr("FPU"))),
	  fpuRegs64(addFPUCategory(tr("FPU"))),
	  mmxRegs(addSIMDCategory(tr("MMX"), MMXFormats)),
	  sseRegs32(addSIMDCategory(tr("SSE"), SSEAVXFormats)),
	  sseRegs64(addSIMDCategory(tr("SSE"), SSEAVXFormats)),
	  avxRegs32(addSIMDCategory(tr("AVX"), SSEAVXFormats)),
	  avxRegs64(addSIMDCategory(tr("AVX"), SSEAVXFormats)) {
	addGPRs32(gprs32);
	addGPRs64(gprs64);

	addGenStatusRegs<32>(genStatusRegs32);
	addGenStatusRegs<64>(genStatusRegs64);

	addSegRegs(segRegs);

	addFPURegs<32>(fpuRegs32);
	addFPURegs<64>(fpuRegs64);

	addDebugRegs<32>(dbgRegs32);
	addDebugRegs<64>(dbgRegs64);

	if (cpuSuppFlags & CPUFeatureBits::MMX) {
		addMMXRegs(mmxRegs);
	}

	if (cpuSuppFlags & CPUFeatureBits::SSE) {
		addSSERegs(sseRegs32, SSE_REG_COUNT32);
		addSSERegs(sseRegs64, SSE_REG_COUNT64);
	}

	if (cpuSuppFlags & CPUFeatureBits::AVX) {
		addAVXRegs(avxRegs32, AVX_REG_COUNT32);
		addAVXRegs(avxRegs64, AVX_REG_COUNT64);
	}

	setCpuMode(CpuMode::UNKNOWN);
}

template <typename RegType, typename ValueType>
void updateRegister(RegisterViewModelBase::Category *cat, int row, ValueType value, const QString &comment, const char *nameToCheck = nullptr) {
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

void RegisterViewModel::updateGPR(std::size_t i, edb::value32 val, const QString &comment) {
	Q_ASSERT(int(i) < gprs32->childCount());
	updateRegister<GPR32>(gprs32, static_cast<int>(i), val, comment);
}

void RegisterViewModel::updateGPR(std::size_t i, edb::value64 val, const QString &comment) {
	Q_ASSERT(int(i) < gprs64->childCount());
	updateRegister<GPR64>(gprs64, static_cast<int>(i), val, comment);
}

void RegisterViewModel::updateIP(edb::value64 value, const QString &comment) {
	updateRegister<RIP>(genStatusRegs64, RIP_ROW, value, comment, "RIP");
}

void RegisterViewModel::updateIP(edb::value32 value, const QString &comment) {
	updateRegister<EIP>(genStatusRegs32, EIP_ROW, value, comment, "EIP");
}

void RegisterViewModel::updateFlags(edb::value64 value, const QString &comment) {
	updateRegister<RFLAGS>(genStatusRegs64, RFLAGS_ROW, value, comment);
}

void RegisterViewModel::updateFlags(edb::value32 value, const QString &comment) {
	updateRegister<EFLAGS>(genStatusRegs32, EFLAGS_ROW, value, comment);
}

void RegisterViewModel::updateSegReg(std::size_t i, edb::value16 value, const QString &comment) {
	updateRegister<SegmentReg>(segRegs, i, value, comment);
}

RegisterViewModelBase::FPUCategory *RegisterViewModel::getFPUcat() const {
	switch (mode) {
	case CpuMode::IA32:
		return fpuRegs32;
	case CpuMode::AMD64:
		return fpuRegs64;
	default:
		return nullptr;
	}
}

void RegisterViewModel::updateFPUReg(std::size_t i, edb::value80 value, const QString &comment) {
	const auto cat = getFPUcat();
	Q_ASSERT(int(i) < cat->childCount());
	updateRegister<FPUReg>(cat, static_cast<int>(i), value, comment);
}

void RegisterViewModel::updateFCR(edb::value16 value, const QString &comment) {
	updateRegister<FPUWord>(getFPUcat(), FCR_ROW, value, comment, "FCR");
}

void RegisterViewModel::updateFTR(edb::value16 value, const QString &comment) {
	updateRegister<FPUWord>(getFPUcat(), FTR_ROW, value, comment, "FTR");
}

void RegisterViewModel::updateFSR(edb::value16 value, const QString &comment) {
	updateRegister<FPUWord>(getFPUcat(), FSR_ROW, value, comment, "FSR");
}

void invalidate(RegisterViewModelBase::Category *cat, int row, const char *nameToCheck) {
	if (!cat) return;
	Q_ASSERT(row < cat->childCount());
	const auto reg = cat->getRegister(row);
	Q_ASSERT(!nameToCheck || reg->name() == nameToCheck);
	Q_UNUSED(nameToCheck)
	reg->invalidate();
}

void RegisterViewModel::invalidateFIP() {
	invalidate(getFPUcat(), FIP_ROW, "FIP");
}
void RegisterViewModel::invalidateFDP() {
	invalidate(getFPUcat(), FDP_ROW, "FDP");
}
void RegisterViewModel::invalidateFIS() {
	invalidate(getFPUcat(), FIS_ROW, "FIS");
}
void RegisterViewModel::invalidateFDS() {
	invalidate(getFPUcat(), FDS_ROW, "FDS");
}
void RegisterViewModel::invalidateFOP() {
	invalidate(getFPUcat(), FOP_ROW, "FOP");
}

void RegisterViewModel::updateFIP(edb::value32 value, const QString &comment) {
	updateRegister<EIP>(getFPUcat(), FIP_ROW, value, comment);
}
void RegisterViewModel::updateFIP(edb::value64 value, const QString &comment) {
	updateRegister<RIP>(getFPUcat(), FIP_ROW, value, comment);
}
void RegisterViewModel::updateFDP(edb::value32 value, const QString &comment) {
	updateRegister<EIP>(getFPUcat(), FDP_ROW, value, comment);
}
void RegisterViewModel::updateFDP(edb::value64 value, const QString &comment) {
	updateRegister<RIP>(getFPUcat(), FDP_ROW, value, comment);
}

void RegisterViewModel::updateFOP(edb::value16 value, const QString &comment) {
	updateRegister<FOPCReg>(getFPUcat(), FOP_ROW, value, comment);
}

void RegisterViewModel::updateFIS(edb::value16 value, const QString &comment) {
	updateRegister<SegmentReg>(getFPUcat(), FIS_ROW, value, comment, "FIS");
}
void RegisterViewModel::updateFDS(edb::value16 value, const QString &comment) {
	updateRegister<SegmentReg>(getFPUcat(), FDS_ROW, value, comment, "FDS");
}

void RegisterViewModel::updateDR(std::size_t i, edb::value32 value, const QString &comment) {
	Q_ASSERT(i < DBG_REG_COUNT);
	if (i < 4)
		updateRegister<EIP>(dbgRegs32, i, value, comment);
	else if (i >= 6)
		updateRegister<EFLAGS>(dbgRegs32, i - 2, value, comment);
}
void RegisterViewModel::updateDR(std::size_t i, edb::value64 value, const QString &comment) {
	Q_ASSERT(i < DBG_REG_COUNT);
	if (i < 4)
		updateRegister<RIP>(dbgRegs64, i, value, comment);
	else if (i >= 6)
		updateRegister<RFLAGS>(dbgRegs64, i - 2, value, comment);
}

void RegisterViewModel::updateMMXReg(std::size_t i, edb::value64 value, const QString &comment) {
	Q_ASSERT(i < MMX_REG_COUNT);
	if (!mmxRegs->childCount()) return;
	updateRegister<MMXReg>(mmxRegs, static_cast<int>(i), value, comment);
}
void RegisterViewModel::invalidateMMXReg(std::size_t i) {
	Q_ASSERT(i < MMX_REG_COUNT);
	if (!mmxRegs->childCount()) return;
	invalidate(mmxRegs, i);
}

std::tuple<RegisterViewModelBase::Category * /*sse*/,
		   RegisterViewModelBase::Category * /*avx*/,
		   unsigned /*maxRegs*/>
RegisterViewModel::getSSEparams() const {
	RegisterViewModelBase::Category *sseCat = nullptr;
	RegisterViewModelBase::Category *avxCat = nullptr;
	unsigned sseRegMax                      = 0;
	switch (mode) {
	case CpuMode::IA32:
		sseRegMax = SSE_REG_COUNT32;
		sseCat    = sseRegs32;
		avxCat    = avxRegs32;
		break;
	case CpuMode::AMD64:
		sseRegMax = SSE_REG_COUNT64;
		sseCat    = sseRegs64;
		avxCat    = avxRegs64;
		break;
	default:
		break;
	}
	return std::make_tuple(sseCat, avxCat, sseRegMax);
}

void RegisterViewModel::updateSSEReg(std::size_t i, edb::value128 value, const QString &comment) {
	RegisterViewModelBase::Category *sseCat, *avxCat;
	unsigned sseRegMax;
	std::tie(sseCat, avxCat, sseRegMax) = getSSEparams();
	Q_ASSERT(i < sseRegMax);
	if (!sseCat->childCount()) return;
	updateRegister<SSEReg>(sseCat, static_cast<int>(i), value, comment);
	// To avoid showing stale data in case this is called when AVX state is supported
	if (avxCat->childCount()) invalidate(avxCat, i);
}
void RegisterViewModel::invalidateSSEReg(std::size_t i) {
	RegisterViewModelBase::Category *sseCat, *avxCat;
	std::size_t sseRegMax;
	std::tie(sseCat, avxCat, sseRegMax) = getSSEparams();
	Q_ASSERT(i < sseRegMax);
	if (!sseCat->childCount()) return;
	invalidate(sseCat, i);
	// To avoid showing stale data in case this is called when AVX state is supported
	if (avxCat->childCount()) invalidate(avxCat, i);
}

void RegisterViewModel::updateAVXReg(std::size_t i, edb::value256 value, const QString &comment) {
	RegisterViewModelBase::Category *sseCat, *avxCat;
	unsigned avxRegMax;
	std::tie(sseCat, avxCat, avxRegMax) = getSSEparams();
	if (i >= avxRegMax) {
		qWarning() << Q_FUNC_INFO << ": i>AVXmax";
		return;
	}
	// update aliases
	updateRegister<SSEReg>(sseCat, static_cast<int>(i), edb::value128(value), comment);
	// update actual registers
	updateRegister<AVXReg>(avxCat, static_cast<int>(i), value, comment);
}
void RegisterViewModel::invalidateAVXReg(std::size_t i) {
	RegisterViewModelBase::Category *sseCat, *avxCat;
	std::size_t avxRegMax;
	std::tie(sseCat, avxCat, avxRegMax) = getSSEparams();
	Q_ASSERT(i < avxRegMax);
	// invalidate aliases
	invalidate(sseCat, i);
	// invalidate actual registers
	invalidate(avxCat, i);
}

void RegisterViewModel::updateMXCSR(edb::value32 value, const QString &comment) {
	RegisterViewModelBase::Category *sseCat, *avxCat;
	std::size_t avxRegMax;
	std::tie(sseCat, avxCat, avxRegMax) = getSSEparams();
	updateRegister<MXCSR>(sseCat, sseCat->childCount() - 1, value, comment, "MXCSR");
	if (avxCat->childCount())
		updateRegister<MXCSR>(avxCat, avxCat->childCount() - 1, value, comment, "MXCSR");
}
void RegisterViewModel::invalidateMXCSR() {
	RegisterViewModelBase::Category *sseCat, *avxCat;
	std::size_t avxRegMax;
	std::tie(sseCat, avxCat, avxRegMax) = getSSEparams();
	invalidate(sseCat, sseCat->childCount() - 1, "MXCSR");
	if (avxCat->childCount())
		invalidate(avxCat, avxCat->childCount() - 1, "MXCSR");
}

void RegisterViewModel::hide64BitModeCategories() {
	gprs64->hide();
	genStatusRegs64->hide();
	fpuRegs64->hide();
	dbgRegs64->hide();
	if (sseRegs64->childCount()) sseRegs64->hide();
	if (avxRegs64->childCount()) avxRegs64->hide();
}

void RegisterViewModel::hide32BitModeCategories() {
	gprs32->hide();
	genStatusRegs32->hide();
	fpuRegs32->hide();
	dbgRegs32->hide();
	if (sseRegs32->childCount()) sseRegs32->hide();
	if (avxRegs32->childCount()) avxRegs32->hide();
}

void RegisterViewModel::show64BitModeCategories() {
	gprs64->show();
	genStatusRegs64->show();
	fpuRegs64->show();
	dbgRegs64->show();
	if (sseRegs64->childCount()) sseRegs64->show();
	if (avxRegs64->childCount()) avxRegs64->show();
}

void RegisterViewModel::show32BitModeCategories() {
	gprs32->show();
	genStatusRegs32->show();
	fpuRegs32->show();
	dbgRegs32->show();
	if (sseRegs32->childCount()) sseRegs32->show();
	if (avxRegs32->childCount()) avxRegs32->show();
}

void RegisterViewModel::hideGenericCategories() {
	segRegs->hide();
	mmxRegs->hide();
}

void RegisterViewModel::showGenericCategories() {
	segRegs->show();
	if (mmxRegs->childCount()) mmxRegs->show();
}

void RegisterViewModel::setCpuMode(CpuMode newMode) {
	if (mode == newMode) return;

	beginResetModel();
	mode = newMode;
	switch (newMode) {
	case CpuMode::UNKNOWN:
		hideAll();
		break;
	case CpuMode::IA32:
		hide64BitModeCategories();
		show32BitModeCategories();
		showGenericCategories();
		break;
	case CpuMode::AMD64:
		hide32BitModeCategories();
		show64BitModeCategories();
		showGenericCategories();
		break;
	default:
		EDB_PRINT_AND_DIE("Invalid newMode value ", (long)newMode);
	}
	endResetModel();
}

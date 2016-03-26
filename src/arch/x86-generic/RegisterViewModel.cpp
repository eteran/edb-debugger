#include "RegisterViewModel.h"
#include "Util.h"
#include <cstdint>
#include <QList>

namespace
{
using util::make_unique;

template<std::size_t bitSize> struct Regs;
template<> struct Regs<32>
{
	using GPR=RegisterViewModelBase::SimpleRegister<edb::value32>;
	using IP=GPR;
	using FLAGS=RegisterViewModelBase::FlagsRegister<edb::value32>;
	static constexpr char namePrefix() { return 'E'; }
};
template<> struct Regs<64>
{
	using GPR=RegisterViewModelBase::SimpleRegister<edb::value64>;
	using IP=GPR;
	using FLAGS=RegisterViewModelBase::FlagsRegister<edb::value64>;
	static constexpr char namePrefix() { return 'R'; }
};

static constexpr const std::size_t FPU_REG_COUNT=8;
static constexpr const std::size_t MMX_REG_COUNT=FPU_REG_COUNT;
static constexpr const std::size_t DBG_REG_COUNT=8;
static constexpr const std::size_t SSE_REG_COUNT32=8;
static constexpr const std::size_t SSE_REG_COUNT64=16;
static constexpr const std::size_t AVX_REG_COUNT32=8;
static constexpr const std::size_t AVX_REG_COUNT64=16;
enum
{
	RIP_ROW, EIP_ROW=RIP_ROW,
	RFLAGS_ROW, EFLAGS_ROW=RFLAGS_ROW,
	FCR_ROW=FPU_REG_COUNT+0,
	FSR_ROW,
	FTR_ROW,
	FOP_ROW,
	FIS_ROW,
	FIP_ROW,
	FDS_ROW,
	FDP_ROW
};
using GPR32=typename Regs<32>::GPR;
using GPR64=typename Regs<64>::GPR;
using EFLAGS=typename Regs<32>::FLAGS;
using RFLAGS=typename Regs<64>::FLAGS;
using EIP=typename Regs<32>::IP;
using RIP=typename Regs<64>::IP;
using FPUReg=RegisterViewModelBase::FPURegister<edb::value80>;
using FPUWord=RegisterViewModelBase::FlagsRegister<edb::value16>;
using FOPCReg=RegisterViewModelBase::SimpleRegister<edb::value16>;
using SegmentReg=RegisterViewModelBase::SimpleRegister<edb::value16>;
using MMXReg=RegisterViewModelBase::SIMDRegister<edb::value64>;
using SSEReg=RegisterViewModelBase::SIMDRegister<edb::value128>;
using AVXReg=RegisterViewModelBase::SIMDRegister<edb::value256>;
using MXCSR=RegisterViewModelBase::FlagsRegister<edb::value32>;

std::vector<RegisterViewModelBase::BitFieldDescription> flagsDescription=
{
	{"CF",0, 1},
	{"PF",2, 1},
	{"AF",4, 1},
	{"ZF",6, 1},
	{"SF",7, 1},
	{"TF",8, 1},
	{"IF",9, 1},
	{"DF",10,1},
	{"OF",11,1},
// these commented out system flags are always zeroed by the kernel before giving out to debugger
//	{"IOPL",12,2},
//	{"NT",14,1},
	{"RF",16,1}, // is debugger-visibly set on e.g. CLI instruction segfault
//	{"VM",17,1},
	{"AC",18,1}, // this one can be set by the application and is visible to debugger
//	{"VIF",19,1},
//	{"VIP",20,1},
	{"ID",21,1} // this one can be set by the application and is visible to debugger
};

static const std::vector<QString> roundingStrings{QObject::tr("Rounding to nearest"),
												  QObject::tr("Rounding down"),
												  QObject::tr("Rounding up"),
												  QObject::tr("Rounding toward zero")};

std::vector<RegisterViewModelBase::BitFieldDescription> FCRDescription=
{
	{"IM",0, 1},
	{"DM",1, 1},
	{"ZM",2, 1},
	{"OM",3, 1},
	{"UM",4, 1},
	{"PM",5, 1},
	{"PC",8, 2,{QObject::tr("Single precision (24 bit complete mantissa)"),
				QString(),
				QObject::tr("Double precision (53 bit complete mantissa)"),
				QObject::tr("Extended precision (64 bit mantissa)")}},
	{"RC",10,2,roundingStrings},
	{"X" ,12,1}
};

std::vector<RegisterViewModelBase::BitFieldDescription> FSRDescription=
{
	{"IE" ,0, 1},
	{"DE" ,1, 1},
	{"ZE" ,2, 1},
	{"OE" ,3, 1},
	{"UE" ,4, 1},
	{"PE" ,5, 1},
	{"SF" ,6, 1},
	{"ES" ,7, 1},
	{"C0" ,8, 1},
	{"C1" ,9, 1},
	{"C2" ,10,1},
	{"TOP",11,3},
	{"C3" ,14,1},
	{"B"  ,15,1}
};

static const std::vector<QString> tagStrings{QObject::tr("Valid"),
											 QObject::tr("Zero"),
											 QObject::tr("Special"),
											 QObject::tr("Empty")};

std::vector<RegisterViewModelBase::BitFieldDescription> FTRDescription=
{
	{"T0",0, 2,tagStrings},
	{"T1",2, 2,tagStrings},
	{"T2",4, 2,tagStrings},
	{"T3",6, 2,tagStrings},
	{"T4",8, 2,tagStrings},
	{"T5",10,2,tagStrings},
	{"T6",12,2,tagStrings},
	{"T7",14,2,tagStrings}
};

std::vector<RegisterViewModelBase::BitFieldDescription> MXCSRDescription=
{
	{"IE" ,0,1},
	{"DE" ,1,1},
	{"ZE" ,2,1},
	{"OE" ,3,1},
	{"UE" ,4,1},
	{"PE" ,5,1},
	{"DAZ",6,1},
	{"IM", 7,1},
	{"DM", 8,1},
	{"ZM", 9,1},
	{"OM",10,1},
	{"UM",11,1},
	{"PM",12,1},
	{"RC",13,2,roundingStrings},
	{"FZ",15,1}
};

std::vector<RegisterViewModelBase::BitFieldDescription> DR6Description=
{
	{"B0", 0,1},
	{"B1", 1,1},
	{"B2", 2,1},
	{"B3", 3,1},
	{"BD",13,1},
	{"BS",14,1},
	{"BT",15,1},
	{"RTM",16,1}
};

static const std::vector<QString> DR7_RW{QObject::tr("Execution"),
										 QObject::tr("Data Writes"),
										 QObject::tr("I/O"),
										 QObject::tr("Data R/W")};

static const std::vector<QString> DR7_LEN{QObject::tr("1 byte"),
										  QObject::tr("2 bytes"),
										  QObject::tr("8 bytes"), // sic!
										  QObject::tr("4 bytes")};

std::vector<RegisterViewModelBase::BitFieldDescription> DR7Description=
{
	{"L0",   0,1},
	{"G0",   1,1},
	{"L1",   2,1},
	{"G1",   3,1},
	{"L2",   4,1},
	{"G2",   5,1},
	{"L3",   6,1},
	{"G3",   7,1},
	{"LE",   8,1},
	{"GE",   9,1},
	{"RTM", 11,1},
	{"GD",  13,1},
	{"R/W0",16,2,DR7_RW},
	{"LEN0",18,2,DR7_LEN},
	{"R/W1",20,2,DR7_RW},
	{"LEN1",22,2,DR7_LEN},
	{"R/W2",24,2,DR7_RW},
	{"LEN2",26,2,DR7_LEN},
	{"R/W3",28,2,DR7_RW},
	{"LEN3",30,2,DR7_LEN}
};

}

void addGPRs32(RegisterViewModelBase::Category* gprs32)
{
	gprs32->addRegister(make_unique<Regs<32>::GPR>("EAX"));
	gprs32->addRegister(make_unique<Regs<32>::GPR>("ECX"));
	gprs32->addRegister(make_unique<Regs<32>::GPR>("EDX"));
	gprs32->addRegister(make_unique<Regs<32>::GPR>("EBX"));
	gprs32->addRegister(make_unique<Regs<32>::GPR>("ESP"));
	gprs32->addRegister(make_unique<Regs<32>::GPR>("EBP"));
	gprs32->addRegister(make_unique<Regs<32>::GPR>("ESI"));
	gprs32->addRegister(make_unique<Regs<32>::GPR>("EDI"));
}

void addGPRs64(RegisterViewModelBase::Category* gprs64)
{
	gprs64->addRegister(make_unique<Regs<64>::GPR>("RAX"));
	gprs64->addRegister(make_unique<Regs<64>::GPR>("RCX"));
	gprs64->addRegister(make_unique<Regs<64>::GPR>("RDX"));
	gprs64->addRegister(make_unique<Regs<64>::GPR>("RBX"));
	gprs64->addRegister(make_unique<Regs<64>::GPR>("RSP"));
	gprs64->addRegister(make_unique<Regs<64>::GPR>("RBP"));
	gprs64->addRegister(make_unique<Regs<64>::GPR>("RSI"));
	gprs64->addRegister(make_unique<Regs<64>::GPR>("RDI"));
	gprs64->addRegister(make_unique<Regs<64>::GPR>("R8" ));
	gprs64->addRegister(make_unique<Regs<64>::GPR>("R9" ));
	gprs64->addRegister(make_unique<Regs<64>::GPR>("R10"));
	gprs64->addRegister(make_unique<Regs<64>::GPR>("R11"));
	gprs64->addRegister(make_unique<Regs<64>::GPR>("R12"));
	gprs64->addRegister(make_unique<Regs<64>::GPR>("R13"));
	gprs64->addRegister(make_unique<Regs<64>::GPR>("R14"));
	gprs64->addRegister(make_unique<Regs<64>::GPR>("R15"));
}

template<std::size_t bitSize>
void addGenStatusRegs(RegisterViewModelBase::Category* cat)
{
	using Rs=Regs<bitSize>;
	cat->addRegister(make_unique<typename Rs::IP>(Rs::namePrefix()+QString("IP")));
	cat->addRegister(make_unique<typename Rs::FLAGS>(Rs::namePrefix()+QString("FLAGS"),flagsDescription));
}

void addSegRegs(RegisterViewModelBase::Category* cat)
{
	cat->addRegister(make_unique<SegmentReg>("ES"));
	cat->addRegister(make_unique<SegmentReg>("CS"));
	cat->addRegister(make_unique<SegmentReg>("SS"));
	cat->addRegister(make_unique<SegmentReg>("DS"));
	cat->addRegister(make_unique<SegmentReg>("FS"));
	cat->addRegister(make_unique<SegmentReg>("GS"));
}

template<std::size_t bitSize>
void addFPURegs(RegisterViewModelBase::Category* fpuRegs)
{
	for(std::size_t i=0;i<FPU_REG_COUNT;++i)
		fpuRegs->addRegister(make_unique<FPUReg>(QString("R%1").arg(i)));
	fpuRegs->addRegister(make_unique<FPUWord>("FCR",FCRDescription));
	fpuRegs->addRegister(make_unique<FPUWord>("FSR",FSRDescription));
	fpuRegs->addRegister(make_unique<FPUWord>("FTR",FTRDescription));
	fpuRegs->addRegister(make_unique<FOPCReg>("FOP"));
	using Rs=Regs<bitSize>;
	fpuRegs->addRegister(make_unique<SegmentReg>("FIS"));
	fpuRegs->addRegister(make_unique<typename Rs::IP>("FIP"));
	fpuRegs->addRegister(make_unique<SegmentReg>("FDS"));
	fpuRegs->addRegister(make_unique<typename Rs::IP>("FDP"));
}

template<std::size_t bitSize>
void addDebugRegs(RegisterViewModelBase::Category* dbgRegs)
{
	using Rs=Regs<bitSize>;
	for(std::size_t i=0;i<4;++i)
		dbgRegs->addRegister(make_unique<typename Rs::IP>(QString("DR%1").arg(i)));
	dbgRegs->addRegister(make_unique<typename Rs::FLAGS>(QString("DR6"),DR6Description));
	dbgRegs->addRegister(make_unique<typename Rs::FLAGS>(QString("DR7"),DR7Description));
}

static const std::vector<NumberDisplayMode> MMXFormats{NumberDisplayMode::Hex,
													   NumberDisplayMode::Signed,
													   NumberDisplayMode::Unsigned};
static const std::vector<NumberDisplayMode> SSEAVXFormats{NumberDisplayMode::Hex,
														  NumberDisplayMode::Signed,
														  NumberDisplayMode::Unsigned,
														  NumberDisplayMode::Float};

void addMMXRegs(RegisterViewModelBase::SIMDCategory* mmxRegs)
{
	using namespace RegisterViewModelBase;
	// TODO: MMXReg should have possibility to be shown in byte/word/dword signed/unsigned/hex formats
	for(std::size_t i=0;i<MMX_REG_COUNT;++i)
		mmxRegs->addRegister(make_unique<MMXReg>(QString("MM%1").arg(i),MMXFormats));
}

void addSSERegs(RegisterViewModelBase::SIMDCategory* sseRegs, unsigned regCount)
{
	for(std::size_t i=0;i<regCount;++i)
		sseRegs->addRegister(make_unique<SSEReg>(QString("XMM%1").arg(i),SSEAVXFormats));
	sseRegs->addRegister(make_unique<MXCSR>("MXCSR",MXCSRDescription));
}

void addAVXRegs(RegisterViewModelBase::SIMDCategory* avxRegs, unsigned regCount)
{
	for(std::size_t i=0;i<regCount;++i)
		avxRegs->addRegister(make_unique<AVXReg>(QString("YMM%1").arg(i),SSEAVXFormats));
	avxRegs->addRegister(make_unique<MXCSR>("MXCSR",MXCSRDescription));
}

QVariant RegisterViewModel::data(QModelIndex const& index, int role) const
{
	if(role==FixedLengthRole)
	{
		using namespace RegisterViewModelBase;
		const auto reg=static_cast<RegisterViewItem*>(index.internalPointer());
		const auto name=reg->data(NAME_COLUMN).toString();
		if(index.column()==NAME_COLUMN)
		{
			if(name=="R8"||name=="R9")
				return 3;
			if(name.startsWith("XMM") || name.startsWith("YMM"))
			{
				if(mode==CPUMode::IA32) return 4;
				if(mode==CPUMode::AMD64) return 5;
				return {};
			}
		}
	}
	return Model::data(index,role);
}

RegisterViewModel::RegisterViewModel(int cpuSuppFlags, QObject* parent)
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
	  mmxRegs(addSIMDCategory(tr("MMX"),MMXFormats)),
	  sseRegs32(addSIMDCategory(tr("SSE"),SSEAVXFormats)),
	  sseRegs64(addSIMDCategory(tr("SSE"),SSEAVXFormats)),
	  avxRegs32(addSIMDCategory(tr("AVX"),SSEAVXFormats)),
	  avxRegs64(addSIMDCategory(tr("AVX"),SSEAVXFormats))
{
	addGPRs32(gprs32);
	addGPRs64(gprs64);

	addGenStatusRegs<32>(genStatusRegs32);
	addGenStatusRegs<64>(genStatusRegs64);

	addSegRegs(segRegs);

	addFPURegs<32>(fpuRegs32);
	addFPURegs<64>(fpuRegs64);

	addDebugRegs<32>(dbgRegs32);
	addDebugRegs<64>(dbgRegs64);

	if(cpuSuppFlags&CPUFeatureBits::MMX)
		addMMXRegs(mmxRegs);
	if(cpuSuppFlags&CPUFeatureBits::SSE)
	{
		addSSERegs(sseRegs32,SSE_REG_COUNT32);
		addSSERegs(sseRegs64,SSE_REG_COUNT64);
	}
	if(cpuSuppFlags&CPUFeatureBits::AVX)
	{
		addAVXRegs(avxRegs32,AVX_REG_COUNT32);
		addAVXRegs(avxRegs64,AVX_REG_COUNT64);
	}

	setCPUMode(CPUMode::UNKNOWN);
}

template<typename RegType, typename ValueType>
void updateRegister(RegisterViewModelBase::Category* cat, int row, ValueType value, QString const& comment, const char* nameToCheck=0)
{
	const auto reg=cat->getRegister(row);
	Q_ASSERT(dynamic_cast<RegType*>(reg));
	Q_ASSERT(!nameToCheck || reg->name()==nameToCheck); Q_UNUSED(nameToCheck);
	static_cast<RegType*>(reg)->update(value,comment);
}

void RegisterViewModel::updateGPR(std::size_t i, edb::value32 val, QString const& comment)
{
	Q_ASSERT(int(i)<gprs32->childCount());
	updateRegister<GPR32>(gprs32,i,val,comment);
}

void RegisterViewModel::updateGPR(std::size_t i, edb::value64 val, QString const& comment)
{
	Q_ASSERT(int(i)<gprs64->childCount());
	updateRegister<GPR64>(gprs64,i,val,comment);
}

void RegisterViewModel::updateIP(edb::value64 value,QString const& comment)
{
	updateRegister<RIP>(genStatusRegs64,RIP_ROW,value,comment,"RIP");
}

void RegisterViewModel::updateIP(edb::value32 value,QString const& comment)
{
	updateRegister<EIP>(genStatusRegs32,EIP_ROW,value,comment,"EIP");
}

void RegisterViewModel::updateFlags(edb::value64 value, QString const& comment)
{
	updateRegister<RFLAGS>(genStatusRegs64,RFLAGS_ROW,value,comment);
}

void RegisterViewModel::updateFlags(edb::value32 value, QString const& comment)
{
	updateRegister<EFLAGS>(genStatusRegs32,EFLAGS_ROW,value,comment);
}

void RegisterViewModel::updateSegReg(std::size_t i, edb::value16 value, QString const& comment)
{
	updateRegister<SegmentReg>(segRegs,i,value,comment);
}

RegisterViewModelBase::FPUCategory* RegisterViewModel::getFPUcat() const
{
	return mode==CPUMode::IA32 ? fpuRegs32 : mode==CPUMode::AMD64 ? fpuRegs64 : 0;
}

void RegisterViewModel::updateFPUReg(std::size_t i, edb::value80 value, QString const& comment)
{
	const auto cat=getFPUcat();
	Q_ASSERT(int(i)<cat->childCount());
	updateRegister<FPUReg>(cat,i,value,comment);
}

void RegisterViewModel::updateFCR(edb::value16 value, QString const& comment)
{
	updateRegister<FPUWord>(getFPUcat(),FCR_ROW,value,comment,"FCR");
}

void RegisterViewModel::updateFTR(edb::value16 value, QString const& comment)
{
	updateRegister<FPUWord>(getFPUcat(),FTR_ROW,value,comment,"FTR");
}

void RegisterViewModel::updateFSR(edb::value16 value, QString const& comment)
{
	updateRegister<FPUWord>(getFPUcat(),FSR_ROW,value,comment,"FSR");
}

void invalidate(RegisterViewModelBase::Category* cat, int row, const char* nameToCheck=nullptr)
{
	if(!cat) return;
	Q_ASSERT(row<cat->childCount());
	const auto reg=cat->getRegister(row);
	Q_ASSERT(!nameToCheck || reg->name()==nameToCheck); Q_UNUSED(nameToCheck);
	reg->invalidate();
}

void RegisterViewModel::invalidateFIP()
{
	invalidate(getFPUcat(),FIP_ROW,"FIP");
}
void RegisterViewModel::invalidateFDP()
{
	invalidate(getFPUcat(),FDP_ROW,"FDP");
}
void RegisterViewModel::invalidateFIS()
{
	invalidate(getFPUcat(),FIS_ROW,"FIS");
}
void RegisterViewModel::invalidateFDS()
{
	invalidate(getFPUcat(),FDS_ROW,"FDS");
}
void RegisterViewModel::invalidateFOP()
{
	invalidate(getFPUcat(),FOP_ROW,"FOP");
}

void RegisterViewModel::updateFIP(edb::value32 value, QString const& comment)
{
	updateRegister<EIP>(getFPUcat(),FIP_ROW,value,comment);
}
void RegisterViewModel::updateFIP(edb::value64 value, QString const& comment)
{
	updateRegister<RIP>(getFPUcat(),FIP_ROW,value,comment);
}
void RegisterViewModel::updateFDP(edb::value32 value, QString const& comment)
{
	updateRegister<EIP>(getFPUcat(),FDP_ROW,value,comment);
}
void RegisterViewModel::updateFDP(edb::value64 value, QString const& comment)
{
	updateRegister<RIP>(getFPUcat(),FDP_ROW,value,comment);
}

void RegisterViewModel::updateFOP(edb::value16 value, QString const& comment)
{
	updateRegister<FOPCReg>(getFPUcat(),FOP_ROW,value,comment);
}

void RegisterViewModel::updateFIS(edb::value16 value, QString const& comment)
{
	updateRegister<SegmentReg>(getFPUcat(),FIS_ROW,value,comment,"FIS");
}
void RegisterViewModel::updateFDS(edb::value16 value, QString const& comment)
{
	updateRegister<SegmentReg>(getFPUcat(),FDS_ROW,value,comment,"FDS");
}

void RegisterViewModel::updateDR(std::size_t i, edb::value32 value, QString const& comment)
{
	Q_ASSERT(i<DBG_REG_COUNT);
	if(i<4) updateRegister<EIP>(dbgRegs32,i,value,comment);
	else if(i>=6) updateRegister<EFLAGS>(dbgRegs32,i-2,value,comment);
}
void RegisterViewModel::updateDR(std::size_t i, edb::value64 value, QString const& comment)
{
	Q_ASSERT(i<DBG_REG_COUNT);
	if(i<4) updateRegister<RIP>(dbgRegs64,i,value,comment);
	else if(i>=6) updateRegister<RFLAGS>(dbgRegs64,i-2,value,comment);
}

void RegisterViewModel::updateMMXReg(std::size_t i, edb::value64 value, QString const& comment)
{
	Q_ASSERT(i<MMX_REG_COUNT);
	if(!mmxRegs->childCount()) return;
	updateRegister<MMXReg>(mmxRegs,i,value,comment);
}
void RegisterViewModel::invalidateMMXReg(std::size_t i)
{
	Q_ASSERT(i<MMX_REG_COUNT);
	if(!mmxRegs->childCount()) return;
	invalidate(mmxRegs,i);
}

std::tuple<RegisterViewModelBase::Category* /*sse*/,
		   RegisterViewModelBase::Category* /*avx*/,
		   unsigned/*maxRegs*/> RegisterViewModel::getSSEparams() const
{
	RegisterViewModelBase::Category *sseCat=0, *avxCat=0;
	unsigned sseRegMax=0;
	switch(mode)
	{
	case CPUMode::IA32:
		sseRegMax=SSE_REG_COUNT32;
		sseCat=sseRegs32;
		avxCat=avxRegs32;
		break;
	case CPUMode::AMD64:
		sseRegMax=SSE_REG_COUNT64;
		sseCat=sseRegs64;
		avxCat=avxRegs64;
		break;
	default: break;
	}
	return std::make_tuple(sseCat,avxCat,sseRegMax);
}

void RegisterViewModel::updateSSEReg(std::size_t i, edb::value128 value, QString const& comment)
{
	RegisterViewModelBase::Category *sseCat, *avxCat;
	unsigned sseRegMax;
	std::tie(sseCat,avxCat,sseRegMax)=getSSEparams();
	Q_ASSERT(i<sseRegMax);
	if(!sseCat->childCount()) return;
	updateRegister<SSEReg>(sseCat,i,value,comment);
	// To avoid showing stale data in case this is called when AVX state is supported
	if(avxCat->childCount()) invalidate(avxCat,i);
}
void RegisterViewModel::invalidateSSEReg(std::size_t i)
{
	RegisterViewModelBase::Category *sseCat, *avxCat;
	std::size_t sseRegMax;
	std::tie(sseCat,avxCat,sseRegMax)=getSSEparams();
	Q_ASSERT(i<sseRegMax);
	if(!sseCat->childCount()) return;
	invalidate(sseCat,i);
	// To avoid showing stale data in case this is called when AVX state is supported
	if(avxCat->childCount()) invalidate(avxCat,i);
}

void RegisterViewModel::updateAVXReg(std::size_t i, edb::value256 value, QString const& comment)
{
	RegisterViewModelBase::Category *sseCat, *avxCat;
	unsigned avxRegMax;
	std::tie(sseCat,avxCat,avxRegMax)=getSSEparams();
	Q_ASSERT(i<avxRegMax);
	// update aliases
	updateRegister<SSEReg>(sseCat,i,edb::value128(value),comment);
	// update actual registers
	updateRegister<AVXReg>(avxCat,i,value,comment);
}
void RegisterViewModel::invalidateAVXReg(std::size_t i)
{
	RegisterViewModelBase::Category *sseCat, *avxCat;
	std::size_t avxRegMax;
	std::tie(sseCat,avxCat,avxRegMax)=getSSEparams();
	Q_ASSERT(i<avxRegMax);
	// invalidate aliases
	invalidate(sseCat,i);
	// invalidate actual registers
	invalidate(avxCat,i);
}

void RegisterViewModel::updateMXCSR(edb::value32 value, QString const& comment)
{
	RegisterViewModelBase::Category *sseCat, *avxCat;
	std::size_t avxRegMax;
	std::tie(sseCat,avxCat,avxRegMax)=getSSEparams();
	updateRegister<MXCSR>(sseCat,sseCat->childCount()-1,value,comment,"MXCSR");
	if(avxCat->childCount())
		updateRegister<MXCSR>(avxCat,avxCat->childCount()-1,value,comment,"MXCSR");
}
void RegisterViewModel::invalidateMXCSR()
{
	RegisterViewModelBase::Category *sseCat, *avxCat;
	std::size_t avxRegMax;
	std::tie(sseCat,avxCat,avxRegMax)=getSSEparams();
	invalidate(sseCat,sseCat->childCount()-1,"MXCSR");
	if(avxCat->childCount())
		invalidate(avxCat,avxCat->childCount()-1,"MXCSR");
}

void RegisterViewModel::hide64BitModeCategories()
{
	gprs64->hide();
	genStatusRegs64->hide();
	fpuRegs64->hide();
	dbgRegs64->hide();
	if(sseRegs64->childCount()) sseRegs64->hide();
	if(avxRegs64->childCount()) avxRegs64->hide();
}

void RegisterViewModel::hide32BitModeCategories()
{
	gprs32->hide();
	genStatusRegs32->hide();
	fpuRegs32->hide();
	dbgRegs32->hide();
	if(sseRegs32->childCount()) sseRegs32->hide();
	if(avxRegs32->childCount()) avxRegs32->hide();
}

void RegisterViewModel::show64BitModeCategories()
{
	gprs64->show();
	genStatusRegs64->show();
	fpuRegs64->show();
	dbgRegs64->show();
	if(sseRegs64->childCount()) sseRegs64->show();
	if(avxRegs64->childCount()) avxRegs64->show();
}

void RegisterViewModel::show32BitModeCategories()
{
	gprs32->show();
	genStatusRegs32->show();
	fpuRegs32->show();
	dbgRegs32->show();
	if(sseRegs32->childCount()) sseRegs32->show();
	if(avxRegs32->childCount()) avxRegs32->show();
}

void RegisterViewModel::hideGenericCategories()
{
	segRegs->hide();
	mmxRegs->hide();
}

void RegisterViewModel::showGenericCategories()
{
	segRegs->show();
	if(mmxRegs->childCount()) mmxRegs->show();
}

void RegisterViewModel::setCPUMode(CPUMode newMode)
{
	if(mode==newMode) return;

	beginResetModel();
	mode=newMode;
	switch(newMode)
	{
	case CPUMode::UNKNOWN:
		hideAll();
		break;
	case CPUMode::IA32:
		hide64BitModeCategories();
		show32BitModeCategories();
		showGenericCategories();
		break;
	case CPUMode::AMD64:
		hide32BitModeCategories();
		show64BitModeCategories();
		showGenericCategories();
		break;
	default:
		EDB_PRINT_AND_DIE("Invalid newMode value ",(long)newMode);
	}
	endResetModel();
}


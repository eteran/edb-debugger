
#include "DialogHeader.h"
#include "ELFXX.h"
#include "PE32.h"
#include "QtHelper.h"
#include "edb.h"

namespace BinaryInfoPlugin {
namespace {

Q_DECLARE_NAMESPACE_TR(BinaryInfo)

template <class Header>
QTreeWidgetItem *create_elf_magic(const Header *header) {

	auto item = new QTreeWidgetItem;

	item->setText(0, tr("Magic"));
	item->setText(1, QString("0x%1, %2, %3, %4")
						 .arg(header->e_ident[EI_MAG0], 0, 16)
						 .arg(static_cast<char>(header->e_ident[EI_MAG1]))
						 .arg(static_cast<char>(header->e_ident[EI_MAG2]))
						 .arg(static_cast<char>(header->e_ident[EI_MAG3])));

	return item;
}

template <class Header>
QTreeWidgetItem *create_elf_class(const Header *header) {

	auto item = new QTreeWidgetItem;

	item->setText(0, tr("Class"));
	switch (header->e_ident[EI_CLASS]) {
	case ELFCLASS32:
		item->setText(1, tr("32-bit"));
		break;
	case ELFCLASS64:
		item->setText(1, tr("64-bit"));
		break;
	default:
		item->setText(1, tr("Invalid"));
		break;
	}
	return item;
}

template <class Header>
QTreeWidgetItem *create_elf_data(const Header *header) {

	auto item = new QTreeWidgetItem;

	item->setText(0, tr("Data"));
	switch (header->e_ident[EI_DATA]) {
	case ELFDATA2LSB:
		item->setText(1, tr("2's complement, little endian"));
		break;
	case ELFDATA2MSB:
		item->setText(1, tr("2's complement, big endian"));
		break;
	default:
		item->setText(1, tr("Invalid"));
		break;
	}
	return item;
}

template <class Header>
QTreeWidgetItem *create_elf_version(const Header *header) {

	auto item = new QTreeWidgetItem;

	item->setText(0, tr("Version"));
	switch (header->e_ident[EI_VERSION]) {
	case EV_CURRENT:
		item->setText(1, tr("Current"));
		break;
	default:
		item->setText(1, tr("Invalid"));
		break;
	}
	return item;
}

template <class Header>
QTreeWidgetItem *create_elf_abi(const Header *header) {

	auto item = new QTreeWidgetItem;

	item->setText(0, tr("ABI"));
	switch (header->e_ident[EI_OSABI]) {
	case ELFOSABI_SYSV:
		//case ELFOSABI_NONE: // alias
		item->setText(1, tr("UNIX System V ABI"));
		break;
	case ELFOSABI_HPUX:
		item->setText(1, tr("HP-UX"));
		break;
	case ELFOSABI_NETBSD:
		item->setText(1, tr("NetBSD"));
		break;
	case ELFOSABI_GNU:
		// case ELFOSABI_LINUX: // alias
		item->setText(1, tr("GNU/Linux"));
		break;
	case ELFOSABI_SOLARIS:
		item->setText(1, tr("Sun Solaris"));
		break;
	case ELFOSABI_AIX:
		item->setText(1, tr("IBM AIX"));
		break;
	case ELFOSABI_IRIX:
		item->setText(1, tr("SGI Irix"));
		break;
	case ELFOSABI_FREEBSD:
		item->setText(1, tr("FreeBSD"));
		break;
	case ELFOSABI_TRU64:
		item->setText(1, tr("Compaq TRU64 UNIX"));
		break;
	case ELFOSABI_MODESTO:
		item->setText(1, tr("Novell Modesto"));
		break;
	case ELFOSABI_OPENBSD:
		item->setText(1, tr("OpenBSD"));
		break;
	case ELFOSABI_ARM_AEABI:
		item->setText(1, tr("ARM EABI"));
		break;
	case ELFOSABI_ARM:
		item->setText(1, tr("ARM"));
		break;
	case ELFOSABI_STANDALONE:
		item->setText(1, tr("Standalone (embedded) application"));
		break;
	default:
		item->setText(1, tr("Invalid"));
		break;
	}
	return item;
}

template <class Header>
QTreeWidgetItem *create_elf_abi_version(const Header *header) {

	auto item = new QTreeWidgetItem;

	item->setText(0, tr("ABI Version"));
	item->setText(1, QString("%1").arg(header->e_ident[EI_MAG0], 0, 10));

	return item;
}

template <class Header>
QTreeWidgetItem *create_elf_type(const Header *header) {

	auto item = new QTreeWidgetItem;

	item->setText(0, tr("Type"));

	switch (header->e_type) {
	case ET_NONE:
		item->setText(1, tr("No file type"));
		break;
	case ET_REL:
		item->setText(1, tr("Relocatable file"));
		break;
	case ET_EXEC:
		item->setText(1, tr("Executable file"));
		break;
	case ET_DYN:
		item->setText(1, tr("Shared object file"));
		break;
	case ET_CORE:
		item->setText(1, tr("Core file"));
		break;
	default:
		item->setText(1, tr("<OS Specific>"));
		break;
	}
	return item;
}

template <class Header>
QTreeWidgetItem *create_elf_machine(const Header *header) {

	auto item = new QTreeWidgetItem;

	item->setText(0, tr("Machine"));

	switch (header->e_machine) {
	case EM_NONE:
		item->setText(1, tr("No machine"));
		break;
	case EM_M32:
		item->setText(1, tr("AT&T WE 32100"));
		break;
	case EM_SPARC:
		item->setText(1, tr("SUN SPARC"));
		break;
	case EM_386:
		item->setText(1, tr("Intel 80386"));
		break;
	case EM_68K:
		item->setText(1, tr("Motorola m68k family"));
		break;
	case EM_88K:
		item->setText(1, tr("Motorola m88k family"));
		break;
	case EM_860:
		item->setText(1, tr("Intel 80860"));
		break;
	case EM_MIPS:
		item->setText(1, tr("MIPS R3000 big-endian"));
		break;
	case EM_S370:
		item->setText(1, tr("IBM System/370"));
		break;
	case EM_MIPS_RS3_LE:
		item->setText(1, tr("MIPS R3000 little-endian"));
		break;
	case EM_PARISC:
		item->setText(1, tr("HPPA"));
		break;
	case EM_VPP500:
		item->setText(1, tr("Fujitsu VPP500"));
		break;
	case EM_SPARC32PLUS:
		item->setText(1, tr("Sun's \"v8plus\""));
		break;
	case EM_960:
		item->setText(1, tr("Intel 80960"));
		break;
	case EM_PPC:
		item->setText(1, tr("PowerPC"));
		break;
	case EM_PPC64:
		item->setText(1, tr("PowerPC 64-bit"));
		break;
	case EM_S390:
		item->setText(1, tr("IBM S390"));
		break;
	case EM_V800:
		item->setText(1, tr("NEC V800 series"));
		break;
	case EM_FR20:
		item->setText(1, tr("Fujitsu FR20"));
		break;
	case EM_RH32:
		item->setText(1, tr("TRW RH-32"));
		break;
	case EM_RCE:
		item->setText(1, tr("Motorola RCE"));
		break;
	case EM_ARM:
		item->setText(1, tr("ARM"));
		break;
	case EM_FAKE_ALPHA:
		item->setText(1, tr("Digital Alpha"));
		break;
	case EM_SH:
		item->setText(1, tr("Hitachi SH"));
		break;
	case EM_SPARCV9:
		item->setText(1, tr("SPARC v9 64-bit"));
		break;
	case EM_TRICORE:
		item->setText(1, tr("Siemens Tricore"));
		break;
	case EM_ARC:
		item->setText(1, tr("Argonaut RISC Core"));
		break;
	case EM_H8_300:
		item->setText(1, tr("Hitachi H8/300"));
		break;
	case EM_H8_300H:
		item->setText(1, tr("Hitachi H8/300H"));
		break;
	case EM_H8S:
		item->setText(1, tr("Hitachi H8S"));
		break;
	case EM_H8_500:
		item->setText(1, tr("Hitachi H8/500"));
		break;
	case EM_IA_64:
		item->setText(1, tr("Intel Merced"));
		break;
	case EM_MIPS_X:
		item->setText(1, tr("Stanford MIPS-X"));
		break;
	case EM_COLDFIRE:
		item->setText(1, tr("Motorola Coldfire"));
		break;
	case EM_68HC12:
		item->setText(1, tr("Motorola M68HC12"));
		break;
	case EM_MMA:
		item->setText(1, tr("Fujitsu MMA Multimedia Accelerator"));
		break;
	case EM_PCP:
		item->setText(1, tr("Siemens PCP"));
		break;
	case EM_NCPU:
		item->setText(1, tr("Sony nCPU embeeded RISC"));
		break;
	case EM_NDR1:
		item->setText(1, tr("Denso NDR1 microprocessor"));
		break;
	case EM_STARCORE:
		item->setText(1, tr("Motorola Start*Core processor"));
		break;
	case EM_ME16:
		item->setText(1, tr("Toyota ME16 processor"));
		break;
	case EM_ST100:
		item->setText(1, tr("STMicroelectronic ST100 processor"));
		break;
	case EM_TINYJ:
		item->setText(1, tr("Advanced Logic Corp. Tinyj emb.fam"));
		break;
	case EM_X86_64:
		item->setText(1, tr("AMD x86-64 architecture"));
		break;
	case EM_PDSP:
		item->setText(1, tr("Sony DSP Processor"));
		break;
	case EM_FX66:
		item->setText(1, tr("Siemens FX66 microcontroller"));
		break;
	case EM_ST9PLUS:
		item->setText(1, tr("STMicroelectronics ST9+ 8/16 mc"));
		break;
	case EM_ST7:
		item->setText(1, tr("STmicroelectronics ST7 8 bit mc"));
		break;
	case EM_68HC16:
		item->setText(1, tr("Motorola MC68HC16 microcontroller"));
		break;
	case EM_68HC11:
		item->setText(1, tr("Motorola MC68HC11 microcontroller"));
		break;
	case EM_68HC08:
		item->setText(1, tr("Motorola MC68HC08 microcontroller"));
		break;
	case EM_68HC05:
		item->setText(1, tr("Motorola MC68HC05 microcontroller"));
		break;
	case EM_SVX:
		item->setText(1, tr("Silicon Graphics SVx"));
		break;
	case EM_ST19:
		item->setText(1, tr("STMicroelectronics ST19 8 bit mc"));
		break;
	case EM_VAX:
		item->setText(1, tr("Digital VAX"));
		break;
	case EM_CRIS:
		item->setText(1, tr("Axis Communications 32-bit embedded processor"));
		break;
	case EM_JAVELIN:
		item->setText(1, tr("Infineon Technologies 32-bit embedded processor"));
		break;
	case EM_FIREPATH:
		item->setText(1, tr("Element 14 64-bit DSP Processor"));
		break;
	case EM_ZSP:
		item->setText(1, tr("LSI Logic 16-bit DSP Processor"));
		break;
	case EM_MMIX:
		item->setText(1, tr("Donald Knuth's educational 64-bit processor"));
		break;
	case EM_HUANY:
		item->setText(1, tr("Harvard University machine-independent object files"));
		break;
	case EM_PRISM:
		item->setText(1, tr("SiTera Prism"));
		break;
	case EM_AVR:
		item->setText(1, tr("Atmel AVR 8-bit microcontroller"));
		break;
	case EM_FR30:
		item->setText(1, tr("Fujitsu FR30"));
		break;
	case EM_D10V:
		item->setText(1, tr("Mitsubishi D10V"));
		break;
	case EM_D30V:
		item->setText(1, tr("Mitsubishi D30V"));
		break;
	case EM_V850:
		item->setText(1, tr("NEC v850"));
		break;
	case EM_M32R:
		item->setText(1, tr("Mitsubishi M32R"));
		break;
	case EM_MN10300:
		item->setText(1, tr("Matsushita MN10300"));
		break;
	case EM_MN10200:
		item->setText(1, tr("Matsushita MN10200"));
		break;
	case EM_PJ:
		item->setText(1, tr("picoJava"));
		break;
	case EM_OPENRISC:
		item->setText(1, tr("OpenRISC 32-bit embedded processor"));
		break;
	case EM_ARC_A5:
		item->setText(1, tr("ARC Cores Tangent-A5"));
		break;
	case EM_XTENSA:
		item->setText(1, tr("Tensilica Xtensa Architecture"));
		break;
	default:
		item->setText(1, tr("Unknown"));
		break;
	}
	return item;
}

template <class Header>
QTreeWidgetItem *create_elf_object_version(const Header *header) {

	auto item = new QTreeWidgetItem;

	item->setText(0, tr("Object File Version"));
	item->setText(1, QString("%1").arg(header->e_version, 0, 10));

	return item;
}

template <class Header>
QTreeWidgetItem *create_elf_entry_point(const Header *header) {

	auto item = new QTreeWidgetItem;

	item->setText(0, tr("Entry Point"));
	item->setText(1, QString("0x%1").arg(header->e_entry, 0, 16));

	return item;
}

}

DialogHeader::DialogHeader(const std::shared_ptr<IRegion> &region, QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {
	ui.setupUi(this);

	if (std::unique_ptr<IBinary> binary_info = edb::v1::get_binary_info(region)) {

		if (auto elf32 = dynamic_cast<ELF32 *>(binary_info.get())) {

			auto header = reinterpret_cast<const elf32_header *>(elf32->header());

			auto root = new QTreeWidgetItem;
			root->setText(0, tr("ELF32"));

			root->addChild(create_elf_magic(header));
			root->addChild(create_elf_class(header));
			root->addChild(create_elf_data(header));
			root->addChild(create_elf_version(header));
			root->addChild(create_elf_abi(header));
			root->addChild(create_elf_abi_version(header));
			root->addChild(create_elf_type(header));
			root->addChild(create_elf_machine(header));
			root->addChild(create_elf_object_version(header));
			root->addChild(create_elf_entry_point(header));

			ui.treeWidget->insertTopLevelItem(0, root);
		}

		if (auto elf64 = dynamic_cast<ELF64 *>(binary_info.get())) {

			auto header = reinterpret_cast<const elf64_header *>(elf64->header());

			auto root = new QTreeWidgetItem;
			root->setText(0, tr("ELF64"));

			root->addChild(create_elf_magic(header));
			root->addChild(create_elf_class(header));
			root->addChild(create_elf_data(header));
			root->addChild(create_elf_version(header));
			root->addChild(create_elf_abi(header));
			root->addChild(create_elf_abi_version(header));
			root->addChild(create_elf_type(header));
			root->addChild(create_elf_machine(header));
			root->addChild(create_elf_object_version(header));
			root->addChild(create_elf_entry_point(header));

			ui.treeWidget->insertTopLevelItem(0, root);
		}

		if (auto pe32 = dynamic_cast<PE32 *>(binary_info.get())) {
			Q_UNUSED(pe32)
#if 0
			auto header = reinterpret_cast<const pe32_header *>(pe32->header());
#endif
			auto root = new QTreeWidgetItem;
			root->setText(0, tr("PE32"));
			ui.treeWidget->insertTopLevelItem(0, root);
		}
	}
}

}

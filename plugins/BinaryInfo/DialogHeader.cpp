/*
Copyright (C) 2006 - 2015 Evan Teran
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

#include "DialogHeader.h"
#include "edb.h"
#include "ELF32.h"
#include "ELF64.h"
#include "MemoryRegions.h"
#include "PE32.h"
#include <QMessageBox>
#include <QSortFilterProxyModel>

#include "ui_DialogHeader.h"

namespace BinaryInfo {
namespace {

template <class T>
QTreeWidgetItem *create_elf_magic(const T *header) {

	auto item = new QTreeWidgetItem;

	item->setText(0, QT_TRANSLATE_NOOP("BinaryInfo", "Magic"));
	item->setText(1, QString("0x%1, %2, %3, %4")
		.arg(header->e_ident[EI_MAG0], 0, 16)
		.arg(static_cast<char>(header->e_ident[EI_MAG1]))
		.arg(static_cast<char>(header->e_ident[EI_MAG2]))
		.arg(static_cast<char>(header->e_ident[EI_MAG3]))
	);

	return item;
}

template <class T>
QTreeWidgetItem *create_elf_class(const T *header) {

	auto item = new QTreeWidgetItem;

	item->setText(0, QT_TRANSLATE_NOOP("BinaryInfo", "Class"));
	switch(header->e_ident[EI_CLASS]) {
	case ELFCLASS32:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "32-bit"));
		break;
	case ELFCLASS64:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "64-bit"));
		break;
	default:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Invalid"));
		break;
	}
	return item;
}

template <class T>
QTreeWidgetItem *create_elf_data(const T *header) {

	auto item = new QTreeWidgetItem;

	item->setText(0, QT_TRANSLATE_NOOP("BinaryInfo", "Data"));
	switch(header->e_ident[EI_DATA]) {
	case ELFDATA2LSB:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "2's complement, little endian"));
		break;
	case ELFDATA2MSB:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "2's complement, big endian"));
		break;
	default:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Invalid"));
		break;
	}
	return item;
}

template <class T>
QTreeWidgetItem *create_elf_version(const T *header) {

	auto item = new QTreeWidgetItem;

	item->setText(0, QT_TRANSLATE_NOOP("BinaryInfo", "Version"));
	switch(header->e_ident[EI_VERSION]) {
	case EV_CURRENT:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Current"));
		break;
	default:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Invalid"));
		break;
	}
	return item;
}

template <class T>
QTreeWidgetItem *create_elf_abi(const T *header) {

	auto item = new QTreeWidgetItem;

	item->setText(0, QT_TRANSLATE_NOOP("BinaryInfo", "ABI"));
	switch(header->e_ident[EI_OSABI]) {
	case ELFOSABI_SYSV:
	//case ELFOSABI_NONE: // alias
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "UNIX System V ABI"));
		break;
	case ELFOSABI_HPUX:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "HP-UX"));
		break;
	case ELFOSABI_NETBSD:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "NetBSD"));
		break;
	case ELFOSABI_GNU:
	// case ELFOSABI_LINUX: // alias
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "GNU/Linux"));
		break;
	case ELFOSABI_SOLARIS:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Sun Solaris"));
		break;
	case ELFOSABI_AIX:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "IBM AIX"));
		break;
	case ELFOSABI_IRIX:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "SGI Irix"));
		break;
	case ELFOSABI_FREEBSD:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "FreeBSD"));
		break;
	case ELFOSABI_TRU64:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Compaq TRU64 UNIX"));
		break;
	case ELFOSABI_MODESTO:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Novell Modesto"));
		break;
	case ELFOSABI_OPENBSD:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "OpenBSD"));
		break;
	case ELFOSABI_ARM_AEABI:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "ARM EABI"));
		break;
	case ELFOSABI_ARM:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "ARM"));
		break;
	case ELFOSABI_STANDALONE:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Standalone (embedded) application"));
		break;
	default:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Invalid"));
		break;
	}
	return item;
}

template <class T>
QTreeWidgetItem *create_elf_abi_version(const T *header) {

	auto item = new QTreeWidgetItem;

	item->setText(0, QT_TRANSLATE_NOOP("BinaryInfo", "ABI Version"));
	item->setText(1, QString("%1").arg(header->e_ident[EI_MAG0], 0, 10));

	return item;
}

template <class T>
QTreeWidgetItem *create_elf_type(const T *header) {

	auto item = new QTreeWidgetItem;

	item->setText(0, QT_TRANSLATE_NOOP("BinaryInfo", "Type"));

	switch(header->e_type) {
	case ET_NONE:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "No file type"));
		break;
	case ET_REL:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Relocatable file"));
		break;
	case ET_EXEC:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Executable file"));
		break;
	case ET_DYN:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Shared object file"));
		break;
	case ET_CORE:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Core file"));
		break;
	default:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "<OS Specific>"));
		break;
	}
	return item;
}

template <class T>
QTreeWidgetItem *create_elf_machine(const T *header) {

	auto item = new QTreeWidgetItem;

	item->setText(0, QT_TRANSLATE_NOOP("BinaryInfo", "Machine"));

	switch(header->e_machine) {
	case EM_NONE:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "No machine"));
		break;
	case EM_M32:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "AT&T WE 32100"));
		break;
	case EM_SPARC:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "SUN SPARC"));
		break;
	case EM_386:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Intel 80386"));
		break;
	case EM_68K:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Motorola m68k family"));
		break;
	case EM_88K:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Motorola m88k family"));
		break;
	case EM_860:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Intel 80860"));
		break;
	case EM_MIPS:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "MIPS R3000 big-endian"));
		break;
	case EM_S370:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "IBM System/370"));
		break;
	case EM_MIPS_RS3_LE:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "MIPS R3000 little-endian"));
		break;
	case EM_PARISC:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "HPPA"));
		break;
	case EM_VPP500:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Fujitsu VPP500"));
		break;
	case EM_SPARC32PLUS:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Sun's \"v8plus\""));
		break;
	case EM_960:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Intel 80960"));
		break;
	case EM_PPC:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "PowerPC"));
		break;
	case EM_PPC64:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "PowerPC 64-bit"));
		break;
	case EM_S390:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "IBM S390"));
		break;
	case EM_V800:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "NEC V800 series"));
		break;
	case EM_FR20:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Fujitsu FR20"));
		break;
	case EM_RH32:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "TRW RH-32"));
		break;
	case EM_RCE:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Motorola RCE"));
		break;
	case EM_ARM:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "ARM"));
		break;
	case EM_FAKE_ALPHA:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Digital Alpha"));
		break;
	case EM_SH:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Hitachi SH"));
		break;
	case EM_SPARCV9:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "SPARC v9 64-bit"));
		break;
	case EM_TRICORE:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Siemens Tricore"));
		break;
	case EM_ARC:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Argonaut RISC Core"));
		break;
	case EM_H8_300:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Hitachi H8/300"));
		break;
	case EM_H8_300H:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Hitachi H8/300H"));
		break;
	case EM_H8S:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Hitachi H8S"));
		break;
	case EM_H8_500:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Hitachi H8/500"));
		break;
	case EM_IA_64:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Intel Merced"));
		break;
	case EM_MIPS_X:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Stanford MIPS-X"));
		break;
	case EM_COLDFIRE:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Motorola Coldfire"));
		break;
	case EM_68HC12:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Motorola M68HC12"));
		break;
	case EM_MMA:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Fujitsu MMA Multimedia Accelerator"));
		break;
	case EM_PCP:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Siemens PCP"));
		break;
	case EM_NCPU:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Sony nCPU embeeded RISC"));
		break;
	case EM_NDR1:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Denso NDR1 microprocessor"));
		break;
	case EM_STARCORE:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Motorola Start*Core processor"));
		break;
	case EM_ME16:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Toyota ME16 processor"));
		break;
	case EM_ST100:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "STMicroelectronic ST100 processor"));
		break;
	case EM_TINYJ:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Advanced Logic Corp. Tinyj emb.fam"));
		break;
	case EM_X86_64:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "AMD x86-64 architecture"));
		break;
	case EM_PDSP:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Sony DSP Processor"));
		break;
	case EM_FX66:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Siemens FX66 microcontroller"));
		break;
	case EM_ST9PLUS:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "STMicroelectronics ST9+ 8/16 mc"));
		break;
	case EM_ST7:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "STmicroelectronics ST7 8 bit mc"));
		break;
	case EM_68HC16:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Motorola MC68HC16 microcontroller"));
		break;
	case EM_68HC11:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Motorola MC68HC11 microcontroller"));
		break;
	case EM_68HC08:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Motorola MC68HC08 microcontroller"));
		break;
	case EM_68HC05:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Motorola MC68HC05 microcontroller"));
		break;
	case EM_SVX:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Silicon Graphics SVx"));
		break;
	case EM_ST19:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "STMicroelectronics ST19 8 bit mc"));
		break;
	case EM_VAX:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Digital VAX"));
		break;
	case EM_CRIS:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Axis Communications 32-bit embedded processor"));
		break;
	case EM_JAVELIN:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Infineon Technologies 32-bit embedded processor"));
		break;
	case EM_FIREPATH:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Element 14 64-bit DSP Processor"));
		break;
	case EM_ZSP:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "LSI Logic 16-bit DSP Processor"));
		break;
	case EM_MMIX:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Donald Knuth's educational 64-bit processor"));
		break;
	case EM_HUANY:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Harvard University machine-independent object files"));
		break;
	case EM_PRISM:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "SiTera Prism"));
		break;
	case EM_AVR:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Atmel AVR 8-bit microcontroller"));
		break;
	case EM_FR30:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Fujitsu FR30"));
		break;
	case EM_D10V:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Mitsubishi D10V"));
		break;
	case EM_D30V:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Mitsubishi D30V"));
		break;
	case EM_V850:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "NEC v850"));
		break;
	case EM_M32R:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Mitsubishi M32R"));
		break;
	case EM_MN10300:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Matsushita MN10300"));
		break;
	case EM_MN10200:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Matsushita MN10200"));
		break;
	case EM_PJ:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "picoJava"));
		break;
	case EM_OPENRISC:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "OpenRISC 32-bit embedded processor"));
		break;
	case EM_ARC_A5:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "ARC Cores Tangent-A5"));
		break;
	case EM_XTENSA:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Tensilica Xtensa Architecture"));
		break;
	default:
		item->setText(1, QT_TRANSLATE_NOOP("BinaryInfo", "Unknown"));
		break;
	}
	return item;
}

template <class T>
QTreeWidgetItem *create_elf_object_version(const T *header) {

	auto item = new QTreeWidgetItem;

	item->setText(0, QT_TRANSLATE_NOOP("BinaryInfo", "Object File Version"));
	item->setText(1, QString("%1").arg(header->e_version, 0, 10));

	return item;
}

template <class T>
QTreeWidgetItem *create_elf_entry_point(const T *header) {

	auto item = new QTreeWidgetItem;

	item->setText(0, QT_TRANSLATE_NOOP("BinaryInfo", "Entry Point"));
	item->setText(1, QString("%1").arg(header->e_entry, 0, 16));

	return item;
}


#if 0
	elf32_off  e_phoff;     /* Program header table file offset */
	elf32_off  e_shoff;     /* Section header table file offset */
	elf32_word e_flags;     /* Processor-specific flags */
	elf32_half e_ehsize;    /* ELF header size in bytes */
	elf32_half e_phentsize; /* Program header table entry size */
	elf32_half e_phnum;     /* Program header table entry count */
	elf32_half e_shentsize; /* Section header table entry size */
	elf32_half e_shnum;     /* Section header table entry count */
	elf32_half e_shstrndx;  /* Section header string table index */
#endif

}


//------------------------------------------------------------------------------
// Name: DialogHeader
// Desc:
//------------------------------------------------------------------------------
DialogHeader::DialogHeader(QWidget *parent) : QDialog(parent), ui(new Ui::DialogHeader) {
	ui->setupUi(this);
	ui->tableView->verticalHeader()->hide();
#if QT_VERSION >= 0x050000
	ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#else
	ui->tableView->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
#endif

	filter_model_ = new QSortFilterProxyModel(this);
	connect(ui->txtSearch, SIGNAL(textChanged(const QString &)), filter_model_, SLOT(setFilterFixedString(const QString &)));
}

//------------------------------------------------------------------------------
// Name: ~DialogHeader
// Desc:
//------------------------------------------------------------------------------
DialogHeader::~DialogHeader() {
	delete ui;
}

//------------------------------------------------------------------------------
// Name: showEvent
// Desc:
//------------------------------------------------------------------------------
void DialogHeader::showEvent(QShowEvent *) {
	filter_model_->setFilterKeyColumn(3);
	filter_model_->setSourceModel(&edb::v1::memory_regions());
	ui->tableView->setModel(filter_model_);
	ui->treeWidget->clear();
}

//------------------------------------------------------------------------------
// Name: on_btnExplore_clicked
// Desc:
//------------------------------------------------------------------------------
void DialogHeader::on_btnExplore_clicked() {
	ui->treeWidget->clear();

	const QItemSelectionModel *const selModel = ui->tableView->selectionModel();
	const QModelIndexList sel = selModel->selectedRows();

	if(sel.size() == 0) {
		QMessageBox::information(
			this,
			tr("No Region Selected"),
			tr("You must select a region which is to be scanned for executable headers."));
	} else {

		for(const QModelIndex &selected_item: sel) {

			const QModelIndex index = filter_model_->mapToSource(selected_item);
			if(auto region = *reinterpret_cast<const IRegion::pointer *>(index.internalPointer())) {

				if(auto binary_info = edb::v1::get_binary_info(region)) {

					if(auto elf32 = dynamic_cast<ELF32 *>(binary_info.get())) {

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

						ui->treeWidget->insertTopLevelItem(0, root);
					}

					if(auto elf64 = dynamic_cast<ELF64 *>(binary_info.get())) {

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

						ui->treeWidget->insertTopLevelItem(0, root);
					}

					if(auto pe32 = dynamic_cast<PE32 *>(binary_info.get())) {
						Q_UNUSED(pe32);
					#if 0
						auto header = reinterpret_cast<const pe32_header *>(pe32->header());
					#endif
						auto root = new QTreeWidgetItem;
						root->setText(0, tr("PE32"));
						ui->treeWidget->insertTopLevelItem(0, root);
					}
				}
			}
		}
	}
}
}

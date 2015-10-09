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

#include "DialogROPTool.h"
#include "ByteShiftArray.h"
#include "edb.h"
#include "IDebugger.h"
#include "MemoryRegions.h"
#include "Util.h"
#include <QDebug>
#include <QHeaderView>
#include <QMessageBox>
#include <QModelIndex>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>

#include "ui_DialogROPTool.h"

namespace ROPTool {

namespace {

bool is_nop(const edb::Instruction &inst) {
	if(inst) {
		if(inst.is_nop()) {
			return true;
		}

		// TODO: does this effect flags?
		if(inst.operation() == edb::Instruction::Operation::X86_INS_MOV && inst.operand_count() == 2) {
			if(inst.operands()[0].general_type() == edb::Operand::TYPE_REGISTER && inst.operands()[1].general_type() == edb::Operand::TYPE_REGISTER) {
				if(inst.operands()[0].reg() == inst.operands()[1].reg()) {
					return true;
				}
			}

		}

		// TODO: does this effect flags?
		if(inst.operation() == edb::Instruction::Operation::X86_INS_XCHG && inst.operand_count() == 2) {
			if(inst.operands()[0].general_type() == edb::Operand::TYPE_REGISTER && inst.operands()[1].general_type() == edb::Operand::TYPE_REGISTER) {
				if(inst.operands()[0].reg() == inst.operands()[1].reg()) {
					return true;
				}
			}

		}

		// TODO: support LEA reg, [reg]

	}
	return false;
}

}

//------------------------------------------------------------------------------
// Name: DialogROPTool
// Desc:
//------------------------------------------------------------------------------
DialogROPTool::DialogROPTool(QWidget *parent) : QDialog(parent), ui(new Ui::DialogROPTool) {
	ui->setupUi(this);
	ui->tableView->verticalHeader()->hide();
#if QT_VERSION >= 0x050000
	ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#else
	ui->tableView->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
#endif

	filter_model_ = new QSortFilterProxyModel(this);
	connect(ui->txtSearch, SIGNAL(textChanged(const QString &)), filter_model_, SLOT(setFilterFixedString(const QString &)));

	result_model_ = new QStandardItemModel(this);
	result_filter_ = new ResultFilterProxy(this);
	result_filter_->setSourceModel(result_model_);
	ui->listView->setModel(result_filter_);
}

//------------------------------------------------------------------------------
// Name: ~DialogROPTool
// Desc:
//------------------------------------------------------------------------------
DialogROPTool::~DialogROPTool() {
	delete ui;
}

//------------------------------------------------------------------------------
// Name: on_listView_itemDoubleClicked
// Desc: follows the found item in the data view
//------------------------------------------------------------------------------
void DialogROPTool::on_listView_doubleClicked(const QModelIndex &index) {
	bool ok;
	const edb::address_t addr = index.data(Qt::UserRole).toULongLong(&ok);
	if(ok) {
		edb::v1::jump_to_address(addr);
	}
}

//------------------------------------------------------------------------------
// Name: showEvent
// Desc:
//------------------------------------------------------------------------------
void DialogROPTool::showEvent(QShowEvent *) {
	filter_model_->setFilterKeyColumn(3);
	filter_model_->setSourceModel(&edb::v1::memory_regions());
	ui->tableView->setModel(filter_model_);
	ui->progressBar->setValue(0);

	result_filter_->set_mask_bit(0x01, ui->chkShowALU->isChecked());
	result_filter_->set_mask_bit(0x02, ui->chkShowStack->isChecked());
	result_filter_->set_mask_bit(0x04, ui->chkShowLogic->isChecked());
	result_filter_->set_mask_bit(0x08, ui->chkShowData->isChecked());
	result_filter_->set_mask_bit(0x10, ui->chkShowOther->isChecked());

	result_model_->clear();
}

//------------------------------------------------------------------------------
// Name: on_chkShowALU_stateChanged
// Desc:
//------------------------------------------------------------------------------
void DialogROPTool::on_chkShowALU_stateChanged(int state) {
	result_filter_->set_mask_bit(0x01, state);
}

//------------------------------------------------------------------------------
// Name: on_chkShowStack_stateChanged
// Desc:
//------------------------------------------------------------------------------
void DialogROPTool::on_chkShowStack_stateChanged(int state) {
	result_filter_->set_mask_bit(0x02, state);
}

//------------------------------------------------------------------------------
// Name: on_chkShowLogic_stateChanged
// Desc:
//------------------------------------------------------------------------------
void DialogROPTool::on_chkShowLogic_stateChanged(int state) {
	result_filter_->set_mask_bit(0x04, state);
}

//------------------------------------------------------------------------------
// Name: on_chkShowData_stateChanged
// Desc:
//------------------------------------------------------------------------------
void DialogROPTool::on_chkShowData_stateChanged(int state) {
	result_filter_->set_mask_bit(0x08, state);
}

//------------------------------------------------------------------------------
// Name: on_chkShowOther_stateChanged
// Desc:
//------------------------------------------------------------------------------
void DialogROPTool::on_chkShowOther_stateChanged(int state) {
	result_filter_->set_mask_bit(0x10, state);
}

//------------------------------------------------------------------------------
// Name: set_gadget_role
// Desc:
//------------------------------------------------------------------------------
void DialogROPTool::set_gadget_role(QStandardItem *item, const edb::Instruction &inst1) {

	switch(inst1.operation()) {
	case edb::Instruction::Operation::X86_INS_ADD:
	case edb::Instruction::Operation::X86_INS_ADC:
	case edb::Instruction::Operation::X86_INS_SUB:
	case edb::Instruction::Operation::X86_INS_SBB:
	case edb::Instruction::Operation::X86_INS_IMUL:
	case edb::Instruction::Operation::X86_INS_MUL:
	case edb::Instruction::Operation::X86_INS_IDIV:
	case edb::Instruction::Operation::X86_INS_DIV:
	case edb::Instruction::Operation::X86_INS_INC:
	case edb::Instruction::Operation::X86_INS_DEC:
	case edb::Instruction::Operation::X86_INS_NEG:
	case edb::Instruction::Operation::X86_INS_CMP:
	case edb::Instruction::Operation::X86_INS_DAA:
	case edb::Instruction::Operation::X86_INS_DAS:
	case edb::Instruction::Operation::X86_INS_AAA:
	case edb::Instruction::Operation::X86_INS_AAS:
	case edb::Instruction::Operation::X86_INS_AAM:
	case edb::Instruction::Operation::X86_INS_AAD:
		// ALU ops
		item->setData(0x01, Qt::UserRole + 1);
		break;
	case edb::Instruction::Operation::X86_INS_PUSH:
	case edb::Instruction::Operation::X86_INS_PUSHAW:
	case edb::Instruction::Operation::X86_INS_PUSHAL:
	case edb::Instruction::Operation::X86_INS_POP:
	case edb::Instruction::Operation::X86_INS_POPAW:
	case edb::Instruction::Operation::X86_INS_POPAL:
		// stack ops
		item->setData(0x02, Qt::UserRole + 1);
		break;
	case edb::Instruction::Operation::X86_INS_AND:
	case edb::Instruction::Operation::X86_INS_OR:
	case edb::Instruction::Operation::X86_INS_XOR:
	case edb::Instruction::Operation::X86_INS_NOT:
	case edb::Instruction::Operation::X86_INS_SAR:
	case edb::Instruction::Operation::X86_INS_SAL:
	case edb::Instruction::Operation::X86_INS_SHR:
	case edb::Instruction::Operation::X86_INS_SHL:
	case edb::Instruction::Operation::X86_INS_SHRD:
	case edb::Instruction::Operation::X86_INS_SHLD:
	case edb::Instruction::Operation::X86_INS_ROR:
	case edb::Instruction::Operation::X86_INS_ROL:
	case edb::Instruction::Operation::X86_INS_RCR:
	case edb::Instruction::Operation::X86_INS_RCL:
	case edb::Instruction::Operation::X86_INS_BT:
	case edb::Instruction::Operation::X86_INS_BTS:
	case edb::Instruction::Operation::X86_INS_BTR:
	case edb::Instruction::Operation::X86_INS_BTC:
	case edb::Instruction::Operation::X86_INS_BSF:
	case edb::Instruction::Operation::X86_INS_BSR:
		// logic ops
		item->setData(0x04, Qt::UserRole + 1);
		break;
	case edb::Instruction::Operation::X86_INS_MOV:
	case edb::Instruction::Operation::X86_INS_MOVABS:
	case edb::Instruction::Operation::X86_INS_CMOVA:
	case edb::Instruction::Operation::X86_INS_CMOVAE:
	case edb::Instruction::Operation::X86_INS_CMOVB:
	case edb::Instruction::Operation::X86_INS_CMOVBE:
	case edb::Instruction::Operation::X86_INS_CMOVE:
	case edb::Instruction::Operation::X86_INS_CMOVG:
	case edb::Instruction::Operation::X86_INS_CMOVGE:
	case edb::Instruction::Operation::X86_INS_CMOVL:
	case edb::Instruction::Operation::X86_INS_CMOVLE:
	case edb::Instruction::Operation::X86_INS_CMOVNE:
	case edb::Instruction::Operation::X86_INS_CMOVNO:
	case edb::Instruction::Operation::X86_INS_CMOVNP:
	case edb::Instruction::Operation::X86_INS_CMOVNS:
	case edb::Instruction::Operation::X86_INS_CMOVO:
	case edb::Instruction::Operation::X86_INS_CMOVP:
	case edb::Instruction::Operation::X86_INS_CMOVS:
	case edb::Instruction::Operation::X86_INS_XCHG:
	case edb::Instruction::Operation::X86_INS_BSWAP:
	case edb::Instruction::Operation::X86_INS_XADD:
	case edb::Instruction::Operation::X86_INS_CMPXCHG:
	case edb::Instruction::Operation::X86_INS_CWD:
	case edb::Instruction::Operation::X86_INS_CDQ:
	case edb::Instruction::Operation::X86_INS_CQO:
	case edb::Instruction::Operation::X86_INS_CDQE:
	case edb::Instruction::Operation::X86_INS_CBW:
	case edb::Instruction::Operation::X86_INS_CWDE:
	case edb::Instruction::Operation::X86_INS_MOVSX:
	case edb::Instruction::Operation::X86_INS_MOVZX:
	case edb::Instruction::Operation::X86_INS_MOVSXD:
	case edb::Instruction::Operation::X86_INS_MOVBE:
	case edb::Instruction::Operation::X86_INS_MOVSB:
	case edb::Instruction::Operation::X86_INS_MOVSW:
	case edb::Instruction::Operation::X86_INS_MOVSD:
	case edb::Instruction::Operation::X86_INS_MOVSQ:
	case edb::Instruction::Operation::X86_INS_CMPSB:
	case edb::Instruction::Operation::X86_INS_CMPSW:
	case edb::Instruction::Operation::X86_INS_CMPSD:
	case edb::Instruction::Operation::X86_INS_CMPSQ:
	case edb::Instruction::Operation::X86_INS_SCASB:
	case edb::Instruction::Operation::X86_INS_SCASW:
	case edb::Instruction::Operation::X86_INS_SCASD:
	case edb::Instruction::Operation::X86_INS_SCASQ:
	case edb::Instruction::Operation::X86_INS_LODSB:
	case edb::Instruction::Operation::X86_INS_LODSW:
	case edb::Instruction::Operation::X86_INS_LODSD:
	case edb::Instruction::Operation::X86_INS_LODSQ:
	case edb::Instruction::Operation::X86_INS_STOSB:
	case edb::Instruction::Operation::X86_INS_STOSW:
	case edb::Instruction::Operation::X86_INS_STOSD:
	case edb::Instruction::Operation::X86_INS_STOSQ:
	case edb::Instruction::Operation::X86_INS_CMPXCHG8B:
	case edb::Instruction::Operation::X86_INS_CMPXCHG16B:
		// data ops
		item->setData(0x08, Qt::UserRole + 1);
		break;
	default:
		// other ops
		item->setData(0x10, Qt::UserRole + 1);
		break;

	}
}

//------------------------------------------------------------------------------
// Name: add_gadget
// Desc:
//------------------------------------------------------------------------------
void DialogROPTool::add_gadget(QList<edb::Instruction> instructions) {

	if(!instructions.isEmpty()) {
		const edb::Instruction inst1 = instructions.takeFirst();

		QString instruction_string = QString("%1").arg(QString::fromStdString(edb::v1::formatter().to_string(inst1)));
		for(const edb::Instruction &instruction: instructions) {
			instruction_string.append(QString("; %1").arg(QString::fromStdString(edb::v1::formatter().to_string(instruction))));
		}

		if(!ui->checkUnique->isChecked() || !unique_results_.contains(instruction_string)) {
			unique_results_.insert(instruction_string);

			// found a gadget
			auto item = new QStandardItem(QString("%1: %2").arg(edb::v1::format_pointer(inst1.rva()), instruction_string));

			item->setData(static_cast<qulonglong>(inst1.rva()), Qt::UserRole);
			
			// TODO: make this look for 1st non-NOP
			set_gadget_role(item, inst1);

			result_model_->insertRow(result_model_->rowCount(), item);
		}
	}
}

//------------------------------------------------------------------------------
// Name: do_find
// Desc:
//------------------------------------------------------------------------------
void DialogROPTool::do_find() {

	const QItemSelectionModel *const selModel = ui->tableView->selectionModel();
	const QModelIndexList sel = selModel->selectedRows();

	if(sel.size() == 0) {
		QMessageBox::information(
			this,
			tr("No Region Selected"),
			tr("You must select a region which is to be scanned for gadgets."));
	} else {

		unique_results_.clear();

		if(IProcess *process = edb::v1::debugger_core->process()) {
			for(const QModelIndex &selected_item: sel) {

				const QModelIndex index = filter_model_->mapToSource(selected_item);
				if(auto region = *reinterpret_cast<const IRegion::pointer *>(index.internalPointer())) {

					edb::address_t start_address     = region->start();
					const edb::address_t end_address = region->end();
					const edb::address_t orig_start  = start_address;

					ByteShiftArray bsa(32);

					while(start_address < end_address) {

						// read in the next byte
						quint8 byte;
						if(process->read_bytes(start_address, &byte, 1)) {
							bsa << byte;

							const quint8       *p = bsa.data();
							const quint8 *const l = p + bsa.size();
							edb::address_t    rva = start_address - bsa.size() + 1;

							QList<edb::Instruction> instruction_list;

							// eat up any NOPs in front...
							Q_FOREVER {
								edb::Instruction inst(p, l, rva);
								if(!is_nop(inst)) {
									break;
								}

								instruction_list << inst;
								p += inst.size();
								rva += inst.size();
							}


							edb::Instruction inst1(p, l, rva);
							if(inst1) {
								instruction_list << inst1;

								if(inst1.operation() == edb::Instruction::Operation::X86_INS_INT && inst1.operands()[0].general_type() == edb::Operand::TYPE_IMMEDIATE && (inst1.operands()[0].immediate() & 0xff) == 0x80) {
									add_gadget(instruction_list);
								} else if(inst1.operation() == edb::Instruction::Operation::X86_INS_SYSENTER) {
									add_gadget(instruction_list);
								} else if(inst1.operation() == edb::Instruction::Operation::X86_INS_SYSCALL) {
									add_gadget(instruction_list);
								} else if(is_ret(inst1)) {
									ui->progressBar->setValue(util::percentage(start_address - orig_start, region->size()));
									++start_address;
									continue;
								} else {

									p += inst1.size();
									rva += inst1.size();

									// eat up any NOPs in between...
									Q_FOREVER {
										edb::Instruction inst(p, l, rva);
										if(!is_nop(inst)) {
											break;
										}

										instruction_list << inst;
										p += inst.size();
										rva += inst.size();
									}

									edb::Instruction inst2(p, l, rva);
									if(is_ret(inst2)) {
										instruction_list << inst2;
										add_gadget(instruction_list);
									} else if(inst2 && inst2.operation() == edb::Instruction::Operation::X86_INS_POP) {
										instruction_list << inst2;
										p += inst2.size();
										rva += inst2.size();

										edb::Instruction inst3(p, l, rva);
										if(inst3 && inst3.operation() == edb::Instruction::Operation::X86_INS_JMP) {

											instruction_list << inst3;

											if(inst2.operand_count() == 1 && inst2.operands()[0].general_type() == edb::Operand::TYPE_REGISTER) {
												if(inst3.operand_count() == 1 && inst3.operands()[0].general_type() == edb::Operand::TYPE_REGISTER) {
													if(inst2.operands()[0].reg() == inst3.operands()[0].reg()) {
														add_gadget(instruction_list);
													}
												}
											}
										}
									}
								}

								// TODO(eteran): catch things like "add rsp, 8; jmp [rsp - 8]" and similar, it's rare,
								// but could happen
							}
						}

						ui->progressBar->setValue(util::percentage(start_address - orig_start, region->size()));
						++start_address;
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_btnFind_clicked
// Desc:
//------------------------------------------------------------------------------
void DialogROPTool::on_btnFind_clicked() {
	ui->btnFind->setEnabled(false);
	ui->progressBar->setValue(0);
	result_model_->clear();
	do_find();
	ui->progressBar->setValue(100);
	ui->btnFind->setEnabled(true);
}

}

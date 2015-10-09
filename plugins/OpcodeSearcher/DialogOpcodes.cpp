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

#include "DialogOpcodes.h"
#include "ByteShiftArray.h"
#include "IDebugger.h"
#include "MemoryRegions.h"
#include "ShiftBuffer.h"
#include "Util.h"
#include "edb.h"

#include <QHeaderView>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QListWidgetItem>
#include <QDebug>

#include "ui_DialogOpcodes.h"

namespace OpcodeSearcher {

namespace {
#if defined(EDB_X86)
const edb::Operand::Register STACK_REG = edb::Operand::Register::X86_REG_ESP;
#elif defined(EDB_X86_64)
const edb::Operand::Register STACK_REG = edb::Operand::Register::X86_REG_RSP;
#endif
}

//------------------------------------------------------------------------------
// Name: DialogOpcodes
// Desc:
//------------------------------------------------------------------------------
DialogOpcodes::DialogOpcodes(QWidget *parent) : QDialog(parent), ui(new Ui::DialogOpcodes) {
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
// Name: ~DialogOpcodes
// Desc:
//------------------------------------------------------------------------------
DialogOpcodes::~DialogOpcodes() {
	delete ui;
}

//------------------------------------------------------------------------------
// Name: on_listWidget_itemDoubleClicked
// Desc: follows the found item in the data view
//------------------------------------------------------------------------------
void DialogOpcodes::on_listWidget_itemDoubleClicked(QListWidgetItem *item) {
	bool ok;
	const edb::address_t addr = item->data(Qt::UserRole).toULongLong(&ok);
	if(ok) {
		edb::v1::jump_to_address(addr);
	}
}

//------------------------------------------------------------------------------
// Name: showEvent
// Desc:
//------------------------------------------------------------------------------
void DialogOpcodes::showEvent(QShowEvent *) {
	filter_model_->setFilterKeyColumn(3);
	filter_model_->setSourceModel(&edb::v1::memory_regions());
	ui->tableView->setModel(filter_model_);
	ui->progressBar->setValue(0);
	ui->listWidget->clear();
	
	
	ui->comboBox->clear();
	if(edb::v1::debuggeeIs64Bit()) {
		ui->comboBox->addItem("RAX -> RIP", 1);
		ui->comboBox->addItem("RBX -> RIP", 2);
		ui->comboBox->addItem("RCX -> RIP", 3);
		ui->comboBox->addItem("RDX -> RIP", 4);
		ui->comboBox->addItem("RBP -> RIP", 5);
		ui->comboBox->addItem("RSP -> RIP", 6);
		ui->comboBox->addItem("RSI -> RIP", 7);
		ui->comboBox->addItem("RDI -> RIP", 8);
		ui->comboBox->addItem("R8 -> RIP", 9);
		ui->comboBox->addItem("R9 -> RIP", 10);
		ui->comboBox->addItem("R10 -> RIP", 11);
		ui->comboBox->addItem("R11 -> RIP", 12);
		ui->comboBox->addItem("R12 -> RIP", 13);
		ui->comboBox->addItem("R13 -> RIP", 14);
		ui->comboBox->addItem("R14 -> RIP", 15);
		ui->comboBox->addItem("R15 -> RIP", 16);
		ui->comboBox->addItem("ANY REGISTER -> RIP", 17);
		ui->comboBox->addItem("[RSP] -> RIP", 18);
		ui->comboBox->addItem("[RSP + 8] -> RIP", 19);
		ui->comboBox->addItem("[RSP + 16] -> RIP", 20);
		ui->comboBox->addItem("[RSP - 8] -> RIP", 21);
		ui->comboBox->addItem("[RAX] -> RIP", 22);
		ui->comboBox->addItem("[RBX] -> RIP", 23);
		ui->comboBox->addItem("[RCX] -> RIP", 24);
		ui->comboBox->addItem("[RDX] -> RIP", 25);
		ui->comboBox->addItem("[RBP] -> RIP", 26);
		ui->comboBox->addItem("[RSI] -> RIP", 28);
		ui->comboBox->addItem("[RDI] -> RIP", 29);
		ui->comboBox->addItem("[R8] -> RIP", 30);
		ui->comboBox->addItem("[R9] -> RIP", 31);
		ui->comboBox->addItem("[R10] -> RIP", 32);
		ui->comboBox->addItem("[R11] -> RIP", 33);
		ui->comboBox->addItem("[R12] -> RIP", 34);
		ui->comboBox->addItem("[R13] -> RIP", 35);
		ui->comboBox->addItem("[R14] -> RIP", 36);
		ui->comboBox->addItem("[R15] -> RIP", 37);
	} else {
		ui->comboBox->addItem("EAX -> EIP", 1);
		ui->comboBox->addItem("EBX -> EIP", 2);
		ui->comboBox->addItem("ECX -> EIP", 3);
		ui->comboBox->addItem("EDX -> EIP", 4);
		ui->comboBox->addItem("EBP -> EIP", 5);
		ui->comboBox->addItem("ESP -> EIP", 6);
		ui->comboBox->addItem("ESI -> EIP", 7);
		ui->comboBox->addItem("EDI -> EIP", 8);
		ui->comboBox->addItem("ANY REGISTER -> EIP", 17);
		ui->comboBox->addItem("[ESP] -> EIP", 18);
		ui->comboBox->addItem("[ESP + 4] -> EIP", 19);
		ui->comboBox->addItem("[ESP + 8] -> EIP", 20);
		ui->comboBox->addItem("[ESP - 4] -> EIP", 21);

		ui->comboBox->addItem("[EAX] -> EIP", 22);
		ui->comboBox->addItem("[EBX] -> EIP", 23);
		ui->comboBox->addItem("[ECX] -> EIP", 24);
		ui->comboBox->addItem("[EDX] -> EIP", 25);
		ui->comboBox->addItem("[EBP] -> EIP", 26);
		ui->comboBox->addItem("[ESI] -> EIP", 28);
		ui->comboBox->addItem("[EDI] -> EIP", 29);
	}
}

//------------------------------------------------------------------------------
// Name: add_result
// Desc:
//------------------------------------------------------------------------------
void DialogOpcodes::add_result(QList<edb::Instruction> instructions, edb::address_t rva) {
	if(!instructions.isEmpty()) {
		const edb::Instruction inst1 = instructions.takeFirst();

		QString instruction_string = QString("%1: %2").arg(
			edb::v1::format_pointer(rva),
			QString::fromStdString(edb::v1::formatter().to_string(inst1)));


		for(const edb::Instruction &instruction: instructions) {
			instruction_string.append(QString("; %1").arg(QString::fromStdString(edb::v1::formatter().to_string(instruction))));
		}

		auto item = new QListWidgetItem(instruction_string);

		item->setData(Qt::UserRole, rva);
		ui->listWidget->addItem(item);
	}
}

//------------------------------------------------------------------------------
// Name: test_deref_reg_to_ip
// Desc:
//------------------------------------------------------------------------------
template <edb::Operand::Register REG>
void DialogOpcodes::test_deref_reg_to_ip(const OpcodeData &data, edb::address_t start_address) {
	const quint8 *p = data.data;
	const quint8 *last = p + sizeof(data);

	edb::Instruction inst(p, last, 0);

	if(inst) {
		const edb::Operand &op1 = inst.operands()[0];
		switch(inst.operation()) {
		case edb::Instruction::Operation::X86_INS_JMP:
		case edb::Instruction::Operation::X86_INS_CALL:
			if(op1.general_type() == edb::Operand::TYPE_EXPRESSION) {

				if(op1.expression().displacement_type == edb::Operand::DISP_NONE) {

					if(op1.expression().base == REG && op1.expression().index == edb::Operand::Register::X86_REG_INVALID && op1.expression().scale == 1) {
						add_result((QList<edb::Instruction>() << inst), start_address);
						return;
					}

					if(op1.expression().index == REG && op1.expression().base == edb::Operand::Register::X86_REG_INVALID && op1.expression().scale == 1) {
						add_result((QList<edb::Instruction>() << inst), start_address);
						return;
					}
				}
			}
			break;
		default:
			break;
		}
	}
}

//------------------------------------------------------------------------------
// Name: test_reg_to_ip
// Desc:
//------------------------------------------------------------------------------
template <edb::Operand::Register REG>
void DialogOpcodes::test_reg_to_ip(const DialogOpcodes::OpcodeData &data, edb::address_t start_address) {

	const quint8 *p = data.data;
	const quint8 *last = p + sizeof(data);

	edb::Instruction inst(p, last, 0);

	if(inst) {
		const edb::Operand &op1 = inst.operands()[0];
		switch(inst.operation()) {
		case edb::Instruction::Operation::X86_INS_JMP:
		case edb::Instruction::Operation::X86_INS_CALL:
			if(op1.general_type() == edb::Operand::TYPE_REGISTER) {
				if(op1.reg() == REG) {
					add_result((QList<edb::Instruction>() << inst), start_address);
					return;
				}
			}
			break;

		case edb::Instruction::Operation::X86_INS_PUSH:
			if(op1.general_type() == edb::Operand::TYPE_REGISTER) {
				if(op1.reg() == REG) {

					p += inst.size();
					edb::Instruction inst2(p, last, 0);
					if(inst2) {
						const edb::Operand &op2 = inst2.operands()[0];
						
						if(is_ret(inst2)) {
							add_result((QList<edb::Instruction>() << inst << inst2), start_address);
						} else {
							switch(inst2.operation()) {
							case edb::Instruction::Operation::X86_INS_JMP:
							case edb::Instruction::Operation::X86_INS_CALL:

								if(op2.general_type() == edb::Operand::TYPE_EXPRESSION) {

									if(op2.expression().displacement_type == edb::Operand::DISP_NONE) {

										if(op2.expression().base == STACK_REG && op2.expression().index == edb::Operand::Register::X86_REG_INVALID) {
											add_result((QList<edb::Instruction>() << inst << inst2), start_address);
											return;
										}

										if(op2.expression().index == STACK_REG && op2.expression().base == edb::Operand::Register::X86_REG_INVALID) {
											add_result((QList<edb::Instruction>() << inst << inst2), start_address);
											return;
										}
									}
								}
								break;
							default:
								break;
							}
						}
					}
				}
			}
			break;
		default:
			break;
		}
	}
}

//------------------------------------------------------------------------------
// Name: test_esp_add_0
// Desc:
//------------------------------------------------------------------------------
void DialogOpcodes::test_esp_add_0(const OpcodeData &data, edb::address_t start_address) {

	const quint8 *p = data.data;
	const quint8 *last = p + sizeof(data);

	edb::Instruction inst(p, last, 0);

	if(inst) {
		const edb::Operand &op1 = inst.operands()[0];
		if(is_ret(inst)) {
			add_result((QList<edb::Instruction>() << inst), start_address);
		} else {
			switch(inst.operation()) {
			case edb::Instruction::Operation::X86_INS_CALL:
			case edb::Instruction::Operation::X86_INS_JMP:
				if(op1.general_type() == edb::Operand::TYPE_EXPRESSION) {

					if(op1.expression().displacement_type == edb::Operand::DISP_NONE) {

						if(op1.expression().base == STACK_REG && op1.expression().index == edb::Operand::Register::X86_REG_INVALID) {
							add_result((QList<edb::Instruction>() << inst), start_address);
							return;
						}

						if(op1.expression().index == STACK_REG && op1.expression().base == edb::Operand::Register::X86_REG_INVALID) {
							add_result((QList<edb::Instruction>() << inst), start_address);
							return;
						}
					}
				}
				break;
			case edb::Instruction::Operation::X86_INS_POP:
				if(op1.general_type() == edb::Operand::TYPE_REGISTER) {

					p += inst.size();
					edb::Instruction inst2(p, last, 0);
					if(inst2) {
						const edb::Operand &op2 = inst2.operands()[0];
						switch(inst2.operation()) {
						case edb::Instruction::Operation::X86_INS_JMP:
						case edb::Instruction::Operation::X86_INS_CALL:

							if(op2.general_type() == edb::Operand::TYPE_REGISTER) {

								if(op1.reg() == op2.reg()) {
									add_result((QList<edb::Instruction>() << inst << inst2), start_address);
								}
							}
							break;
						default:
							break;
						}
					}
				}
				break;

			default:
				break;
			}		
		}
	}
}

//------------------------------------------------------------------------------
// Name: test_esp_add_regx1
// Desc:
//------------------------------------------------------------------------------
void DialogOpcodes::test_esp_add_regx1(const OpcodeData &data, edb::address_t start_address) {

	const quint8 *p = data.data;
	const quint8 *last = p + sizeof(data);

	edb::Instruction inst(p, last, 0);

	if(inst) {
		const edb::Operand &op1 = inst.operands()[0];
		switch(inst.operation()) {
		case edb::Instruction::Operation::X86_INS_POP:

			if(op1.general_type() != edb::Operand::TYPE_REGISTER || op1.reg() != STACK_REG) {
				p += inst.size();
				edb::Instruction inst2(p, last, 0);
				if(inst2) {
					if(is_ret(inst2)) {
						add_result((QList<edb::Instruction>() << inst << inst2), start_address);
					}
				}
			}
			break;
		case edb::Instruction::Operation::X86_INS_JMP:
		case edb::Instruction::Operation::X86_INS_CALL:

			if(op1.general_type() == edb::Operand::TYPE_EXPRESSION) {

				if(op1.displacement() == 4) {
					if(op1.expression().base == STACK_REG && op1.expression().index == edb::Operand::Register::X86_REG_INVALID) {
						add_result((QList<edb::Instruction>() << inst), start_address);
					} else if(op1.expression().base == edb::Operand::Register::X86_REG_INVALID && op1.expression().index == STACK_REG && op1.expression().scale == 1) {
						add_result((QList<edb::Instruction>() << inst), start_address);
					}

				}
			}
			break;
		case edb::Instruction::Operation::X86_INS_SUB:
			if(op1.general_type() == edb::Operand::TYPE_REGISTER && op1.reg() == STACK_REG) {

				const edb::Operand &op2 = inst.operands()[1];
				if(op2.general_type() == edb::Operand::TYPE_IMMEDIATE) {

					if(op2.immediate() == -static_cast<int>(sizeof(edb::reg_t))) {
						p += inst.size();
						edb::Instruction inst2(p, last, 0);
						if(inst2) {
							if(is_ret(inst2)) {
								add_result((QList<edb::Instruction>() << inst << inst2), start_address);
							}
						}
					}
				}
			}
			break;

		case edb::Instruction::Operation::X86_INS_ADD:
			if(op1.general_type() == edb::Operand::TYPE_REGISTER && op1.reg() == STACK_REG) {

				const edb::Operand &op2 = inst.operands()[1];
				if(op2.general_type() == edb::Operand::TYPE_IMMEDIATE) {

					if(op2.immediate() == sizeof(edb::reg_t)) {
						p += inst.size();
						edb::Instruction inst2(p, last, 0);
						if(inst2) {
							if(is_ret(inst2)) {
								add_result((QList<edb::Instruction>() << inst << inst2), start_address);
							}
						}
					}
				}
			}
			break;

		default:
			break;
		}
	}
}

//------------------------------------------------------------------------------
// Name: test_esp_add_regx2
// Desc:
//------------------------------------------------------------------------------
void DialogOpcodes::test_esp_add_regx2(const OpcodeData &data, edb::address_t start_address) {

	const quint8 *p = data.data;
	const quint8 *last = p + sizeof(data);

	edb::Instruction inst(p, last, 0);

	if(inst) {
		const edb::Operand &op1 = inst.operands()[0];
		switch(inst.operation()) {
		case edb::Instruction::Operation::X86_INS_POP:

			if(op1.general_type() != edb::Operand::TYPE_REGISTER || op1.reg() != STACK_REG) {
				p += inst.size();
				edb::Instruction inst2(p, last, 0);
				if(inst2) {
					const edb::Operand &op2 = inst2.operands()[0];
					switch(inst2.operation()) {
					case edb::Instruction::Operation::X86_INS_POP:

						if(op2.general_type() != edb::Operand::TYPE_REGISTER || op2.reg() != STACK_REG) {
							p += inst2.size();
							edb::Instruction inst3(p, last, 0);
							if(inst3) {
								if(is_ret(inst3)) {
									add_result((QList<edb::Instruction>() << inst << inst2 << inst3), start_address);
								}
							}
						}
						break;
					default:
						break;
					}
				}
			}
			break;

		case edb::Instruction::Operation::X86_INS_JMP:
		case edb::Instruction::Operation::X86_INS_CALL:
			if(op1.general_type() == edb::Operand::TYPE_EXPRESSION) {

				if(op1.displacement() == (sizeof(edb::reg_t) * 2)) {
					if(op1.expression().base == STACK_REG && op1.expression().index == edb::Operand::Register::X86_REG_INVALID) {
						add_result((QList<edb::Instruction>() << inst), start_address);
					} else if(op1.expression().base == edb::Operand::Register::X86_REG_INVALID && op1.expression().index == STACK_REG && op1.expression().scale == 1) {
						add_result((QList<edb::Instruction>() << inst), start_address);
					}

				}
			}
			break;

		case edb::Instruction::Operation::X86_INS_SUB:
			if(op1.general_type() == edb::Operand::TYPE_REGISTER && op1.reg() == STACK_REG) {

				const edb::Operand &op2 = inst.operands()[1];
				if(op2.general_type() == edb::Operand::TYPE_IMMEDIATE) {

					if(op2.immediate() == -static_cast<int>(sizeof(edb::reg_t) * 2)) {
						p += inst.size();
						edb::Instruction inst2(p, last, 0);
						if(inst2) {
							if(is_ret(inst2)) {
								add_result((QList<edb::Instruction>() << inst << inst2), start_address);
							}
						}
					}
				}
			}
			break;

		case edb::Instruction::Operation::X86_INS_ADD:
			if(op1.general_type() == edb::Operand::TYPE_REGISTER && op1.reg() == STACK_REG) {

				const edb::Operand &op2 = inst.operands()[1];
				if(op2.general_type() == edb::Operand::TYPE_IMMEDIATE) {

					if(op2.immediate() == (sizeof(edb::reg_t) * 2)) {
						p += inst.size();
						edb::Instruction inst2(p, last, 0);
						if(inst2) {
							if(is_ret(inst2)) {
								add_result((QList<edb::Instruction>() << inst << inst2), start_address);
							}
						}
					}
				}
			}
			break;

		default:
			break;
		}
	}
}

//------------------------------------------------------------------------------
// Name: test_esp_sub_regx1
// Desc:
//------------------------------------------------------------------------------
void DialogOpcodes::test_esp_sub_regx1(const OpcodeData &data, edb::address_t start_address) {

	const quint8 *p = data.data;
	const quint8 *last = p + sizeof(data);

	edb::Instruction inst(p, last, 0);

	if(inst) {
		const edb::Operand &op1 = inst.operands()[0];
		switch(inst.operation()) {
		case edb::Instruction::Operation::X86_INS_JMP:
		case edb::Instruction::Operation::X86_INS_CALL:
			if(op1.general_type() == edb::Operand::TYPE_EXPRESSION) {

				if(op1.displacement() == -static_cast<int>(sizeof(edb::reg_t))) {
					if(op1.expression().base == STACK_REG && op1.expression().index == edb::Operand::Register::X86_REG_INVALID) {
						add_result((QList<edb::Instruction>() << inst), start_address);
					} else if(op1.expression().base == edb::Operand::Register::X86_REG_INVALID && op1.expression().index == STACK_REG && op1.expression().scale == 1) {
						add_result((QList<edb::Instruction>() << inst), start_address);
					}

				}
			}
			break;

		case edb::Instruction::Operation::X86_INS_SUB:
			if(op1.general_type() == edb::Operand::TYPE_REGISTER && op1.reg() == STACK_REG) {

				const edb::Operand &op2 = inst.operands()[1];
				if(op2.general_type() == edb::Operand::TYPE_IMMEDIATE) {

					if(op2.immediate() == static_cast<int>(sizeof(edb::reg_t))) {
						p += inst.size();
						edb::Instruction inst2(p, last, 0);
						if(inst2) {
							if(is_ret(inst2)) {
								add_result((QList<edb::Instruction>() << inst << inst2), start_address);
							}
						}
					}
				}
			}
			break;

		case edb::Instruction::Operation::X86_INS_ADD:
			if(op1.general_type() == edb::Operand::TYPE_REGISTER && op1.reg() == STACK_REG) {

				const edb::Operand &op2 = inst.operands()[1];
				if(op2.general_type() == edb::Operand::TYPE_IMMEDIATE) {

					if(op2.immediate() == -static_cast<int>(sizeof(edb::reg_t))) {
						p += inst.size();
						edb::Instruction inst2(p, last, 0);
						if(inst2) {
							if(is_ret(inst2)) {
								add_result((QList<edb::Instruction>() << inst << inst2), start_address);
							}
						}
					}
				}
			}
			break;

		default:
			break;
		}
	}
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
void DialogOpcodes::run_tests(int classtype, const OpcodeData &opcode, edb::address_t address) {

	switch(classtype) {
#if defined(EDB_X86)
	case 1: test_reg_to_ip<edb::Operand::Register::X86_REG_EAX>(opcode, address); break;
	case 2: test_reg_to_ip<edb::Operand::Register::X86_REG_EBX>(opcode, address); break;
	case 3: test_reg_to_ip<edb::Operand::Register::X86_REG_ECX>(opcode, address); break;
	case 4: test_reg_to_ip<edb::Operand::Register::X86_REG_EDX>(opcode, address); break;
	case 5: test_reg_to_ip<edb::Operand::Register::X86_REG_EBP>(opcode, address); break;
	case 6: test_reg_to_ip<edb::Operand::Register::X86_REG_ESP>(opcode, address); break;
	case 7: test_reg_to_ip<edb::Operand::Register::X86_REG_ESI>(opcode, address); break;
	case 8: test_reg_to_ip<edb::Operand::Register::X86_REG_EDI>(opcode, address); break;
#elif defined(EDB_X86_64)
	case 1: test_reg_to_ip<edb::Operand::Register::X86_REG_RAX>(opcode, address); break;
	case 2: test_reg_to_ip<edb::Operand::Register::X86_REG_RBX>(opcode, address); break;
	case 3: test_reg_to_ip<edb::Operand::Register::X86_REG_RCX>(opcode, address); break;
	case 4: test_reg_to_ip<edb::Operand::Register::X86_REG_RDX>(opcode, address); break;
	case 5: test_reg_to_ip<edb::Operand::Register::X86_REG_RBP>(opcode, address); break;
	case 6: test_reg_to_ip<edb::Operand::Register::X86_REG_RSP>(opcode, address); break;
	case 7: test_reg_to_ip<edb::Operand::Register::X86_REG_RSI>(opcode, address); break;
	case 8: test_reg_to_ip<edb::Operand::Register::X86_REG_RDI>(opcode, address); break;
	case 9: test_reg_to_ip<edb::Operand::Register::X86_REG_R8>(opcode, address); break;
	case 10: test_reg_to_ip<edb::Operand::Register::X86_REG_R9>(opcode, address); break;
	case 11: test_reg_to_ip<edb::Operand::Register::X86_REG_R10>(opcode, address); break;
	case 12: test_reg_to_ip<edb::Operand::Register::X86_REG_R11>(opcode, address); break;
	case 13: test_reg_to_ip<edb::Operand::Register::X86_REG_R12>(opcode, address); break;
	case 14: test_reg_to_ip<edb::Operand::Register::X86_REG_R13>(opcode, address); break;
	case 15: test_reg_to_ip<edb::Operand::Register::X86_REG_R14>(opcode, address); break;
	case 16: test_reg_to_ip<edb::Operand::Register::X86_REG_R15>(opcode, address); break;
#endif

	case 17:
	#if defined(EDB_X86)
		test_reg_to_ip<edb::Operand::Register::X86_REG_EAX>(opcode, address);
		test_reg_to_ip<edb::Operand::Register::X86_REG_EBX>(opcode, address);
		test_reg_to_ip<edb::Operand::Register::X86_REG_ECX>(opcode, address);
		test_reg_to_ip<edb::Operand::Register::X86_REG_EDX>(opcode, address);
		test_reg_to_ip<edb::Operand::Register::X86_REG_EBP>(opcode, address);
		test_reg_to_ip<edb::Operand::Register::X86_REG_ESP>(opcode, address);
		test_reg_to_ip<edb::Operand::Register::X86_REG_ESI>(opcode, address);
		test_reg_to_ip<edb::Operand::Register::X86_REG_EDI>(opcode, address);
	#elif defined(EDB_X86_64)
		test_reg_to_ip<edb::Operand::Register::X86_REG_RAX>(opcode, address);
		test_reg_to_ip<edb::Operand::Register::X86_REG_RBX>(opcode, address);
		test_reg_to_ip<edb::Operand::Register::X86_REG_RCX>(opcode, address);
		test_reg_to_ip<edb::Operand::Register::X86_REG_RDX>(opcode, address);
		test_reg_to_ip<edb::Operand::Register::X86_REG_RBP>(opcode, address);
		test_reg_to_ip<edb::Operand::Register::X86_REG_RSP>(opcode, address);
		test_reg_to_ip<edb::Operand::Register::X86_REG_RSI>(opcode, address);
		test_reg_to_ip<edb::Operand::Register::X86_REG_RDI>(opcode, address);
		test_reg_to_ip<edb::Operand::Register::X86_REG_R8>(opcode, address);
		test_reg_to_ip<edb::Operand::Register::X86_REG_R9>(opcode, address);
		test_reg_to_ip<edb::Operand::Register::X86_REG_R10>(opcode, address);
		test_reg_to_ip<edb::Operand::Register::X86_REG_R11>(opcode, address);
		test_reg_to_ip<edb::Operand::Register::X86_REG_R12>(opcode, address);
		test_reg_to_ip<edb::Operand::Register::X86_REG_R13>(opcode, address);
		test_reg_to_ip<edb::Operand::Register::X86_REG_R14>(opcode, address);
		test_reg_to_ip<edb::Operand::Register::X86_REG_R15>(opcode, address);
	#endif
		break;
	case 18:
		// [ESP] -> EIP
		test_esp_add_0(opcode, address);
		break;
	case 19:
		// [ESP + 4] -> EIP
		test_esp_add_regx1(opcode, address);
		break;
	case 20:
		// [ESP + 8] -> EIP
		test_esp_add_regx2(opcode, address);
		break;
	case 21:
		// [ESP - 4] -> EIP
		test_esp_sub_regx1(opcode, address);
		break;


	case 22: test_deref_reg_to_ip<edb::Operand::Register::X86_REG_RAX>(opcode, address); break;
	case 23: test_deref_reg_to_ip<edb::Operand::Register::X86_REG_RBX>(opcode, address); break;
	case 24: test_deref_reg_to_ip<edb::Operand::Register::X86_REG_RCX>(opcode, address); break;
	case 25: test_deref_reg_to_ip<edb::Operand::Register::X86_REG_RDX>(opcode, address); break;
	case 26: test_deref_reg_to_ip<edb::Operand::Register::X86_REG_RBP>(opcode, address); break;
	case 28: test_deref_reg_to_ip<edb::Operand::Register::X86_REG_RSI>(opcode, address); break;
	case 29: test_deref_reg_to_ip<edb::Operand::Register::X86_REG_RDI>(opcode, address); break;
	case 30: test_deref_reg_to_ip<edb::Operand::Register::X86_REG_R8>(opcode, address); break;
	case 31: test_deref_reg_to_ip<edb::Operand::Register::X86_REG_R9>(opcode, address); break;
	case 32: test_deref_reg_to_ip<edb::Operand::Register::X86_REG_R10>(opcode, address); break;
	case 33: test_deref_reg_to_ip<edb::Operand::Register::X86_REG_R11>(opcode, address); break;
	case 34: test_deref_reg_to_ip<edb::Operand::Register::X86_REG_R12>(opcode, address); break;
	case 35: test_deref_reg_to_ip<edb::Operand::Register::X86_REG_R13>(opcode, address); break;
	case 36: test_deref_reg_to_ip<edb::Operand::Register::X86_REG_R14>(opcode, address); break;
	case 37: test_deref_reg_to_ip<edb::Operand::Register::X86_REG_R15>(opcode, address); break;
	}
}

//------------------------------------------------------------------------------
// Name: do_find
// Desc:
//------------------------------------------------------------------------------
void DialogOpcodes::do_find() {

	const int classtype = ui->comboBox->itemData(ui->comboBox->currentIndex()).toInt();

	const QItemSelectionModel *const selModel = ui->tableView->selectionModel();
	const QModelIndexList sel = selModel->selectedRows();

	if(sel.size() == 0) {
		QMessageBox::information(
			this,
			tr("No Region Selected"),
			tr("You must select a region which is to be scanned for the desired opcode."));
	} else {

		if(IProcess *process = edb::v1::debugger_core->process()) {
			for(const QModelIndex &selected_item: sel) {

				const QModelIndex index = filter_model_->mapToSource(selected_item);

				if(auto region = *reinterpret_cast<const IRegion::pointer *>(index.internalPointer())) {

					edb::address_t start_address     = region->start();
					edb::address_t address           = region->start();
					const edb::address_t end_address = region->end();
					const edb::address_t orig_start  = region->start();

					ShiftBuffer<sizeof(OpcodeData)> shift_buffer;

					// this will read the rest of the region
					size_t i = 0;
					while(start_address < end_address) {

						// create a reference to the bsa's data so we can pass it to the testXXXX functions
						// but only do so if we have read enough bytes to fill our shift buffer
						if(i >= shift_buffer.size()) {
							auto opcode = *reinterpret_cast<const OpcodeData *>(&shift_buffer[0]);
							run_tests(classtype, opcode, address - shift_buffer.size());
						}

						quint8 byte;
						process->read_bytes(start_address, &byte, 1);
						shift_buffer.shl();
						shift_buffer[shift_buffer.size() - 1] = byte;

						++start_address;

						ui->progressBar->setValue(util::percentage(address - orig_start, region->size()));
						++address;
						++i;
					}

					// test the stuff at the regions edge
					for(size_t i = 0; i < shift_buffer.size(); ++i) {

						// create a reference to the bsa's data so we can pass it to the testXXXX functions
						auto opcode = *reinterpret_cast<const OpcodeData *>(&shift_buffer[0]);
						run_tests(classtype, opcode, address - shift_buffer.size());

						// we just shift in 0's and hope it doesn't give false positives
						shift_buffer.shl();
						shift_buffer[shift_buffer.size() - 1] = 0x00;

						ui->progressBar->setValue(util::percentage(address - orig_start, region->size()));
						++address;
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
void DialogOpcodes::on_btnFind_clicked() {
	ui->btnFind->setEnabled(false);
	ui->listWidget->clear();
	ui->progressBar->setValue(0);
	do_find();
	ui->progressBar->setValue(100);
	ui->btnFind->setEnabled(true);
}

}

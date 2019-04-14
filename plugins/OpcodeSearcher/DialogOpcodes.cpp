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
#include "IProcess.h"
#include "IRegion.h"
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

namespace OpcodeSearcherPlugin {

namespace {
#if defined(EDB_X86)
constexpr int STACK_REG = X86_REG_ESP;
#elif defined(EDB_X86_64)
constexpr int STACK_REG = X86_REG_RSP;
#elif defined EDB_ARM32
constexpr int STACK_REG = ARM_REG_SP;
#elif defined EDB_ARM64
constexpr int STACK_REG = ARM64_REG_SP;
#endif
}

//------------------------------------------------------------------------------
// Name: DialogOpcodes
// Desc:
//------------------------------------------------------------------------------
DialogOpcodes::DialogOpcodes(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f), ui(new Ui::DialogOpcodes) {
	ui->setupUi(this);
	ui->tableView->verticalHeader()->hide();
	ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	filter_model_ = new QSortFilterProxyModel(this);
	connect(ui->txtSearch, &QLineEdit::textChanged, filter_model_, &QSortFilterProxyModel::setFilterFixedString);
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
	
#if defined(EDB_X86) || defined(EDB_X86_64)
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
#elif defined(EDB_ARM32)
	// TODO(eteran): implement
#elif defined(EDB_ARM64)
	// TODO(eteran): implement
#endif
}

//------------------------------------------------------------------------------
// Name: add_result
// Desc:
//------------------------------------------------------------------------------
void DialogOpcodes::add_result(const InstructionList &instructions, edb::address_t rva) {
	if(!instructions.empty()) {
	
		auto it = instructions.begin();
		const edb::Instruction *inst1 = *it++;

		QString instruction_string = QString("%1: %2").arg(
			edb::v1::format_pointer(rva),
			QString::fromStdString(edb::v1::formatter().to_string(*inst1)));


		for(; it != instructions.end(); ++it) {
			const edb::Instruction *inst = *it;
			instruction_string.append(QString("; %1").arg(QString::fromStdString(edb::v1::formatter().to_string(*inst))));
		}

		auto item = new QListWidgetItem(instruction_string);

		item->setData(Qt::UserRole, rva.toQVariant());
		ui->listWidget->addItem(item);
	}
}

//------------------------------------------------------------------------------
// Name: test_deref_reg_to_ip
// Desc:
//------------------------------------------------------------------------------
template <int REG>
void DialogOpcodes::test_deref_reg_to_ip(const OpcodeData &data, edb::address_t start_address) {
	const quint8 *p = data.data;
	const quint8 *last = p + sizeof(data);

	edb::Instruction inst(p, last, 0);

	if(inst) {
		
		if(is_call(inst) || is_jump(inst)) {
			const auto op1 = inst[0];
			if(is_expression(op1)) {

				if(op1->mem.disp == 0) {

					if(op1->mem.base == REG && op1->mem.index == X86_REG_INVALID && op1->mem.scale == 1) {
						add_result({ &inst }, start_address);
						return;
					}

					if(op1->mem.index == REG && op1->mem.base == X86_REG_INVALID && op1->mem.scale == 1) {
						add_result({ &inst }, start_address);
						return;
					}
				}
			}
		
		}
	}
}

//------------------------------------------------------------------------------
// Name: test_reg_to_ip
// Desc:
//------------------------------------------------------------------------------
template <int REG>
void DialogOpcodes::test_reg_to_ip(const DialogOpcodes::OpcodeData &data, edb::address_t start_address) {

	const quint8 *p = data.data;
	const quint8 *last = p + sizeof(data);

	edb::Instruction inst(p, last, 0);

	if(inst) {
		if(is_call(inst) || is_jump(inst)) {
			const auto op1 = inst[0];
			if(is_register(op1)) {
				if(op1->reg == REG) {
					add_result({ &inst }, start_address);
					return;
				}
			}
		} else {
			const auto op1 = inst[0];
			switch(inst.operation()) {
			case X86_INS_PUSH:
				if(is_register(op1)) {
					if(op1->reg == REG) {

						p += inst.byte_size();
						edb::Instruction inst2(p, last, 0);
						if(inst2) {
							const auto op2 = inst2[0];

							if(is_ret(inst2)) {
								add_result({ &inst, &inst2 }, start_address);
							} else {
								switch(inst2.operation()) {
								case X86_INS_JMP:
								case X86_INS_CALL:

									if(is_expression(op2)) {

										if(op2->mem.disp == 0) {

											if(op2->mem.base == STACK_REG && op2->mem.index == X86_REG_INVALID) {
												add_result({ &inst, &inst2 }, start_address);
												return;
											}

											if(op2->mem.index == STACK_REG && op2->mem.base == X86_REG_INVALID) {
												add_result({ &inst, &inst2 }, start_address);
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
		const auto op1 = inst[0];
		if(is_ret(inst)) {
			add_result({ &inst }, start_address);
		} else if(is_call(inst) || is_jump(inst)) {
				if(is_expression(op1)) {

					if(op1->mem.disp == 0) {

						if(op1->mem.base == STACK_REG && op1->mem.index == X86_REG_INVALID) {
							add_result({ &inst }, start_address);
							return;
						}

						if(op1->mem.index == STACK_REG && op1->mem.base == X86_REG_INVALID) {
							add_result({ &inst }, start_address);
							return;
						}
					}
				}
		} else {
			switch(inst.operation()) {
			case X86_INS_POP:
				if(is_register(op1)) {

					p += inst.byte_size();
					edb::Instruction inst2(p, last, 0);
					if(inst2) {
						const auto op2 = inst2[0];
						switch(inst2.operation()) {
						case X86_INS_JMP:
						case X86_INS_CALL:

							if(is_register(op2)) {

								if(op1->reg == op2->reg) {
									add_result({ &inst, &inst2 }, start_address);
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
		const auto op1 = inst[0];
		if(is_call(inst) || is_jump(inst)) {
				if(is_expression(op1)) {

					if(op1->mem.disp == 4) {
						if(op1->mem.base == STACK_REG && op1->mem.index == X86_REG_INVALID) {
							add_result({ &inst }, start_address);
						} else if(op1->mem.base == X86_REG_INVALID && op1->mem.index == STACK_REG && op1->mem.scale == 1) {
							add_result({ &inst }, start_address);
						}

					}
				}
		} else { 		
			switch(inst.operation()) {
			case X86_INS_POP:

				if(!is_register(op1) || op1->reg != STACK_REG) {
					p += inst.byte_size();
					edb::Instruction inst2(p, last, 0);
					if(inst2) {
						if(is_ret(inst2)) {
							add_result({ &inst, &inst2 }, start_address);
						}
					}
				}
				break;
			case X86_INS_SUB:
				if(is_register(op1) && op1->reg == STACK_REG) {

					const auto op2 = inst[1];
					if(is_expression(op2)) {

						if(op2->imm == -static_cast<int>(sizeof(edb::reg_t))) {
							p += inst.byte_size();
							edb::Instruction inst2(p, last, 0);
							if(inst2) {
								if(is_ret(inst2)) {
									add_result({ &inst, &inst2 }, start_address);
								}
							}
						}
					}
				}
				break;
			case X86_INS_ADD:
				if(is_register(op1) && op1->reg == STACK_REG) {

					const auto op2 = inst[1];
					if(is_expression(op2)) {

						if(op2->imm == sizeof(edb::reg_t)) {
							p += inst.byte_size();
							edb::Instruction inst2(p, last, 0);
							if(inst2) {
								if(is_ret(inst2)) {
									add_result({ &inst, &inst2 }, start_address);
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
		const auto op1 = inst[0];
		if(is_call(inst) || is_jump(inst)) {
			if(is_expression(op1)) {

				if(op1->mem.disp == (sizeof(edb::reg_t) * 2)) {
					if(op1->mem.base == STACK_REG && op1->mem.index == X86_REG_INVALID) {
						add_result({ &inst }, start_address);
					} else if(op1->mem.base == X86_REG_INVALID && op1->mem.index == STACK_REG && op1->mem.scale == 1) {
						add_result({ &inst }, start_address);
					}

				}
			}
		} else {		
			switch(inst.operation()) {
			case X86_INS_POP:

				if(!is_register(op1) || op1->reg != STACK_REG) {
					p += inst.byte_size();
					edb::Instruction inst2(p, last, 0);
					if(inst2) {
						const auto op2 = inst2[0];
						switch(inst2.operation()) {
						case X86_INS_POP:

							if(!is_register(op2) || op2->reg != STACK_REG) {
								p += inst2.byte_size();
								edb::Instruction inst3(p, last, 0);
								if(inst3) {
									if(is_ret(inst3)) {
										add_result({ &inst, &inst2, &inst3 }, start_address);
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
			case X86_INS_SUB:
				if(is_register(op1) && op1->reg == STACK_REG) {

					const auto op2 = inst[1];
					if(is_expression(op2)) {

						if(op2->imm == -static_cast<int>(sizeof(edb::reg_t) * 2)) {
							p += inst.byte_size();
							edb::Instruction inst2(p, last, 0);
							if(inst2) {
								if(is_ret(inst2)) {
									add_result({ &inst, &inst2 }, start_address);
								}
							}
						}
					}
				}
				break;

			case X86_INS_ADD:
				if(is_register(op1) && op1->reg == STACK_REG) {

					const auto op2 = inst[1];
					if(is_expression(op2)) {

						if(op2->imm == (sizeof(edb::reg_t) * 2)) {
							p += inst.byte_size();
							edb::Instruction inst2(p, last, 0);
							if(inst2) {
								if(is_ret(inst2)) {
									add_result({ &inst, &inst2 }, start_address);
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
		const auto op1 = inst[0];
		if(is_call(inst) || is_jump(inst)) {
			if(is_expression(op1)) {

				if(op1->mem.disp == -static_cast<int>(sizeof(edb::reg_t))) {
					if(op1->mem.base == STACK_REG && op1->mem.index == X86_REG_INVALID) {
						add_result({ &inst }, start_address);
					} else if(op1->mem.base == X86_REG_INVALID && op1->mem.index == STACK_REG && op1->mem.scale == 1) {
						add_result({ &inst }, start_address);
					}

				}
			}		
		} else {
			switch(inst.operation()) {
			case X86_INS_SUB:
				if(is_register(op1) && op1->reg == STACK_REG) {

					const auto op2 = inst[1];
					if(is_expression(op2)) {

						if(op2->imm == static_cast<int>(sizeof(edb::reg_t))) {
							p += inst.byte_size();
							edb::Instruction inst2(p, last, 0);
							if(inst2) {
								if(is_ret(inst2)) {
									add_result({ &inst, &inst2 }, start_address);
								}
							}
						}
					}
				}
				break;

			case X86_INS_ADD:
				if(is_register(op1) && op1->reg == STACK_REG) {

					const auto op2 = inst[1];
					if(is_expression(op2)) {

						if(op2->imm == -static_cast<int>(sizeof(edb::reg_t))) {
							p += inst.byte_size();
							edb::Instruction inst2(p, last, 0);
							if(inst2) {
								if(is_ret(inst2)) {
									add_result({ &inst, &inst2 }, start_address);
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
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
void DialogOpcodes::run_tests(int classtype, const OpcodeData &opcode, edb::address_t address) {

	switch(classtype) {
#if defined(EDB_X86)
	case 1: test_reg_to_ip<X86_REG_EAX>(opcode, address); break;
	case 2: test_reg_to_ip<X86_REG_EBX>(opcode, address); break;
	case 3: test_reg_to_ip<X86_REG_ECX>(opcode, address); break;
	case 4: test_reg_to_ip<X86_REG_EDX>(opcode, address); break;
	case 5: test_reg_to_ip<X86_REG_EBP>(opcode, address); break;
	case 6: test_reg_to_ip<X86_REG_ESP>(opcode, address); break;
	case 7: test_reg_to_ip<X86_REG_ESI>(opcode, address); break;
	case 8: test_reg_to_ip<X86_REG_EDI>(opcode, address); break;
#elif defined(EDB_X86_64)
	case 1: test_reg_to_ip<X86_REG_RAX>(opcode, address); break;
	case 2: test_reg_to_ip<X86_REG_RBX>(opcode, address); break;
	case 3: test_reg_to_ip<X86_REG_RCX>(opcode, address); break;
	case 4: test_reg_to_ip<X86_REG_RDX>(opcode, address); break;
	case 5: test_reg_to_ip<X86_REG_RBP>(opcode, address); break;
	case 6: test_reg_to_ip<X86_REG_RSP>(opcode, address); break;
	case 7: test_reg_to_ip<X86_REG_RSI>(opcode, address); break;
	case 8: test_reg_to_ip<X86_REG_RDI>(opcode, address); break;
	case 9: test_reg_to_ip<X86_REG_R8>(opcode, address); break;
	case 10: test_reg_to_ip<X86_REG_R9>(opcode, address); break;
	case 11: test_reg_to_ip<X86_REG_R10>(opcode, address); break;
	case 12: test_reg_to_ip<X86_REG_R11>(opcode, address); break;
	case 13: test_reg_to_ip<X86_REG_R12>(opcode, address); break;
	case 14: test_reg_to_ip<X86_REG_R13>(opcode, address); break;
	case 15: test_reg_to_ip<X86_REG_R14>(opcode, address); break;
	case 16: test_reg_to_ip<X86_REG_R15>(opcode, address); break;
#endif

	case 17:
	#if defined(EDB_X86)
		test_reg_to_ip<X86_REG_EAX>(opcode, address);
		test_reg_to_ip<X86_REG_EBX>(opcode, address);
		test_reg_to_ip<X86_REG_ECX>(opcode, address);
		test_reg_to_ip<X86_REG_EDX>(opcode, address);
		test_reg_to_ip<X86_REG_EBP>(opcode, address);
		test_reg_to_ip<X86_REG_ESP>(opcode, address);
		test_reg_to_ip<X86_REG_ESI>(opcode, address);
		test_reg_to_ip<X86_REG_EDI>(opcode, address);
	#elif defined(EDB_X86_64)
		test_reg_to_ip<X86_REG_RAX>(opcode, address);
		test_reg_to_ip<X86_REG_RBX>(opcode, address);
		test_reg_to_ip<X86_REG_RCX>(opcode, address);
		test_reg_to_ip<X86_REG_RDX>(opcode, address);
		test_reg_to_ip<X86_REG_RBP>(opcode, address);
		test_reg_to_ip<X86_REG_RSP>(opcode, address);
		test_reg_to_ip<X86_REG_RSI>(opcode, address);
		test_reg_to_ip<X86_REG_RDI>(opcode, address);
		test_reg_to_ip<X86_REG_R8>(opcode, address);
		test_reg_to_ip<X86_REG_R9>(opcode, address);
		test_reg_to_ip<X86_REG_R10>(opcode, address);
		test_reg_to_ip<X86_REG_R11>(opcode, address);
		test_reg_to_ip<X86_REG_R12>(opcode, address);
		test_reg_to_ip<X86_REG_R13>(opcode, address);
		test_reg_to_ip<X86_REG_R14>(opcode, address);
		test_reg_to_ip<X86_REG_R15>(opcode, address);
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


	case 22: test_deref_reg_to_ip<X86_REG_RAX>(opcode, address); break;
	case 23: test_deref_reg_to_ip<X86_REG_RBX>(opcode, address); break;
	case 24: test_deref_reg_to_ip<X86_REG_RCX>(opcode, address); break;
	case 25: test_deref_reg_to_ip<X86_REG_RDX>(opcode, address); break;
	case 26: test_deref_reg_to_ip<X86_REG_RBP>(opcode, address); break;
	case 28: test_deref_reg_to_ip<X86_REG_RSI>(opcode, address); break;
	case 29: test_deref_reg_to_ip<X86_REG_RDI>(opcode, address); break;
	case 30: test_deref_reg_to_ip<X86_REG_R8>(opcode, address); break;
	case 31: test_deref_reg_to_ip<X86_REG_R9>(opcode, address); break;
	case 32: test_deref_reg_to_ip<X86_REG_R10>(opcode, address); break;
	case 33: test_deref_reg_to_ip<X86_REG_R11>(opcode, address); break;
	case 34: test_deref_reg_to_ip<X86_REG_R12>(opcode, address); break;
	case 35: test_deref_reg_to_ip<X86_REG_R13>(opcode, address); break;
	case 36: test_deref_reg_to_ip<X86_REG_R14>(opcode, address); break;
	case 37: test_deref_reg_to_ip<X86_REG_R15>(opcode, address); break;
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
		QMessageBox::critical(
			this,
			tr("No Region Selected"),
			tr("You must select a region which is to be scanned for the desired opcode."));
	} else {

		if(IProcess *process = edb::v1::debugger_core->process()) {
			for(const QModelIndex &selected_item: sel) {

				const QModelIndex index = filter_model_->mapToSource(selected_item);

				if(auto region = *reinterpret_cast<const std::shared_ptr<IRegion> *>(index.internalPointer())) {

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

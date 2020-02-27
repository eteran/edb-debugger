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
#include "DialogResults.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "IRegion.h"
#include "Instruction.h"
#include "MemoryRegions.h"
#include "ResultsModel.h"
#include "edb.h"
#include "util/Container.h"
#include "util/Math.h"

#include <QDebug>
#include <QHeaderView>
#include <QList>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>

#include <vector>

namespace OpcodeSearcherPlugin {
namespace {

using InstructionList = std::vector<edb::Instruction *>;

// we currently only support opcodes sequences up to 8 bytes big
using OpcodeData = std::array<uint8_t, sizeof(uint64_t)>;

/**
 * @brief add_result
 * @param resultsDialog
 * @param instructions
 * @param rva
 */
void add_result(DialogResults *resultsDialog, const InstructionList &instructions, edb::address_t rva) {
	if (!instructions.empty()) {

		auto it                       = instructions.begin();
		const edb::Instruction *inst1 = *it++;

		auto instruction_string = QString::fromStdString(edb::v1::formatter().toString(*inst1));

		for (; it != instructions.end(); ++it) {
			const edb::Instruction *inst = *it;
			instruction_string.append(QString("; %1").arg(QString::fromStdString(edb::v1::formatter().toString(*inst))));
		}

		resultsDialog->addResult({rva, instruction_string});
	}
}

/**
 * @brief test_deref_reg_to_ip
 * @param resultsDialog
 * @param data
 * @param start_address
 */
template <int Register>
void test_deref_reg_to_ip(DialogResults *resultsDialog, const OpcodeData &data, edb::address_t start_address) {
	const uint8_t *p    = data.data();
	const uint8_t *last = p + sizeof(data);

	edb::Instruction inst(p, last, 0);

	if (inst) {

		if (is_call(inst) || is_jump(inst)) {
			const auto op1 = inst[0];
			if (is_expression(op1)) {

				if (op1->mem.disp == 0) {

					if (op1->mem.base == Register && op1->mem.index == X86_REG_INVALID && op1->mem.scale == 1) {
						add_result(resultsDialog, {&inst}, start_address);
						return;
					}

					if (op1->mem.index == Register && op1->mem.base == X86_REG_INVALID && op1->mem.scale == 1) {
						add_result(resultsDialog, {&inst}, start_address);
						return;
					}
				}
			}
		}
	}
}

/**
 * @brief test_reg_to_ip
 * @param resultsDialog
 * @param data
 * @param start_address
 */
template <int Register, int StackRegister>
void test_reg_to_ip(DialogResults *resultsDialog, const OpcodeData &data, edb::address_t start_address) {

	const uint8_t *p    = data.data();
	const uint8_t *last = p + sizeof(data);

	edb::Instruction inst(p, last, 0);

	if (inst) {
		if (is_call(inst) || is_jump(inst)) {
			const auto op1 = inst[0];
			if (is_register(op1)) {
				if (op1->reg == Register) {
					add_result(resultsDialog, {&inst}, start_address);
					return;
				}
			}
		} else {
			const auto op1 = inst[0];
			switch (inst.operation()) {
			case X86_INS_PUSH:
				if (is_register(op1)) {
					if (op1->reg == Register) {

						p += inst.byteSize();
						edb::Instruction inst2(p, last, 0);
						if (inst2) {
							const auto op2 = inst2[0];

							if (is_ret(inst2)) {
								add_result(resultsDialog, {&inst, &inst2}, start_address);
							} else {
								switch (inst2.operation()) {
								case X86_INS_JMP:
								case X86_INS_CALL:

									if (is_expression(op2)) {

										if (op2->mem.disp == 0) {

											if (op2->mem.base == StackRegister && op2->mem.index == X86_REG_INVALID) {
												add_result(resultsDialog, {&inst, &inst2}, start_address);
												return;
											}

											if (op2->mem.index == StackRegister && op2->mem.base == X86_REG_INVALID) {
												add_result(resultsDialog, {&inst, &inst2}, start_address);
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

/**
 * @brief test_esp_add_0
 * @param resultsDialog
 * @param data
 * @param start_address
 */
template <int StackRegister>
void test_esp_add_0(DialogResults *resultsDialog, const OpcodeData &data, edb::address_t start_address) {

	const uint8_t *p    = data.data();
	const uint8_t *last = p + sizeof(data);

	edb::Instruction inst(p, last, 0);

	if (inst) {
		const auto op1 = inst[0];
		if (is_ret(inst)) {
			add_result(resultsDialog, {&inst}, start_address);
		} else if (is_call(inst) || is_jump(inst)) {
			if (is_expression(op1)) {

				if (op1->mem.disp == 0) {

					if (op1->mem.base == StackRegister && op1->mem.index == X86_REG_INVALID) {
						add_result(resultsDialog, {&inst}, start_address);
						return;
					}

					if (op1->mem.index == StackRegister && op1->mem.base == X86_REG_INVALID) {
						add_result(resultsDialog, {&inst}, start_address);
						return;
					}
				}
			}
		} else {
			switch (inst.operation()) {
			case X86_INS_POP:
				if (is_register(op1)) {

					p += inst.byteSize();
					edb::Instruction inst2(p, last, 0);
					if (inst2) {
						const auto op2 = inst2[0];
						switch (inst2.operation()) {
						case X86_INS_JMP:
						case X86_INS_CALL:

							if (is_register(op2)) {

								if (op1->reg == op2->reg) {
									add_result(resultsDialog, {&inst, &inst2}, start_address);
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

/**
 * @brief test_esp_add_regx1
 * @param resultsDialog
 * @param data
 * @param start_address
 */
template <int StackRegister>
void test_esp_add_regx1(DialogResults *resultsDialog, const OpcodeData &data, edb::address_t start_address) {

	const uint8_t *p    = data.data();
	const uint8_t *last = p + sizeof(data);

	edb::Instruction inst(p, last, 0);

	if (inst) {
		const auto op1 = inst[0];
		if (is_call(inst) || is_jump(inst)) {
			if (is_expression(op1)) {

				if (op1->mem.disp == 4) {
					if (op1->mem.base == StackRegister && op1->mem.index == X86_REG_INVALID) {
						add_result(resultsDialog, {&inst}, start_address);
					} else if (op1->mem.base == X86_REG_INVALID && op1->mem.index == StackRegister && op1->mem.scale == 1) {
						add_result(resultsDialog, {&inst}, start_address);
					}
				}
			}
		} else {
			switch (inst.operation()) {
			case X86_INS_POP:

				if (!is_register(op1) || op1->reg != StackRegister) {
					p += inst.byteSize();
					edb::Instruction inst2(p, last, 0);
					if (inst2) {
						if (is_ret(inst2)) {
							add_result(resultsDialog, {&inst, &inst2}, start_address);
						}
					}
				}
				break;
			case X86_INS_SUB:
				if (is_register(op1) && op1->reg == StackRegister) {

					const auto op2 = inst[1];
					if (is_expression(op2)) {

						if (op2->imm == -static_cast<int>(sizeof(edb::reg_t))) {
							p += inst.byteSize();
							edb::Instruction inst2(p, last, 0);
							if (inst2) {
								if (is_ret(inst2)) {
									add_result(resultsDialog, {&inst, &inst2}, start_address);
								}
							}
						}
					}
				}
				break;
			case X86_INS_ADD:
				if (is_register(op1) && op1->reg == StackRegister) {

					const auto op2 = inst[1];
					if (is_expression(op2)) {

						if (op2->imm == sizeof(edb::reg_t)) {
							p += inst.byteSize();
							edb::Instruction inst2(p, last, 0);
							if (inst2) {
								if (is_ret(inst2)) {
									add_result(resultsDialog, {&inst, &inst2}, start_address);
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

/**
 * @brief test_esp_add_regx2
 * @param resultsDialog
 * @param data
 * @param start_address
 */
template <int StackRegister>
void test_esp_add_regx2(DialogResults *resultsDialog, const OpcodeData &data, edb::address_t start_address) {

	const uint8_t *p    = data.data();
	const uint8_t *last = p + sizeof(data);

	edb::Instruction inst(p, last, 0);

	if (inst) {
		const auto op1 = inst[0];
		if (is_call(inst) || is_jump(inst)) {
			if (is_expression(op1)) {

				if (op1->mem.disp == (sizeof(edb::reg_t) * 2)) {
					if (op1->mem.base == StackRegister && op1->mem.index == X86_REG_INVALID) {
						add_result(resultsDialog, {&inst}, start_address);
					} else if (op1->mem.base == X86_REG_INVALID && op1->mem.index == StackRegister && op1->mem.scale == 1) {
						add_result(resultsDialog, {&inst}, start_address);
					}
				}
			}
		} else {
			switch (inst.operation()) {
			case X86_INS_POP:

				if (!is_register(op1) || op1->reg != StackRegister) {
					p += inst.byteSize();
					edb::Instruction inst2(p, last, 0);
					if (inst2) {
						const auto op2 = inst2[0];
						switch (inst2.operation()) {
						case X86_INS_POP:

							if (!is_register(op2) || op2->reg != StackRegister) {
								p += inst2.byteSize();
								edb::Instruction inst3(p, last, 0);
								if (inst3) {
									if (is_ret(inst3)) {
										add_result(resultsDialog, {&inst, &inst2, &inst3}, start_address);
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
				if (is_register(op1) && op1->reg == StackRegister) {

					const auto op2 = inst[1];
					if (is_expression(op2)) {

						if (op2->imm == -static_cast<int>(sizeof(edb::reg_t) * 2)) {
							p += inst.byteSize();
							edb::Instruction inst2(p, last, 0);
							if (inst2) {
								if (is_ret(inst2)) {
									add_result(resultsDialog, {&inst, &inst2}, start_address);
								}
							}
						}
					}
				}
				break;

			case X86_INS_ADD:
				if (is_register(op1) && op1->reg == StackRegister) {

					const auto op2 = inst[1];
					if (is_expression(op2)) {

						if (op2->imm == (sizeof(edb::reg_t) * 2)) {
							p += inst.byteSize();
							edb::Instruction inst2(p, last, 0);
							if (inst2) {
								if (is_ret(inst2)) {
									add_result(resultsDialog, {&inst, &inst2}, start_address);
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

/**
 * @brief test_esp_sub_regx1
 * @param resultsDialog
 * @param data
 * @param start_address
 */
template <int StackRegister>
void test_esp_sub_regx1(DialogResults *resultsDialog, const OpcodeData &data, edb::address_t start_address) {

	const uint8_t *p    = data.data();
	const uint8_t *last = p + sizeof(data);

	edb::Instruction inst(p, last, 0);

	if (inst) {
		const auto op1 = inst[0];
		if (is_call(inst) || is_jump(inst)) {
			if (is_expression(op1)) {

				if (op1->mem.disp == -static_cast<int>(sizeof(edb::reg_t))) {
					if (op1->mem.base == StackRegister && op1->mem.index == X86_REG_INVALID) {
						add_result(resultsDialog, {&inst}, start_address);
					} else if (op1->mem.base == X86_REG_INVALID && op1->mem.index == StackRegister && op1->mem.scale == 1) {
						add_result(resultsDialog, {&inst}, start_address);
					}
				}
			}
		} else {
			switch (inst.operation()) {
			case X86_INS_SUB:
				if (is_register(op1) && op1->reg == StackRegister) {

					const auto op2 = inst[1];
					if (is_expression(op2)) {

						if (op2->imm == static_cast<int>(sizeof(edb::reg_t))) {
							p += inst.byteSize();
							edb::Instruction inst2(p, last, 0);
							if (inst2) {
								if (is_ret(inst2)) {
									add_result(resultsDialog, {&inst, &inst2}, start_address);
								}
							}
						}
					}
				}
				break;

			case X86_INS_ADD:
				if (is_register(op1) && op1->reg == StackRegister) {

					const auto op2 = inst[1];
					if (is_expression(op2)) {

						if (op2->imm == -static_cast<int>(sizeof(edb::reg_t))) {
							p += inst.byteSize();
							edb::Instruction inst2(p, last, 0);
							if (inst2) {
								if (is_ret(inst2)) {
									add_result(resultsDialog, {&inst, &inst2}, start_address);
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

/**
 * @brief run_tests
 * @param resultsDialog
 * @param classtype
 * @param opcode
 * @param address
 */
void run_tests(DialogResults *resultsDialog, int classtype, const OpcodeData &opcode, edb::address_t address) {

#if defined(EDB_X86) || defined(EDB_X86_64)
	if (edb::v1::debuggeeIs32Bit()) {
		switch (classtype) {
		case 1:
			test_reg_to_ip<X86_REG_EAX, X86_REG_ESP>(resultsDialog, opcode, address);
			break;
		case 2:
			test_reg_to_ip<X86_REG_EBX, X86_REG_ESP>(resultsDialog, opcode, address);
			break;
		case 3:
			test_reg_to_ip<X86_REG_ECX, X86_REG_ESP>(resultsDialog, opcode, address);
			break;
		case 4:
			test_reg_to_ip<X86_REG_EDX, X86_REG_ESP>(resultsDialog, opcode, address);
			break;
		case 5:
			test_reg_to_ip<X86_REG_EBP, X86_REG_ESP>(resultsDialog, opcode, address);
			break;
		case 6:
			test_reg_to_ip<X86_REG_ESP, X86_REG_ESP>(resultsDialog, opcode, address);
			break;
		case 7:
			test_reg_to_ip<X86_REG_ESI, X86_REG_ESP>(resultsDialog, opcode, address);
			break;
		case 8:
			test_reg_to_ip<X86_REG_EDI, X86_REG_ESP>(resultsDialog, opcode, address);
			break;
		case 17:
			test_reg_to_ip<X86_REG_EAX, X86_REG_ESP>(resultsDialog, opcode, address);
			test_reg_to_ip<X86_REG_EBX, X86_REG_ESP>(resultsDialog, opcode, address);
			test_reg_to_ip<X86_REG_ECX, X86_REG_ESP>(resultsDialog, opcode, address);
			test_reg_to_ip<X86_REG_EDX, X86_REG_ESP>(resultsDialog, opcode, address);
			test_reg_to_ip<X86_REG_EBP, X86_REG_ESP>(resultsDialog, opcode, address);
			test_reg_to_ip<X86_REG_ESP, X86_REG_ESP>(resultsDialog, opcode, address);
			test_reg_to_ip<X86_REG_ESI, X86_REG_ESP>(resultsDialog, opcode, address);
			test_reg_to_ip<X86_REG_EDI, X86_REG_ESP>(resultsDialog, opcode, address);
			break;
		case 18:
			// [ESP] -> EIP
			test_esp_add_0<X86_REG_ESP>(resultsDialog, opcode, address);
			break;
		case 19:
			// [ESP + 4] -> EIP
			test_esp_add_regx1<X86_REG_ESP>(resultsDialog, opcode, address);
			break;
		case 20:
			// [ESP + 8] -> EIP
			test_esp_add_regx2<X86_REG_ESP>(resultsDialog, opcode, address);
			break;
		case 21:
			// [ESP - 4] -> EIP
			test_esp_sub_regx1<X86_REG_ESP>(resultsDialog, opcode, address);
			break;
		}
	} else {
		switch (classtype) {
		case 1:
			test_reg_to_ip<X86_REG_RAX, X86_REG_RSP>(resultsDialog, opcode, address);
			break;
		case 2:
			test_reg_to_ip<X86_REG_RBX, X86_REG_RSP>(resultsDialog, opcode, address);
			break;
		case 3:
			test_reg_to_ip<X86_REG_RCX, X86_REG_RSP>(resultsDialog, opcode, address);
			break;
		case 4:
			test_reg_to_ip<X86_REG_RDX, X86_REG_RSP>(resultsDialog, opcode, address);
			break;
		case 5:
			test_reg_to_ip<X86_REG_RBP, X86_REG_RSP>(resultsDialog, opcode, address);
			break;
		case 6:
			test_reg_to_ip<X86_REG_RSP, X86_REG_RSP>(resultsDialog, opcode, address);
			break;
		case 7:
			test_reg_to_ip<X86_REG_RSI, X86_REG_RSP>(resultsDialog, opcode, address);
			break;
		case 8:
			test_reg_to_ip<X86_REG_RDI, X86_REG_RSP>(resultsDialog, opcode, address);
			break;
		case 9:
			test_reg_to_ip<X86_REG_R8, X86_REG_RSP>(resultsDialog, opcode, address);
			break;
		case 10:
			test_reg_to_ip<X86_REG_R9, X86_REG_RSP>(resultsDialog, opcode, address);
			break;
		case 11:
			test_reg_to_ip<X86_REG_R10, X86_REG_RSP>(resultsDialog, opcode, address);
			break;
		case 12:
			test_reg_to_ip<X86_REG_R11, X86_REG_RSP>(resultsDialog, opcode, address);
			break;
		case 13:
			test_reg_to_ip<X86_REG_R12, X86_REG_RSP>(resultsDialog, opcode, address);
			break;
		case 14:
			test_reg_to_ip<X86_REG_R13, X86_REG_RSP>(resultsDialog, opcode, address);
			break;
		case 15:
			test_reg_to_ip<X86_REG_R14, X86_REG_RSP>(resultsDialog, opcode, address);
			break;
		case 16:
			test_reg_to_ip<X86_REG_R15, X86_REG_RSP>(resultsDialog, opcode, address);
			break;
		case 17:
			test_reg_to_ip<X86_REG_RAX, X86_REG_RSP>(resultsDialog, opcode, address);
			test_reg_to_ip<X86_REG_RBX, X86_REG_RSP>(resultsDialog, opcode, address);
			test_reg_to_ip<X86_REG_RCX, X86_REG_RSP>(resultsDialog, opcode, address);
			test_reg_to_ip<X86_REG_RDX, X86_REG_RSP>(resultsDialog, opcode, address);
			test_reg_to_ip<X86_REG_RBP, X86_REG_RSP>(resultsDialog, opcode, address);
			test_reg_to_ip<X86_REG_RSP, X86_REG_RSP>(resultsDialog, opcode, address);
			test_reg_to_ip<X86_REG_RSI, X86_REG_RSP>(resultsDialog, opcode, address);
			test_reg_to_ip<X86_REG_RDI, X86_REG_RSP>(resultsDialog, opcode, address);
			test_reg_to_ip<X86_REG_R8, X86_REG_RSP>(resultsDialog, opcode, address);
			test_reg_to_ip<X86_REG_R9, X86_REG_RSP>(resultsDialog, opcode, address);
			test_reg_to_ip<X86_REG_R10, X86_REG_RSP>(resultsDialog, opcode, address);
			test_reg_to_ip<X86_REG_R11, X86_REG_RSP>(resultsDialog, opcode, address);
			test_reg_to_ip<X86_REG_R12, X86_REG_RSP>(resultsDialog, opcode, address);
			test_reg_to_ip<X86_REG_R13, X86_REG_RSP>(resultsDialog, opcode, address);
			test_reg_to_ip<X86_REG_R14, X86_REG_RSP>(resultsDialog, opcode, address);
			test_reg_to_ip<X86_REG_R15, X86_REG_RSP>(resultsDialog, opcode, address);
			break;
		case 18:
			// [ESP] -> EIP
			test_esp_add_0<X86_REG_RSP>(resultsDialog, opcode, address);
			break;
		case 19:
			// [ESP + 4] -> EIP
			test_esp_add_regx1<X86_REG_RSP>(resultsDialog, opcode, address);
			break;
		case 20:
			// [ESP + 8] -> EIP
			test_esp_add_regx2<X86_REG_RSP>(resultsDialog, opcode, address);
			break;
		case 21:
			// [ESP - 4] -> EIP
			test_esp_sub_regx1<X86_REG_RSP>(resultsDialog, opcode, address);
			break;
		case 22:
			test_deref_reg_to_ip<X86_REG_RAX>(resultsDialog, opcode, address);
			break;
		case 23:
			test_deref_reg_to_ip<X86_REG_RBX>(resultsDialog, opcode, address);
			break;
		case 24:
			test_deref_reg_to_ip<X86_REG_RCX>(resultsDialog, opcode, address);
			break;
		case 25:
			test_deref_reg_to_ip<X86_REG_RDX>(resultsDialog, opcode, address);
			break;
		case 26:
			test_deref_reg_to_ip<X86_REG_RBP>(resultsDialog, opcode, address);
			break;
		case 28:
			test_deref_reg_to_ip<X86_REG_RSI>(resultsDialog, opcode, address);
			break;
		case 29:
			test_deref_reg_to_ip<X86_REG_RDI>(resultsDialog, opcode, address);
			break;
		case 30:
			test_deref_reg_to_ip<X86_REG_R8>(resultsDialog, opcode, address);
			break;
		case 31:
			test_deref_reg_to_ip<X86_REG_R9>(resultsDialog, opcode, address);
			break;
		case 32:
			test_deref_reg_to_ip<X86_REG_R10>(resultsDialog, opcode, address);
			break;
		case 33:
			test_deref_reg_to_ip<X86_REG_R11>(resultsDialog, opcode, address);
			break;
		case 34:
			test_deref_reg_to_ip<X86_REG_R12>(resultsDialog, opcode, address);
			break;
		case 35:
			test_deref_reg_to_ip<X86_REG_R13>(resultsDialog, opcode, address);
			break;
		case 36:
			test_deref_reg_to_ip<X86_REG_R14>(resultsDialog, opcode, address);
			break;
		case 37:
			test_deref_reg_to_ip<X86_REG_R15>(resultsDialog, opcode, address);
			break;
		}
	}
#elif defined(EDB_ARM32)
	// TODO(eteran): implement
#elif defined(EDB_ARM64)
	// TODO(eteran): implement
#endif
}

}

/**
 * @brief DialogOpcodes::DialogOpcodes
 * @param parent
 * @param f
 */
DialogOpcodes::DialogOpcodes(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {
	ui.setupUi(this);
	ui.tableView->verticalHeader()->hide();
	ui.tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	filterModel_ = new QSortFilterProxyModel(this);
	connect(ui.txtSearch, &QLineEdit::textChanged, filterModel_, &QSortFilterProxyModel::setFilterFixedString);

	buttonFind_ = new QPushButton(QIcon::fromTheme("edit-find"), tr("Find"));
	connect(buttonFind_, &QPushButton::clicked, this, [this]() {
		buttonFind_->setEnabled(false);
		ui.progressBar->setValue(0);
		doFind();
		ui.progressBar->setValue(100);
		buttonFind_->setEnabled(true);
	});

	ui.buttonBox->addButton(buttonFind_, QDialogButtonBox::ActionRole);
}

/**
 * @brief DialogOpcodes::showEvent
 */
void DialogOpcodes::showEvent(QShowEvent *) {
	filterModel_->setFilterKeyColumn(3);
	filterModel_->setSourceModel(&edb::v1::memory_regions());
	ui.tableView->setModel(filterModel_);
	ui.progressBar->setValue(0);

	ui.comboBox->clear();

#if defined(EDB_X86) || defined(EDB_X86_64)
	if (edb::v1::debuggeeIs64Bit()) {
		ui.comboBox->addItem("RAX -> RIP", 1);
		ui.comboBox->addItem("RBX -> RIP", 2);
		ui.comboBox->addItem("RCX -> RIP", 3);
		ui.comboBox->addItem("RDX -> RIP", 4);
		ui.comboBox->addItem("RBP -> RIP", 5);
		ui.comboBox->addItem("RSP -> RIP", 6);
		ui.comboBox->addItem("RSI -> RIP", 7);
		ui.comboBox->addItem("RDI -> RIP", 8);
		ui.comboBox->addItem("R8 -> RIP", 9);
		ui.comboBox->addItem("R9 -> RIP", 10);
		ui.comboBox->addItem("R10 -> RIP", 11);
		ui.comboBox->addItem("R11 -> RIP", 12);
		ui.comboBox->addItem("R12 -> RIP", 13);
		ui.comboBox->addItem("R13 -> RIP", 14);
		ui.comboBox->addItem("R14 -> RIP", 15);
		ui.comboBox->addItem("R15 -> RIP", 16);
		ui.comboBox->addItem("ANY REGISTER -> RIP", 17);
		ui.comboBox->addItem("[RSP] -> RIP", 18);
		ui.comboBox->addItem("[RSP + 8] -> RIP", 19);
		ui.comboBox->addItem("[RSP + 16] -> RIP", 20);
		ui.comboBox->addItem("[RSP - 8] -> RIP", 21);
		ui.comboBox->addItem("[RAX] -> RIP", 22);
		ui.comboBox->addItem("[RBX] -> RIP", 23);
		ui.comboBox->addItem("[RCX] -> RIP", 24);
		ui.comboBox->addItem("[RDX] -> RIP", 25);
		ui.comboBox->addItem("[RBP] -> RIP", 26);
		ui.comboBox->addItem("[RSI] -> RIP", 28);
		ui.comboBox->addItem("[RDI] -> RIP", 29);
		ui.comboBox->addItem("[R8] -> RIP", 30);
		ui.comboBox->addItem("[R9] -> RIP", 31);
		ui.comboBox->addItem("[R10] -> RIP", 32);
		ui.comboBox->addItem("[R11] -> RIP", 33);
		ui.comboBox->addItem("[R12] -> RIP", 34);
		ui.comboBox->addItem("[R13] -> RIP", 35);
		ui.comboBox->addItem("[R14] -> RIP", 36);
		ui.comboBox->addItem("[R15] -> RIP", 37);
	} else {
		ui.comboBox->addItem("EAX -> EIP", 1);
		ui.comboBox->addItem("EBX -> EIP", 2);
		ui.comboBox->addItem("ECX -> EIP", 3);
		ui.comboBox->addItem("EDX -> EIP", 4);
		ui.comboBox->addItem("EBP -> EIP", 5);
		ui.comboBox->addItem("ESP -> EIP", 6);
		ui.comboBox->addItem("ESI -> EIP", 7);
		ui.comboBox->addItem("EDI -> EIP", 8);
		ui.comboBox->addItem("ANY REGISTER -> EIP", 17);
		ui.comboBox->addItem("[ESP] -> EIP", 18);
		ui.comboBox->addItem("[ESP + 4] -> EIP", 19);
		ui.comboBox->addItem("[ESP + 8] -> EIP", 20);
		ui.comboBox->addItem("[ESP - 4] -> EIP", 21);

		ui.comboBox->addItem("[EAX] -> EIP", 22);
		ui.comboBox->addItem("[EBX] -> EIP", 23);
		ui.comboBox->addItem("[ECX] -> EIP", 24);
		ui.comboBox->addItem("[EDX] -> EIP", 25);
		ui.comboBox->addItem("[EBP] -> EIP", 26);
		ui.comboBox->addItem("[ESI] -> EIP", 28);
		ui.comboBox->addItem("[EDI] -> EIP", 29);
	}
#elif defined(EDB_ARM32)
	// TODO(eteran): implement
#elif defined(EDB_ARM64)
	// TODO(eteran): implement
#endif
}

/**
 * @brief DialogOpcodes::doFind
 */
void DialogOpcodes::doFind() {

	const int classtype = ui.comboBox->itemData(ui.comboBox->currentIndex()).toInt();

	const QItemSelectionModel *const selModel = ui.tableView->selectionModel();
	const QModelIndexList sel                 = selModel->selectedRows();

	if (sel.size() == 0) {
		QMessageBox::critical(
			this,
			tr("No Region Selected"),
			tr("You must select a region which is to be scanned for the desired opcode."));
		return;
	}

	auto resultsDialog = new DialogResults(this);

	if (IProcess *process = edb::v1::debugger_core->process()) {
		for (const QModelIndex &selected_item : sel) {

			const QModelIndex index = filterModel_->mapToSource(selected_item);

			if (auto region = *reinterpret_cast<const std::shared_ptr<IRegion> *>(index.internalPointer())) {

				edb::address_t start_address     = region->start();
				edb::address_t address           = region->start();
				const edb::address_t end_address = region->end();
				const edb::address_t orig_start  = region->start();

				OpcodeData shift_buffer = {};

				// this will read the rest of the region
				size_t i = 0;
				while (start_address < end_address) {

					// create a reference to the bsa's data so we can pass it to the testXXXX functions
					// but only do so if we have read enough bytes to fill our shift buffer
					if (i >= shift_buffer.size()) {
						run_tests(resultsDialog, classtype, shift_buffer, address - shift_buffer.size());
					}

					uint8_t byte;
					process->readBytes(start_address, &byte, 1);
					util::shl(shift_buffer, byte);

					++start_address;

					ui.progressBar->setValue(util::percentage(address - orig_start, region->size()));
					++address;
					++i;
				}

				// test the stuff at the regions edge
				for (size_t i = 0; i < shift_buffer.size(); ++i) {

					// create a reference to the bsa's data so we can pass it to the testXXXX functions
					run_tests(resultsDialog, classtype, shift_buffer, address - shift_buffer.size());

					// we just shift in 0's and hope it doesn't give false positives
					util::shl(shift_buffer, 0x00);

					ui.progressBar->setValue(util::percentage(address - orig_start, region->size()));
					++address;
				}
			}
		}
	}

	if (resultsDialog->resultCount() == 0) {
		QMessageBox::information(this, tr("No Opcodes Found"), tr("No opcodes were found in the selected region."));
		delete resultsDialog;
	} else {
		resultsDialog->show();
	}
}

}

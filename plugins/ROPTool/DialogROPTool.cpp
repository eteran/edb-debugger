/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogROPTool.h"
#include "ByteShiftArray.h"
#include "DialogResults.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "IRegion.h"
#include "MemoryRegions.h"
#include "edb.h"
#include "util/Math.h"

#include <QDebug>
#include <QHeaderView>
#include <QMessageBox>
#include <QModelIndex>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QtConcurrent/QtConcurrentRun>

namespace ROPToolPlugin {

namespace {

/**
 * @brief Gets the gadget role for the given instruction.
 *
 * @param inst The instruction for which to determine the gadget role.
 * @return A uint32_t representing the gadget role of the instruction.
 */
uint32_t get_gadget_role(const edb::Instruction &inst) {
	switch (inst.operation()) {
	case X86_INS_ADD:
	case X86_INS_ADC:
	case X86_INS_SUB:
	case X86_INS_SBB:
	case X86_INS_IMUL:
	case X86_INS_MUL:
	case X86_INS_IDIV:
	case X86_INS_DIV:
	case X86_INS_INC:
	case X86_INS_DEC:
	case X86_INS_NEG:
	case X86_INS_CMP:
	case X86_INS_DAA:
	case X86_INS_DAS:
	case X86_INS_AAA:
	case X86_INS_AAS:
	case X86_INS_AAM:
	case X86_INS_AAD:
		// ALU ops
		return 0x01;
	case X86_INS_PUSH:
	case X86_INS_PUSHAW:
	case X86_INS_PUSHAL:
	case X86_INS_POP:
	case X86_INS_POPAW:
	case X86_INS_POPAL:
		// stack ops
		return 0x02;
	case X86_INS_AND:
	case X86_INS_OR:
	case X86_INS_XOR:
	case X86_INS_NOT:
	case X86_INS_SAR:
	case X86_INS_SAL:
	case X86_INS_SHR:
	case X86_INS_SHL:
	case X86_INS_SHRD:
	case X86_INS_SHLD:
	case X86_INS_ROR:
	case X86_INS_ROL:
	case X86_INS_RCR:
	case X86_INS_RCL:
	case X86_INS_BT:
	case X86_INS_BTS:
	case X86_INS_BTR:
	case X86_INS_BTC:
	case X86_INS_BSF:
	case X86_INS_BSR:
		// logic ops
		return 0x04;
	case X86_INS_MOV:
	case X86_INS_MOVABS:
	case X86_INS_CMOVA:
	case X86_INS_CMOVAE:
	case X86_INS_CMOVB:
	case X86_INS_CMOVBE:
	case X86_INS_CMOVE:
	case X86_INS_CMOVG:
	case X86_INS_CMOVGE:
	case X86_INS_CMOVL:
	case X86_INS_CMOVLE:
	case X86_INS_CMOVNE:
	case X86_INS_CMOVNO:
	case X86_INS_CMOVNP:
	case X86_INS_CMOVNS:
	case X86_INS_CMOVO:
	case X86_INS_CMOVP:
	case X86_INS_CMOVS:
	case X86_INS_XCHG:
	case X86_INS_BSWAP:
	case X86_INS_XADD:
	case X86_INS_CMPXCHG:
	case X86_INS_CWD:
	case X86_INS_CDQ:
	case X86_INS_CQO:
	case X86_INS_CDQE:
	case X86_INS_CBW:
	case X86_INS_CWDE:
	case X86_INS_MOVSX:
	case X86_INS_MOVZX:
	case X86_INS_MOVSXD:
	case X86_INS_MOVBE:
	case X86_INS_MOVSB:
	case X86_INS_MOVSW:
	case X86_INS_MOVSD:
	case X86_INS_MOVSQ:
	case X86_INS_CMPSB:
	case X86_INS_CMPSW:
	case X86_INS_CMPSD:
	case X86_INS_CMPSQ:
	case X86_INS_SCASB:
	case X86_INS_SCASW:
	case X86_INS_SCASD:
	case X86_INS_SCASQ:
	case X86_INS_LODSB:
	case X86_INS_LODSW:
	case X86_INS_LODSD:
	case X86_INS_LODSQ:
	case X86_INS_STOSB:
	case X86_INS_STOSW:
	case X86_INS_STOSD:
	case X86_INS_STOSQ:
	case X86_INS_CMPXCHG8B:
	case X86_INS_CMPXCHG16B:
		// data ops
		return 0x08;
	default:
		// other ops
		return 0x10;
	}
}

// See issue #457, thanks mrexodia!
/**
 * @brief Checks if a register operand is safe for 64-bit NOP operations.
 *
 * @param op The operand to check.
 * @return true if the operand is safe for 64-bit NOP operations, false otherwise.
 */
bool is_safe_64_nop_reg_op(const edb::Operand &op) {

	if (op->type != X86_OP_REG) {
		return true; // a non-register is safe
	}

	if (edb::v1::debuggeeIs64Bit()) {
		switch (op->reg) {
		case X86_REG_EAX:
		case X86_REG_EBX:
		case X86_REG_ECX:
		case X86_REG_EDX:
		case X86_REG_EBP:
		case X86_REG_ESP:
		case X86_REG_ESI:
		case X86_REG_EDI:
			return false; // 32 bit register modifications clear the high part of the 64 bit register
		default:
			return true; // all other registers are safe
		}
	} else {
		return true;
	}
}

/**
 * @brief Checks if an instruction is an effective NOP (No Operation).
 *
 * @param inst The instruction to check.
 * @return true if the instruction is an effective NOP, false otherwise.
 */
bool is_effective_nop(const edb::Instruction &inst) {

	if (!inst) {
		return false;
	}

	// trivially a nop
	if (is_nop(inst)) {
		return true;
	}

	switch (inst->id) {
	case X86_INS_NOP:
	case X86_INS_PAUSE:
	case X86_INS_FNOP:
		// nop
		return true;
	case X86_INS_MOV:
	case X86_INS_CMOVA:
	case X86_INS_CMOVAE:
	case X86_INS_CMOVB:
	case X86_INS_CMOVBE:
	case X86_INS_CMOVE:
	case X86_INS_CMOVNE:
	case X86_INS_CMOVG:
	case X86_INS_CMOVGE:
	case X86_INS_CMOVL:
	case X86_INS_CMOVLE:
	case X86_INS_CMOVO:
	case X86_INS_CMOVNO:
	case X86_INS_CMOVP:
	case X86_INS_CMOVNP:
	case X86_INS_CMOVS:
	case X86_INS_CMOVNS:
	case X86_INS_MOVAPS:
	case X86_INS_MOVAPD:
	case X86_INS_MOVUPS:
	case X86_INS_MOVUPD:
	case X86_INS_XCHG:
		// mov edi, edi
		return inst[0]->type == X86_OP_REG && inst[1]->type == X86_OP_REG && inst[0]->reg == inst[1]->reg && is_safe_64_nop_reg_op(inst[0]);
	case X86_INS_LEA: {
		// lea eax, [eax + 0]
		auto reg = inst[0]->reg;
		auto mem = inst[1]->mem;
		return inst[0]->type == X86_OP_REG && inst[1]->type == X86_OP_MEM && mem.disp == 0 &&
			   ((mem.index == X86_REG_INVALID && mem.base == reg) ||
				(mem.index == reg && mem.base == X86_REG_INVALID && mem.scale == 1)) &&
			   is_safe_64_nop_reg_op(inst[0]);
	}
	case X86_INS_JMP:
	case X86_INS_JA:
	case X86_INS_JAE:
	case X86_INS_JB:
	case X86_INS_JBE:
	case X86_INS_JE:
	case X86_INS_JNE:
	case X86_INS_JG:
	case X86_INS_JGE:
	case X86_INS_JL:
	case X86_INS_JLE:
	case X86_INS_JO:
	case X86_INS_JNO:
	case X86_INS_JP:
	case X86_INS_JNP:
	case X86_INS_JS:
	case X86_INS_JNS:
	case X86_INS_JECXZ:
	case X86_INS_JRCXZ:
	case X86_INS_JCXZ:
		// jmp 0
		return inst[0]->type == X86_OP_IMM && static_cast<edb::address_t>(inst[0]->imm) == inst.rva() + inst.byteSize();
	case X86_INS_SHL:
	case X86_INS_SHR:
	case X86_INS_ROL:
	case X86_INS_ROR:
	case X86_INS_SAR:
	case X86_INS_SAL:
		// shl eax, 0
		return inst[1]->type == X86_OP_IMM && inst[1]->imm == 0 && is_safe_64_nop_reg_op(inst[0]);
	case X86_INS_SHLD:
	case X86_INS_SHRD:
		// shld eax, ebx, 0
		return inst[2]->type == X86_OP_IMM && inst[2]->imm == 0 && is_safe_64_nop_reg_op(inst[0]) && is_safe_64_nop_reg_op(inst[1]);
	default:
		return false;
	}
}

}

/**
 * @brief Constructs a DialogROPTool object with the specified parent widget and window flags.
 *
 * @param parent The parent widget for this dialog.
 * @param f The window flags for this dialog.
 */
DialogROPTool::DialogROPTool(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);
	ui.tableView->verticalHeader()->hide();
	ui.tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	filterModel_ = new QSortFilterProxyModel(this);
	connect(ui.txtSearch, &QLineEdit::textChanged, filterModel_, &QSortFilterProxyModel::setFilterFixedString);

	buttonFind_ = new QPushButton(QIcon::fromTheme("edit-find"), tr("Find"));
	connect(buttonFind_, &QPushButton::clicked, this, &DialogROPTool::onFindClicked);
	connect(&searchWatcher_, &QFutureWatcher<SearchResult>::finished, this, &DialogROPTool::onFindFinished);

	progressTimer_.setInterval(50);
	connect(&progressTimer_, &QTimer::timeout, this, &DialogROPTool::updateProgress);

	ui.buttonBox->addButton(buttonFind_, QDialogButtonBox::ActionRole);
}

/**
 * @brief Handles the show event for the DialogROPTool.
 *
 * @param event The show event.
 */
void DialogROPTool::showEvent(QShowEvent *) {
	filterModel_->setFilterKeyColumn(3);
	filterModel_->setSourceModel(&edb::v1::memory_regions());
	ui.tableView->setModel(filterModel_);
	ui.progressBar->setValue(0);
}

void DialogROPTool::reject() {
	if (searchRunning_) {
		cancelRequested_.store(true);
		buttonFind_->setEnabled(false);
		buttonFind_->setText(tr("Cancelling..."));
		return;
	}

	QDialog::reject();
}

/**
 * @brief Performs the search for ROP gadgets in the selected memory regions.
 */
DialogROPTool::SearchResult DialogROPTool::doFind(const IProcess *process, const QVector<RegionScan> &regions, bool uniqueOnly) {
	SearchResult result;
	progressDone_.store(0);
	progressTotal_.store(0);

	if (process == nullptr) {
		return result;
	}

	QSet<QString> uniqueResults;
	const size_t totalBytes = std::accumulate(regions.begin(), regions.end(), size_t(0), [](size_t total, const RegionScan &region) {
		return total + static_cast<size_t>(region.end - region.start);
	});
	progressTotal_.store(totalBytes);

	auto makeInstructionString = [](const InstructionList &instructions) {
		if (instructions.empty()) {
			return QString();
		}

		auto it                    = instructions.begin();
		QString instruction_string = QString::fromStdString(edb::v1::formatter().toString(**it++));
		for (; it != instructions.end(); ++it) {
			instruction_string.append(QStringLiteral("; %1").arg(QString::fromStdString(edb::v1::formatter().toString(**it))));
		}

		return instruction_string;
	};

	for (const RegionScan &region : regions) {
		if (cancelRequested_.load()) {
			result.cancelled = true;
			break;
		}

		if (!region.accessible) {
			progressDone_.fetch_add(static_cast<size_t>(region.end - region.start));
			continue;
		}

		edb::address_t start_address     = region.start;
		const edb::address_t end_address = region.end;
		const edb::address_t orig_start  = start_address;

		ByteShiftArray bsa(32);

		while (start_address < end_address) {

			if (cancelRequested_.load()) {
				result.cancelled = true;
				break;
			}

			// read in the next byte
			uint8_t byte;
			if (process->readBytes(start_address, &byte, 1)) {
				bsa << byte;

				const uint8_t *p       = bsa.data();
				const uint8_t *const l = p + bsa.size();
				edb::address_t rva     = start_address - bsa.size() + 1;

				InstructionList instruction_list;

				// eat up any NOPs in front...
				Q_FOREVER {
					auto inst = std::make_shared<edb::Instruction>(p, l, rva);
					if (!is_effective_nop(*inst)) {
						break;
					}

					instruction_list.push_back(inst);
					p += inst->byteSize();
					rva += inst->byteSize();
				}

				auto inst1 = std::make_shared<edb::Instruction>(p, l, rva);
				if (inst1->valid()) {
					instruction_list.push_back(inst1);

					if (is_int(*inst1) && is_immediate(inst1->operand(0)) && (inst1->operand(0)->imm & 0xff) == 0x80) {
						const QString instruction_string = makeInstructionString(instruction_list);
						if (!uniqueOnly || !uniqueResults.contains(instruction_string)) {
							uniqueResults.insert(instruction_string);
							result.results.push_back({inst1->rva(), instruction_string, get_gadget_role(*inst1)});
						}
					} else if (is_sysenter(*inst1)) {
						const QString instruction_string = makeInstructionString(instruction_list);
						if (!uniqueOnly || !uniqueResults.contains(instruction_string)) {
							uniqueResults.insert(instruction_string);
							result.results.push_back({inst1->rva(), instruction_string, get_gadget_role(*inst1)});
						}
					} else if (is_syscall(*inst1)) {
						const QString instruction_string = makeInstructionString(instruction_list);
						if (!uniqueOnly || !uniqueResults.contains(instruction_string)) {
							uniqueResults.insert(instruction_string);
							result.results.push_back({inst1->rva(), instruction_string, get_gadget_role(*inst1)});
						}
					} else if (is_ret(*inst1)) {
						++start_address;
						progressDone_.fetch_add(1);
						continue;
					} else {

						p += inst1->byteSize();
						rva += inst1->byteSize();

						// eat up any NOPs in between...
						Q_FOREVER {
							auto inst = std::make_shared<edb::Instruction>(p, l, rva);
							if (!is_effective_nop(*inst)) {
								break;
							}

							instruction_list.push_back(inst);
							p += inst->byteSize();
							rva += inst->byteSize();
						}

						auto inst2 = std::make_shared<edb::Instruction>(p, l, rva);

						if (is_ret(*inst2)) {
							instruction_list.push_back(inst2);
							const QString instruction_string = makeInstructionString(instruction_list);
							if (!uniqueOnly || !uniqueResults.contains(instruction_string)) {
								uniqueResults.insert(instruction_string);
								result.results.push_back({inst1->rva(), instruction_string, get_gadget_role(*inst1)});
							}
						} else if (inst2->valid() && inst2->operation() == X86_INS_POP) {
							instruction_list.push_back(inst2);
							p += inst2->byteSize();
							rva += inst2->byteSize();

							auto inst3 = std::make_shared<edb::Instruction>(p, l, rva);

							if (inst3->valid() && is_jump(*inst3)) {

								instruction_list.push_back(inst3);

								if (inst2->operandCount() == 1 && is_register(inst2->operand(0))) {
									if (inst3->operandCount() == 1 && is_register(inst3->operand(0))) {
										if (inst2->operand(0)->reg == inst3->operand(0)->reg) {
											const QString instruction_string = makeInstructionString(instruction_list);
											if (!uniqueOnly || !uniqueResults.contains(instruction_string)) {
												uniqueResults.insert(instruction_string);
												result.results.push_back({inst1->rva(), instruction_string, get_gadget_role(*inst1)});
											}
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

			progressDone_.fetch_add(1);
			++start_address;
		}
	}

	return result;
}

void DialogROPTool::onFindClicked() {
	if (searchRunning_) {
		cancelRequested_.store(true);
		buttonFind_->setEnabled(false);
		buttonFind_->setText(tr("Cancelling..."));
		return;
	}

	const QItemSelectionModel *const selModel = ui.tableView->selectionModel();
	const QModelIndexList sel                 = selModel->selectedRows();

	if (sel.empty()) {
		QMessageBox::critical(
			this,
			tr("No Region Selected"),
			tr("You must select a region which is to be scanned for gadgets."));
		return;
	}

	if (edb::v1::debugger_core == nullptr) {
		return;
	}

	QVector<RegionScan> regions;
	regions.reserve(sel.size());
	for (const QModelIndex &selected_item : sel) {
		const QModelIndex index = filterModel_->mapToSource(selected_item);
		if (auto region = *reinterpret_cast<const std::shared_ptr<IRegion> *>(index.internalPointer())) {
			regions.push_back({region->start(), region->end(), region->accessible()});
		}
	}

	if (regions.isEmpty()) {
		return;
	}

	ui.progressBar->setValue(0);
	cancelRequested_.store(false);
	progressDone_.store(0);
	progressTotal_.store(0);
	setSearchRunning(true);
	progressTimer_.start();

	const IProcess *process = edb::v1::debugger_core->process();
	const bool uniqueOnly   = ui.checkUnique->isChecked();
	searchWatcher_.setFuture(QtConcurrent::run([this, process, regions, uniqueOnly]() {
		return doFind(process, regions, uniqueOnly);
	}));
}

void DialogROPTool::onFindFinished() {
	progressTimer_.stop();
	updateProgress();

	const SearchResult result = searchWatcher_.result();
	setSearchRunning(false);

	if (result.allocationFailed) {
		QMessageBox::critical(
			this,
			tr("Memory Allocation Error"),
			tr("Unable to satisfy memory allocation request while scanning for gadgets."));
		return;
	}

	if (result.cancelled) {
		return;
	}

	auto resultsDialog = new DialogResults(this);
	for (const ResultsModel::Result &resultItem : result.results) {
		resultsDialog->addResult(resultItem);
	}

	if (resultsDialog->resultCount() == 0) {
		QMessageBox::information(this, tr("No Results"), tr("No Rop Gadgets found in the selected region."));
		delete resultsDialog;
	} else {
		resultsDialog->show();
	}
}

void DialogROPTool::updateProgress() {
	const size_t done  = progressDone_.load();
	const size_t total = progressTotal_.load();

	if (total == 0) {
		ui.progressBar->setValue(0);
		return;
	}

	ui.progressBar->setValue(util::percentage(done, total));
}

void DialogROPTool::setSearchRunning(bool running) {
	searchRunning_ = running;

	if (searchRunning_) {
		buttonFind_->setEnabled(true);
		buttonFind_->setIcon(QIcon::fromTheme("process-stop"));
		buttonFind_->setText(tr("Cancel"));
	} else {
		buttonFind_->setEnabled(true);
		buttonFind_->setIcon(QIcon::fromTheme("edit-find"));
		buttonFind_->setText(tr("Find"));
		ui.progressBar->setValue(100);
	}
}

}

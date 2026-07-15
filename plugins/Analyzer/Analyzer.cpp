/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "Analyzer.h"
#include "AnalyzerWidget.h"
#include "Configuration.h"
#include "DialogXRefs.h"
#include "Function.h"
#include "IBinary.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "ISymbolManager.h"
#include "IThread.h"
#include "Instruction.h"
#include "MemoryRegions.h"
#include "OptionsPage.h"
#include "Prototype.h"
#include "SpecifiedFunctions.h"
#include "State.h"
#include "edb.h"
#include "util/Math.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSettings>
#include <QStack>
#include <QToolBar>
#include <QtDebug>

#include <cstring>
#include <functional>

namespace AnalyzerPlugin {

namespace {

constexpr int MinRefCount = 2;
/**
 * @brief Returns true if the function at the given address is not marked as non-returning.
 *
 * @param address
 * @return
 */
bool will_return(edb::address_t address) {

	const std::optional<Symbol> symbol = edb::v1::symbol_manager().find(address);
	if (symbol) {
		const QString symname   = symbol->name_no_prefix;
		const QString func_name = symname.mid(0, symname.indexOf(QLatin1Char('@')));

		if (const edb::Prototype *const info = edb::v1::get_function_info(func_name)) {
			if (info->noreturn) {
				return false;
			}
		}
	}

	return true;
}

/**
 * @brief Returns true if the given symbol represents the ELF process entry point (_start).
 *
 * @param sym
 * @return
 */
bool is_entrypoint(const Symbol &sym) {
#ifdef Q_OS_UNIX
	return sym.name_no_prefix == QStringLiteral("_start");
#else
	return false;
#endif
}

/**
 * @brief Returns true if the function at the given address begins with an unconditional jump, indicating a thunk.
 *
 * @param address
 * @return true if the first instruction of the function is a jmp
 */
bool is_thunk(edb::address_t address) {

	uint8_t buf[edb::Instruction::MaxSize];
	if (const int buf_size = edb::v1::get_instruction_bytes(address, buf)) {
		const edb::Instruction inst(buf, buf + buf_size, address);
		return is_unconditional_jump(inst);
	}

	return false;
}

/**
 * @brief Classifies each function in the map as either a thunk or a standard function based on its entry instruction.
 *
 * @param results
 */
void set_function_types(IAnalyzer::FunctionMap *results) {

	Q_ASSERT(results);

	// give bonus if we have a symbol for the address
	for (auto it = results->begin(); it != results->end(); ++it) {

		Function &function = it.value();

		if (function.empty()) {
			qDebug() << "Function at " << function.entryAddress().toHexString() << " is empty, skipping type classification";
			continue;
		}

		Q_ASSERT(!function.empty());
		if (is_thunk(function.entryAddress())) {
			function.type = Function::Thunk;
		} else {
			function.type = Function::Standard;
		}
	}
}

/**
 * @brief Returns the entry point address of the binary module that contains the given memory region.
 *
 * @param region
 * @return
 */
edb::address_t module_entry_point(const std::shared_ptr<IRegion> &region) {
	// NOTE(eteran): because modern ELF files actually have the ELF header in its
	// own, non-executable section that precedes the main one, so this may fail...
	if (std::unique_ptr<IBinary> binary_info = edb::v1::get_binary_info(region)) {
		return binary_info->entryPoint();
	}

	// if it does, we can just try to see if there is a region that fits the bill
	// one page before us. it's a guess, but it's not a bad one
	const size_t page_size          = edb::v1::debugger_core->pageSize();
	const edb::address_t prevRegion = region->start() - page_size;
	if (std::shared_ptr<IRegion> region = edb::v1::memory_regions().findRegion(prevRegion)) {
		if (std::unique_ptr<IBinary> binary_info = edb::v1::get_binary_info(region)) {
			return binary_info->entryPoint();
		}
	}

	return 0;
}

}

/**
 * @brief Constructs the Analyzer plugin object.
 *
 * @param parent
 */
Analyzer::Analyzer(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief Returns the plugin's configuration options widget.
 *
 * @return
 */
QWidget *Analyzer::optionsPage() {
	return new OptionsPage;
}

/**
 * @brief Creates and returns the Analyzer plugin menu, adding toolbar and dock widgets on first call.
 *
 * @param parent
 * @return
 */
QMenu *Analyzer::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if (!menu_) {
		menu_ = new QMenu(tr("Analyzer"), parent);
		menu_->addAction(tr("Show &Specified Functions"), this, &Analyzer::showSpecified);

		if (edb::v1::debugger_core) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
			menu_->addAction(tr("&Analyze %1's Region").arg(edb::v1::debugger_core->instructionPointer().toUpper()), QKeySequence(tr("Ctrl+A")), this, &Analyzer::doIpAnalysis);
#else
			menu_->addAction(tr("&Analyze %1's Region").arg(edb::v1::debugger_core->instructionPointer().toUpper()), this, &Analyzer::doIpAnalysis, QKeySequence(tr("Ctrl+A")));
#endif
		}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		menu_->addAction(tr("&Analyze Viewed Region"), QKeySequence(tr("Ctrl+Shift+A")), this, &Analyzer::doViewAnalysis);
#else
		menu_->addAction(tr("&Analyze Viewed Region"), this, &Analyzer::doViewAnalysis, QKeySequence(tr("Ctrl+Shift+A")));
#endif

		// if we are dealing with a main window (and we are...)
		// add the dock object
		if (auto main_window = qobject_cast<QMainWindow *>(edb::v1::debugger_ui)) {
			analyzerWidget_ = new AnalyzerWidget;

			// make the toolbar widget and _name_ it, it is important to name it so
			// that it's state is saved in the GUI info
			auto toolbar = new QToolBar(tr("Region Analysis"), main_window);
			toolbar->setAllowedAreas(Qt::TopToolBarArea | Qt::BottomToolBarArea);
			toolbar->setObjectName(QString::fromUtf8("Region Analysis"));
			toolbar->addWidget(analyzerWidget_);

			// add it to the dock
			main_window->addToolBar(Qt::TopToolBarArea, toolbar);

			// make the menu and add the show/hide toggle for the widget
			menu_->addAction(toolbar->toggleViewAction());
		}
	}

	return menu_;
}

/**
 * @brief Registers this instance as the global active analyzer.
 */
void Analyzer::privateInit() {
	edb::v1::set_analyzer(this);
}

/**
 * @brief Opens or raises the dialog listing user-specified function start addresses.
 */
void Analyzer::showSpecified() {
	static auto dialog = new SpecifiedFunctions(edb::v1::debugger_ui);
	dialog->show();
}

/**
 * @brief Analyzes the memory region containing the current instruction pointer.
 */
void Analyzer::doIpAnalysis() {
	if (IProcess *process = edb::v1::debugger_core->process()) {
		if (std::shared_ptr<IThread> thread = process->currentThread()) {
			State state;
			thread->getState(&state);

			const edb::address_t address = state.instructionPointer();
			if (std::shared_ptr<IRegion> region = edb::v1::memory_regions().findRegion(address)) {
				doAnalysis(region);
			}
		}
	}
}

/**
 * @brief Analyzes the memory region currently visible in the CPU disassembly view.
 */
void Analyzer::doViewAnalysis() {
	doAnalysis(edb::v1::current_cpu_view_region());
}

/**
 * @brief Marks the currently selected address as a known function start and invalidates the region's analysis.
 */
void Analyzer::markFunctionStart() {

	const edb::address_t address = edb::v1::cpu_selected_address();
	if (std::shared_ptr<IRegion> region = edb::v1::memory_regions().findRegion(address)) {
		qDebug("Added %s to the list of known functions", qPrintable(address.toPointerString()));
		specifiedFunctions_.insert(address);
		invalidateDynamicAnalysis(region);
	}
}

/**
 * @brief Opens a dialog listing all cross-references that target the currently selected address.
 */
void Analyzer::showXrefs() {

	const edb::address_t address = edb::v1::cpu_selected_address();

	auto dialog = new DialogXRefs(edb::v1::debugger_ui);

	for (const RegionData &data : analysisInfo_) {
		for (const BasicBlock &bb : data.basicBlocks) {
			const std::vector<std::pair<edb::address_t, edb::address_t>> refs = bb.references();

			for (const auto &[from, to] : refs) {
				if (to == address) {
					dialog->addReference({from, to});
				}
			}
		}
	}

	dialog->setWindowTitle(tr("X-Refs For %1").arg(address.toPointerString()));
	dialog->show();
}
/**
 * @brief Jumps the disassembly view to the entry address of the function containing the selected address.
 */
void Analyzer::gotoFunctionStart() {

	const edb::address_t address = edb::v1::cpu_selected_address();

	Function function;
	if (findContainingFunction(address, &function)) {
		edb::v1::jump_to_address(function.entryAddress());
		return;
	}

	QMessageBox::critical(
		nullptr,
		tr("Goto Function Start"),
		tr("The selected instruction is not inside of a known function. Have you run an analysis of this region?"));
}

/**
 * @brief Jumps the disassembly view to the last instruction of the function containing the selected address.
 */
void Analyzer::gotoFunctionEnd() {

	const edb::address_t address = edb::v1::cpu_selected_address();

	Function function;
	if (findContainingFunction(address, &function)) {
		edb::v1::jump_to_address(function.lastInstruction());
		return;
	}

	QMessageBox::critical(
		nullptr,
		tr("Goto Function End"),
		tr("The selected instruction is not inside of a known function. Have you run an analysis of this region?"));
}
/**
 * @brief Returns the list of CPU context menu actions contributed by the Analyzer plugin.
 *
 * @return
 */
QList<QAction *> Analyzer::cpuContextMenu() {

	auto action_find                = new QAction(tr("Analyze Here"), this);
	auto action_goto_function_start = new QAction(tr("Goto Function Start"), this);
	auto action_goto_function_end   = new QAction(tr("Goto Function End"), this);
	auto action_mark_function_start = new QAction(tr("Mark As Function Start"), this);
	auto action_xrefs               = new QAction(tr("Show X-Refs"), this);

	connect(action_find, &QAction::triggered, this, &Analyzer::doViewAnalysis);
	connect(action_goto_function_start, &QAction::triggered, this, &Analyzer::gotoFunctionStart);
	connect(action_goto_function_end, &QAction::triggered, this, &Analyzer::gotoFunctionEnd);
	connect(action_mark_function_start, &QAction::triggered, this, &Analyzer::markFunctionStart);
	connect(action_xrefs, &QAction::triggered, this, &Analyzer::showXrefs);

	return {
		action_find,
		action_goto_function_start,
		action_goto_function_end,
		action_mark_function_start,
		action_xrefs,
	};
}

/**
 * @brief Runs a full analysis pass on the given region, showing progress and repainting the CPU view on completion.
 *
 * @param region
 */
void Analyzer::doAnalysis(const std::shared_ptr<IRegion> &region) {
	if (region && region->size() != 0) {
		QProgressDialog progress(tr("Performing Analysis"), QString(), 0, 100, edb::v1::debugger_ui);
		connect(this, &Analyzer::updateProgress, &progress, &QProgressDialog::setValue);
		progress.show();
		progress.setValue(0);
		analyze(region);
		edb::v1::repaint_cpu_view();
	}
}

/**
 * @brief Seeds the known-function list with the address of main() if it can be located in the region.
 *
 * @param data
 */
void Analyzer::bonusMain(RegionData *data) const {

	Q_ASSERT(data);

	if (IProcess *process = edb::v1::debugger_core->process()) {
		const QString s = process->executable();
		if (!s.isEmpty()) {
			if (const edb::address_t main = edb::v1::locate_main_function()) {
				if (data->region->contains(main)) {
					data->knownFunctions.insert(main);
				}
			}
		}
	}
}

/**
 * @brief Seeds the known-function list with all code symbol addresses that fall within the region.
 *
 * @param data
 */
void Analyzer::bonusSymbols(RegionData *data) {

	Q_ASSERT(data);

	// give bonus if we have a symbol for the address
	const std::vector<Symbol> symbols = edb::v1::symbol_manager().symbols();

	for (const Symbol &sym : symbols) {
		const edb::address_t addr = sym.address;

		// NOTE(eteran): we special case the module entry point because while we bonus the
		// application's entry point in bonusEntryPoint, each module can have one which
		// is called on load by the linker, including the linker itself! And unfortunately
		// at least on some systems, it is a data symbol, not a code symbol
		if (data->region->contains(addr) && (sym.isCode() || is_entrypoint(sym))) {
			qDebug("[Analyzer] adding: %s <%s>", qPrintable(sym.name), qPrintable(addr.toPointerString()));
			data->knownFunctions.insert(addr);
		}
	}
}

/**
 * @brief Seeds the known-function list with all user-specified function addresses that fall within the region.
 *
 * @param data
 */
void Analyzer::bonusMarkedFunctions(RegionData *data) {

	Q_ASSERT(data);

	for (const edb::address_t addr : specifiedFunctions_) {
		if (data->region->contains(addr)) {
			qDebug("[Analyzer] adding user marked function: <%s>", qPrintable(addr.toPointerString()));
			data->knownFunctions.insert(addr);
		}
	}
}

/**
 * @brief Hook for identifying executable header information within the region; currently a no-op.
 *
 * @param data
 */
void Analyzer::identHeader(Analyzer::RegionData *data) {
	Q_UNUSED(data)
}

bool split_function(Function &func) {

	for (auto bb_it = func.begin(); bb_it != func.end(); ++bb_it) {
		auto &[address, bb] = *bb_it;
		Q_UNUSED(address)

		if (bb.size() <= 1) {
			continue;
		}

		for (auto it = bb.begin(); it != bb.end(); ++it) {
			const std::shared_ptr<edb::Instruction> &insn = *it;

			// if it's a call and not the last instruction of the BB
			// then split!
			if (is_call(*insn) && insn != bb.back()) {

				auto &&[newBlocksFirst, newBlocksSecond] = bb.splitBlock(insn);
				func.erase(bb_it);

				Q_ASSERT(!newBlocksFirst.empty());
				Q_ASSERT(!newBlocksSecond.empty());

				func.insert(newBlocksFirst);
				func.insert(newBlocksSecond);

				return true;
			}
		}
	}

	return false;
}

void Analyzer::splitBlocks(RegionData *data) {
	Q_ASSERT(data);

	for (auto it = data->functions.begin(); it != data->functions.end(); ++it) {
		const edb::address_t function = it.key();
		Function &func                = it.value();

		while (split_function(func)) {
			continue;
		}
	}
}

void Analyzer::computeNonReturning(Analyzer::RegionData *data) {
	Q_UNUSED(data);

	for (auto it = data->functions.begin(); it != data->functions.end(); ++it) {

		auto func = &it.value();

		bool noreturn = true;

		// Check all of the basic blocks in the function. If **all** of them don't indicate a return, then we can mark the function as non-returning.
		// Otherwise, we just assume it returns. This is a conservative approach, but it is better than marking a function as non-returning when
		// it actually does return.
		for (const auto &[address, bb] : it.value()) {
			Q_UNUSED(address);

			if (!bb.empty()) {
				const std::shared_ptr<edb::Instruction> &inst = bb.back();

				// If the basic block ends with a halt, then it is non-returning. Continue to the next basic block.
				if (is_halt(*inst)) {
					continue;
				}

				// If the basic block ends with a UD instruction, then it is non-returning. Continue to the next basic block.
				if (is_ud(*inst)) {
					continue;
				}

				// If the basic block ends with a jump instruction that lands within the function, then it is non-returning. Continue to the next basic block.
				if (is_jump(*inst)) {
					Q_ASSERT(inst->operandCount() == 1);
					const edb::Operand op = inst->operand(0);

					if (is_immediate(op)) {

						const edb::address_t ea = op->imm;
						if (func->containsBlock(ea)) {
							continue;
						}
					}
				}

				// Anything else, be conservative and assume it will return. Mark the function as returning and break out of the loop.
				noreturn = false;
				break;
			}
		}

		if (noreturn) {
			qDebug() << "Function at" << it.key().toPointerString() << "is non-returning";
		}
	}
}

/**
 * @brief Performs recursive disassembly from all known entry points to build the complete function and basic block maps.
 *
 * @param data
 */
void Analyzer::collectFunctions(Analyzer::RegionData *data) {
	Q_ASSERT(data);

	// results
	QHash<edb::address_t, BasicBlock> basic_blocks;
	FunctionMap functions;

	// push all known functions onto a stack
	QStack<edb::address_t> known_functions;
	for (const edb::address_t function : data->knownFunctions) {
		known_functions.push(function);
	}

	// push all fuzzy function too...
	for (const edb::address_t function : data->fuzzyFunctions) {
		known_functions.push(function);
	}

	// process all functions that are known
	while (!known_functions.empty()) {
		const edb::address_t function_address = known_functions.pop();

		if (!functions.contains(function_address)) {

			QStack<edb::address_t> blocks;
			blocks.push(function_address);

			Function func;

			// process are basic blocks that are known
			while (!blocks.empty()) {

				const edb::address_t block_address = blocks.pop();
				edb::address_t address             = block_address;
				BasicBlock block;

				if (!basic_blocks.contains(block_address)) {
					while (data->region->contains(address)) {

						uint8_t buffer[edb::Instruction::MaxSize];
						const int buf_size = edb::v1::get_instruction_bytes(address, buffer);
						if (buf_size == 0) {
							break;
						}

						auto inst = std::make_shared<edb::Instruction>(buffer, buffer + buf_size, address);
						if (!inst->valid()) {
							break;
						}

						block.push_back(inst);

						if (is_call(*inst)) {

							// note the destination and move on
							// we special case some simple things.
							// also this is an opportunity to find call tables.
							const edb::Operand op = inst->operand(0);
							if (is_immediate(op)) {
								const edb::address_t ea = op->imm;

								// skip over ones which are: "call <label>; label:"
								if (ea != address + inst->byteSize()) {
									known_functions.push(ea);

									if (!will_return(ea)) {
										break;
									}

									block.addReference(address, ea);
								}
							} else if (is_expression(op)) {
								// looks like: "call [...]", if it is of the form, call [C + REG]
								// then it may be a jump table using REG as an offset
							} else if (is_register(op)) {
								// looks like: "call <reg>", this is this may be a callback
								// if we can use analysis to determine that it's a constant
								// we can figure it out...
								// eventually, we should figure out the parameters of the function
								// to see if we can know what the target is
							}

						} else if (is_unconditional_jump(*inst)) {

							Q_ASSERT(inst->operandCount() >= 1);
							const edb::Operand op = inst->operand(0);

							// TODO(eteran): we need some heuristic for detecting when this is
							//               a call/ret -> jmp optimization
							if (is_immediate(op)) {
								const edb::address_t ea = op->imm;

								if (functions.contains(ea)) {
									functions[ea].addReference();
								} else if ((ea - function_address) > 0x2000u) {
									known_functions.push(ea);
								} else {
									blocks.push(ea);
								}

								block.addReference(address, ea);
							}
							break;
						} else if (is_conditional_jump(*inst)) {

							Q_ASSERT(inst->operandCount() == 1);
							const edb::Operand op = inst->operand(0);

							if (is_immediate(op)) {

								const edb::address_t ea = op->imm;

								blocks.push(ea);
								blocks.push(address + inst->byteSize());

								block.addReference(address, ea);
							}
							break;
						} else if (is_terminator(*inst)) {
							break;
						}

						address += inst->byteSize();
					}

					if (!block.empty()) {
						basic_blocks.insert(block_address, block);

						if (block_address >= function_address) {
							func.insert(block);
						}
					}
				}
			}

			if (!func.empty()) {
				functions.insert(function_address, func);
			}
		} else {
			functions[function_address].addReference();
		}
	}

	std::swap(data->basicBlocks, basic_blocks);
	std::swap(data->functions, functions);
}

/**
 * @brief Heuristically discovers additional function entry points by scanning for call targets and ENDBR instructions.
 *
 * @param data
 */
void Analyzer::collectFuzzyFunctions(RegionData *data) {
	Q_ASSERT(data);

	data->fuzzyFunctions.clear();

	if (data->fuzzy) {

		QHash<edb::address_t, int> fuzzy_functions;

		uint8_t *const first = (data->memory).data();
		uint8_t *const last  = &first[data->memory.size()];

		uint8_t *p = first;

		// fuzzy_functions, known_functions
		for (edb::address_t addr = data->region->start(); addr != data->region->end(); ++addr) {
			if (auto inst = edb::Instruction(p, last, addr)) {
				if (is_call(inst)) {

					// note the destination and move on
					// we special case some simple things.
					// also this is an opportunity to find call tables.
					const edb::Operand op = inst[0];
					if (is_immediate(op)) {
						const edb::address_t ea = op->imm;

						// skip over ones which are: "call <label>; label:"
						if (ea != addr + inst.byteSize()) {

							if (!data->knownFunctions.contains(ea)) {
								fuzzy_functions[ea]++;
							}
						}
					}
#if defined(EDB_X86) || defined(EDB_X86_64)
#if CS_API_MAJOR >= 4
				} else if (inst->id == X86_INS_ENDBR64 || inst->id == X86_INS_ENDBR32) {

					// Intel's CET stuff actually helps us identify functions pretty easily
					if (!data->knownFunctions.contains(addr)) {
						fuzzy_functions[addr] = MinRefCount + 1;
					}
#endif
#endif
				}
			}
			++p;
		}

		// transfer results to data->fuzzy_functions
		for (auto it = fuzzy_functions.begin(); it != fuzzy_functions.end(); ++it) {
			if (it.value() > MinRefCount) {
				data->fuzzyFunctions.insert(it.key());
			}
		}
	}
}

/**
 * @brief Runs all analysis pipeline steps on the given region, caching results and emitting progress updates.
 *
 * @param region
 */
void Analyzer::analyze(const std::shared_ptr<IRegion> &region) {

	QElapsedTimer t;
	t.start();

	RegionData &region_data = analysisInfo_[region->start()];
	qDebug() << "[Analyzer] Region name:" << region->name();

	QSettings settings;
	const bool fuzzy = settings.value(QStringLiteral("Analyzer/fuzzy_logic_functions.enabled"), true).toBool();

	const size_t page_size  = edb::v1::debugger_core->pageSize();
	const size_t page_count = region->size() / page_size;

	QVector<uint8_t> memory = edb::v1::read_pages(region->start(), page_count);

	const QByteArray md5      = (!memory.isEmpty()) ? edb::v1::get_md5(memory) : QByteArray();
	const QByteArray prev_md5 = region_data.md5;

	if (md5 == prev_md5 && fuzzy == region_data.fuzzy) {
		qDebug("[Analyzer] region unchanged, using previous analysis");
		return;
	}

	region_data.basicBlocks.clear();
	region_data.functions.clear();
	region_data.fuzzyFunctions.clear();
	region_data.knownFunctions.clear();

	region_data.memory = memory;
	region_data.region = region;
	region_data.md5    = md5;
	region_data.fuzzy  = fuzzy;

	const struct {
		const char *message;
		std::function<void()> function;
	} analysis_steps[] = {
		{"identifying executable headers...", [this, &region_data]() { identHeader(&region_data); }},
		{"adding entry points to the list...", [this, &region_data]() { bonusEntryPoint(&region_data); }},
		{"attempting to add 'main' to the list...", [this, &region_data]() { bonusMain(&region_data); }},
		{"attempting to add functions with symbols to the list...", [this, &region_data]() { bonusSymbols(&region_data); }},
		{"attempting to add marked functions to the list...", [this, &region_data]() { bonusMarkedFunctions(&region_data); }},
		{"attempting to collect functions with fuzzy analysis...", [this, &region_data]() { collectFuzzyFunctions(&region_data); }},
		{"collecting basic blocks...", [this, &region_data]() { collectFunctions(&region_data); }},
		{"splitting basic blocks...", [this, &region_data]() { splitBlocks(&region_data); }},
		{"computing non-returning functions...", [this, &region_data]() { computeNonReturning(&region_data); }},
	};

	const int total_steps = sizeof(analysis_steps) / sizeof(analysis_steps[0]);

	Q_EMIT updateProgress(util::percentage(0, total_steps));
	for (int i = 0; i < total_steps; ++i) {
		qDebug("[Analyzer] %s", analysis_steps[i].message);
		analysis_steps[i].function();
		Q_EMIT updateProgress(util::percentage(i + 1, total_steps));
	}

	qDebug("[Analyzer] determining function types...");

	set_function_types(&region_data.functions);

	qDebug("[Analyzer] complete");
	Q_EMIT updateProgress(100);

	if (analyzerWidget_) {
		analyzerWidget_->update();
	}

	qDebug("[Analyzer] elapsed: %lld ms", t.elapsed());
}

/**
 * @brief Returns the role of the given address (function start, body, end, or unknown) relative to known functions.
 *
 * @param address
 * @return
 */
IAnalyzer::AddressCategory Analyzer::category(edb::address_t address) const {

	Function func;
	if (findContainingFunction(address, &func)) {
		if (address == func.entryAddress()) {
			return ADDRESS_FUNC_START;
		}

		if (address == func.endAddress()) {
			return ADDRESS_FUNC_END;
		}

		return ADDRESS_FUNC_BODY;
	}
	return ADDRESS_FUNC_UNKNOWN;
}

/**
 * @brief Returns the map of all functions found within the given memory region.
 *
 * @param region
 * @return
 */
IAnalyzer::FunctionMap Analyzer::functions(const std::shared_ptr<IRegion> &region) const {
	return analysisInfo_[region->start()].functions;
}

/**
 * @brief Returns the combined map of all functions across every analyzed region.
 *
 * @return
 */
IAnalyzer::FunctionMap Analyzer::functions() const {
	FunctionMap results;
	for (auto &it : analysisInfo_) {
		results.insert(it.functions);
	}
	return results;
}

/**
 * @brief Finds the function that contains the given address and stores it in the output parameter.
 *
 * @param address
 * @param function
 * @return
 */
bool Analyzer::findContainingFunction(edb::address_t address, Function *function) const {

	Q_ASSERT(function);

	if (std::shared_ptr<IRegion> region = edb::v1::memory_regions().findRegion(address)) {
		const FunctionMap &funcs = functions(region);

		// upperBound returns the first item that is >= address here, or end().
		// Note the ">", it does not do the opposite of lowerBound!
		auto iter = funcs.upperBound(address);
		if (iter == funcs.end()) {
			return false;
		}

		// handle address == entrypoint case
		if ((*iter).entryAddress() == address) {
			*function = *iter;
			return true;
		}

		// go to previous function and test it
		if (iter == funcs.begin()) {
			return false;
		}

		const Function &func = *(--iter);
		if (address >= func.entryAddress() && address <= func.endAddress()) {
			*function = func;
			return true;
		}
	}
	return false;
}

/**
 * @brief Invokes a callback for each known function whose body overlaps the given address range.
 *
 * Calls functor once for every function that exists between the start and end
 * addresses. This includes functions whose bodies include the start address.
 * start and end must reside in the same region. If the functor returns false,
 * iteration is halted.
 *
 * @param start
 * @param end
 * @param functor
 * @return true if all functions were iterated,
 * false if the iteration was halted early.
 */
bool Analyzer::forFuncsInRange(edb::address_t start, edb::address_t end, std::function<bool(const Function *)> functor) const {
	if (std::shared_ptr<IRegion> region = edb::v1::memory_regions().findRegion(start)) {
		const FunctionMap &funcs = functions(region);
		auto it                  = funcs.lowerBound(start - 4096);

		while (it != funcs.end()) {
			edb::address_t f_start = it->entryAddress();
			edb::address_t f_end   = it->endAddress();

			// Only works if FunctionMap is a QMap
			if (f_start > end) {
				return true;
			}
			// ranges overlap: http://stackoverflow.com/a/3269471
			if (f_start <= end && start <= f_end) {
				if (!functor(&(*it))) {
					return false;
				}
			}

			++it;
		}
	}
	return true;
}

/**
 * @brief Seeds the known-function list with the module's binary entry point address.
 *
 * @param data
 */
void Analyzer::bonusEntryPoint(RegionData *data) const {

	Q_ASSERT(data);

	if (edb::address_t entry = module_entry_point(data->region)) {

		// if the entry seems like a relative one (like for a library)
		// then add the base of its image
		if (entry < data->region->start()) {
			entry += data->region->start();
		}

		qDebug("[Analyzer] found entry point: %s", qPrintable(entry.toPointerString()));

		if (data->region->contains(entry)) {
			data->knownFunctions.insert(entry);
		}
	}
}

/**
 * @brief Clears cached analysis for the given region and removes any user-specified functions within it.
 *
 * @param region
 */
void Analyzer::invalidateAnalysis(const std::shared_ptr<IRegion> &region) {
	invalidateDynamicAnalysis(region);
	const auto specifiedFunctionsCopy = specifiedFunctions_;
	for (const edb::address_t addr : specifiedFunctionsCopy) {
		if (addr >= region->start() && addr < region->end()) {
			specifiedFunctions_.remove(addr);
		}
	}
}

/**
 * @brief Resets the dynamically-computed analysis data for the given region without affecting user-specified functions.
 *
 * @param region
 */
void Analyzer::invalidateDynamicAnalysis(const std::shared_ptr<IRegion> &region) {

	RegionData info;
	info.region = region;
	info.fuzzy  = false;

	analysisInfo_[region->start()] = info;
}

/**
 * @brief Clears all cached analysis data and all user-specified function addresses.
 */
void Analyzer::invalidateAnalysis() {
	analysisInfo_.clear();
	specifiedFunctions_.clear();
}

/**
 * @brief Returns the entry point of the function containing the given address, or an error if none is found.
 *
 * @param address
 * @return the entry point of the function which contains <address>
 */
Result<edb::address_t, QString> Analyzer::findContainingFunction(edb::address_t address) const {

	Function function;
	if (findContainingFunction(address, &function)) {
		return function.entryAddress();
	}

	return make_unexpected(tr("Containing Function Not Found"));
}

}

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
 * @brief will_return
 * @param address
 * @return
 */
bool will_return(edb::address_t address) {

	const std::shared_ptr<Symbol> symbol = edb::v1::symbol_manager().find(address);
	if (symbol) {
		const QString symname   = symbol->name_no_prefix;
		const QString func_name = symname.mid(0, symname.indexOf("@"));

		if (const edb::Prototype *const info = edb::v1::get_function_info(func_name)) {
			if (info->noreturn) {
				return false;
			}
		}
	}

	return true;
}

/**
 * @brief is_entrypoint
 * @param sym
 * @return
 */
bool is_entrypoint(const Symbol &sym) {
#ifdef Q_OS_UNIX
	return sym.name_no_prefix == "_start";
#else
	return false;
#endif
}

/**
 * @brief is_thunk
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
 * @brief set_function_types
 * @param results
 */
void set_function_types(IAnalyzer::FunctionMap *results) {

	Q_ASSERT(results);

	// give bonus if we have a symbol for the address
	std::for_each(results->begin(), results->end(), [](Function &function) {
		if (is_thunk(function.entryAddress())) {
			function.setType(Function::Thunk);
		} else {
			function.setType(Function::Standard);
		}
	});
}

/**
 * @brief module_entry_point
 * @param region
 * @return
 */
edb::address_t module_entry_point(const std::shared_ptr<IRegion> &region) {
	if (std::unique_ptr<IBinary> binary_info = edb::v1::get_binary_info(region)) {
		return binary_info->entryPoint();
	}

	return 0;
}

}

/**
 * @brief Analyzer::Analyzer
 * @param parent
 */
Analyzer::Analyzer(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief Analyzer::optionsPage
 * @return
 */
QWidget *Analyzer::optionsPage() {
	return new OptionsPage;
}

/**
 * @brief Analyzer::menu
 * @param parent
 * @return
 */
QMenu *Analyzer::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if (!menu_) {
		menu_ = new QMenu(tr("Analyzer"), parent);
		menu_->addAction(tr("Show &Specified Functions"), this, SLOT(showSpecified()));

		if (edb::v1::debugger_core) {
			menu_->addAction(tr("&Analyze %1's Region").arg(edb::v1::debugger_core->instructionPointer().toUpper()), this, SLOT(doIpAnalysis()), QKeySequence(tr("Ctrl+A")));
		}

		menu_->addAction(tr("&Analyze Viewed Region"), this, SLOT(doViewAnalysis()), QKeySequence(tr("Ctrl+Shift+A")));

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
 * @brief Analyzer::privateInit
 */
void Analyzer::privateInit() {
	edb::v1::set_analyzer(this);
}

/**
 * @brief Analyzer::showSpecified
 */
void Analyzer::showSpecified() {
	static auto dialog = new SpecifiedFunctions(edb::v1::debugger_ui);
	dialog->show();
}

/**
 * @brief Analyzer::doIpAnalysis
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
 * @brief Analyzer::doViewAnalysis
 */
void Analyzer::doViewAnalysis() {
	doAnalysis(edb::v1::current_cpu_view_region());
}

/**
 * @brief Analyzer::markFunctionStart
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
 * @brief Analyzer::showXrefs
 */
void Analyzer::showXrefs() {

	const edb::address_t address = edb::v1::cpu_selected_address();

	auto dialog = new DialogXRefs(edb::v1::debugger_ui);

	for (const RegionData &data : analysisInfo_) {
		for (const BasicBlock &bb : data.basicBlocks) {
			const std::vector<std::pair<edb::address_t, edb::address_t>> refs = bb.references();

			for (auto it = refs.begin(); it != refs.end(); ++it) {
				if (it->second == address) {
					dialog->addReference(*it);
				}
			}
		}
	}

	dialog->setWindowTitle(tr("X-Refs For %1").arg(address.toPointerString()));
	dialog->show();
}
/**
 * @brief Analyzer::gotoFunctionStart
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
 * @brief Analyzer::gotoFunctionEnd
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
 * @brief Analyzer::cpuContextMenu
 * @return
 */
QList<QAction *> Analyzer::cpuContextMenu() {

	QList<QAction *> ret;

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

	ret << action_find << action_goto_function_start << action_goto_function_end << action_mark_function_start << action_xrefs;

	return ret;
}

/**
 * @brief Analyzer::doAnalysis
 * @param region
 */
void Analyzer::doAnalysis(const std::shared_ptr<IRegion> &region) {
	if (region && region->size() != 0) {
		QProgressDialog progress(tr("Performing Analysis"), nullptr, 0, 100, edb::v1::debugger_ui);
		connect(this, &Analyzer::updateProgress, &progress, &QProgressDialog::setValue);
		progress.show();
		progress.setValue(0);
		analyze(region);
		edb::v1::repaint_cpu_view();
	}
}

/**
 * @brief Analyzer::bonusMain
 * @param data
 */
void Analyzer::bonusMain(RegionData *data) const {

	Q_ASSERT(data);

	const QString s = edb::v1::debugger_core->process()->executable();
	if (!s.isEmpty()) {
		if (const edb::address_t main = edb::v1::locate_main_function()) {
			if (data->region->contains(main)) {
				data->knownFunctions.insert(main);
			}
		}
	}
}

/**
 * @brief Analyzer::bonusSymbols
 * @param data
 */
void Analyzer::bonusSymbols(RegionData *data) {

	Q_ASSERT(data);

	// give bonus if we have a symbol for the address
	const std::vector<std::shared_ptr<Symbol>> symbols = edb::v1::symbol_manager().symbols();

	for (const std::shared_ptr<Symbol> &sym : symbols) {
		const edb::address_t addr = sym->address;

		// NOTE(eteran): we special case the module entry point because while we bonus the
		// application's entry point in bonusEntryPoint, each module can have one which
		// is called on load by the linker, including the linker itself! And unfortunately
		// at least on some systems, it is a data symbol, not a code symbol
		if (data->region->contains(addr) && (sym->isCode() || is_entrypoint(*sym))) {
			qDebug("[Analyzer] adding: %s <%s>", qPrintable(sym->name), qPrintable(addr.toPointerString()));
			data->knownFunctions.insert(addr);
		}
	}
}

/**
 * @brief Analyzer::bonusMarkedFunctions
 * @param data
 */
void Analyzer::bonusMarkedFunctions(RegionData *data) {

	Q_ASSERT(data);

	Q_FOREACH (const edb::address_t addr, specifiedFunctions_) {
		if (data->region->contains(addr)) {
			qDebug("[Analyzer] adding user marked function: <%s>", qPrintable(addr.toPointerString()));
			data->knownFunctions.insert(addr);
		}
	}
}

/**
 * @brief Analyzer::identHeader
 * @param data
 */
void Analyzer::identHeader(Analyzer::RegionData *data) {
	Q_UNUSED(data)
}

/**
 * @brief Analyzer::collectFunctions
 * @param data
 */
void Analyzer::collectFunctions(Analyzer::RegionData *data) {
	Q_ASSERT(data);

	// results
	QHash<edb::address_t, BasicBlock> basic_blocks;
	FunctionMap functions;

	// push all known functions onto a stack
	QStack<edb::address_t> known_functions;
	Q_FOREACH (const edb::address_t function, data->knownFunctions) {
		known_functions.push(function);
	}

	// push all fuzzy function too...
	Q_FOREACH (const edb::address_t function, data->fuzzyFunctions) {
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
 * @brief Analyzer::collectFuzzyFunctions
 * @param data
 */
void Analyzer::collectFuzzyFunctions(RegionData *data) {
	Q_ASSERT(data);

	data->fuzzyFunctions.clear();

	if (data->fuzzy) {

		QHash<edb::address_t, int> fuzzy_functions;

		uint8_t *const first = &data->memory[0];
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
 * @brief Analyzer::analyze
 * @param region
 */
void Analyzer::analyze(const std::shared_ptr<IRegion> &region) {

	QElapsedTimer t;
	t.start();

	RegionData &region_data = analysisInfo_[region->start()];
	qDebug() << "[Analyzer] Region name:" << region->name();

	QSettings settings;
	const bool fuzzy = settings.value("Analyzer/fuzzy_logic_functions.enabled", true).toBool();

	const size_t page_size  = edb::v1::debugger_core->pageSize();
	const size_t page_count = region->size() / page_size;

	QVector<uint8_t> memory = edb::v1::read_pages(region->start(), page_count);

	const QByteArray md5      = (!memory.isEmpty()) ? edb::v1::get_md5(memory) : QByteArray();
	const QByteArray prev_md5 = region_data.md5;

	if (md5 != prev_md5 || fuzzy != region_data.fuzzy) {

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

	} else {
		qDebug("[Analyzer] region unchanged, using previous analysis");
	}

	qDebug("[Analyzer] elapsed: %lld ms", t.elapsed());
}

/**
 * @brief Analyzer::category
 * @param address
 * @return
 */
IAnalyzer::AddressCategory Analyzer::category(edb::address_t address) const {

	Function func;
	if (findContainingFunction(address, &func)) {
		if (address == func.entryAddress()) {
			return ADDRESS_FUNC_START;
		} else if (address == func.endAddress()) {
			return ADDRESS_FUNC_END;
		} else {
			return ADDRESS_FUNC_BODY;
		}
	}
	return ADDRESS_FUNC_UNKNOWN;
}

/**
 * @brief Analyzer::functions
 * @param region
 * @return
 */
IAnalyzer::FunctionMap Analyzer::functions(const std::shared_ptr<IRegion> &region) const {
	return analysisInfo_[region->start()].functions;
}

/**
 * @brief Analyzer::functions
 * @return
 */
IAnalyzer::FunctionMap Analyzer::functions() const {
	FunctionMap results;
	for (auto &it : analysisInfo_) {
		results.unite(it.functions);
	}
	return results;
}

/**
 * @brief Analyzer::findContainingFunction
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
 * @brief Analyzer::forFuncsInRange
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
 * @brief Analyzer::bonusEntryPoint
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
 * @brief Analyzer::invalidateAnalysis
 * @param region
 */
void Analyzer::invalidateAnalysis(const std::shared_ptr<IRegion> &region) {
	invalidateDynamicAnalysis(region);
	Q_FOREACH (const edb::address_t addr, specifiedFunctions_) {
		if (addr >= region->start() && addr < region->end()) {
			specifiedFunctions_.remove(addr);
		}
	}
}

/**
 * @brief Analyzer::invalidateDynamicAnalysis
 * @param region
 */
void Analyzer::invalidateDynamicAnalysis(const std::shared_ptr<IRegion> &region) {

	RegionData info;
	info.region = region;
	info.fuzzy  = false;

	analysisInfo_[region->start()] = info;
}

/**
 * @brief Analyzer::invalidateAnalysis
 */
void Analyzer::invalidateAnalysis() {
	analysisInfo_.clear();
	specifiedFunctions_.clear();
}

/**
 * @brief Analyzer::findContainingFunction
 * @param address
 * @return the entry point of the function which contains <address>
 */
Result<edb::address_t, QString> Analyzer::findContainingFunction(edb::address_t address) const {

	Function function;
	if (findContainingFunction(address, &function)) {
		return function.entryAddress();
	} else {
		return make_unexpected(tr("Containing Function Not Found"));
	}
}

}

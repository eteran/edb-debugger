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
#include "Util.h"
#include "edb.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSettings>
#include <QStack>
#include <QTime>
#include <QToolBar>
#include <QtDebug>

#include <functional>
#include <cstring>

namespace AnalyzerPlugin {

namespace {

constexpr int MIN_REFCOUNT = 2;

//------------------------------------------------------------------------------
// Name: is_thunk
// Desc: basically returns true if the first instruction of the function is a
//       jmp
//------------------------------------------------------------------------------
bool is_thunk(edb::address_t address) {

	quint8 buf[edb::Instruction::MAX_SIZE];
	if(const int buf_size = edb::v1::get_instruction_bytes(address, buf)) {
		const edb::Instruction inst(buf, buf + buf_size, address);
		return is_unconditional_jump(inst);
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: set_function_types
// Desc:
//------------------------------------------------------------------------------
void set_function_types(IAnalyzer::FunctionMap *results) {

	Q_ASSERT(results);

	// give bonus if we have a symbol for the address
	std::for_each(results->begin(), results->end(), [](Function &function) {
		if(is_thunk(function.entry_address())) {
			function.set_type(Function::FUNCTION_THUNK);
		} else {
			function.set_type(Function::FUNCTION_STANDARD);
		}
	});
}

//------------------------------------------------------------------------------
// Name: module_entry_point
// Desc:
//------------------------------------------------------------------------------
edb::address_t module_entry_point(const std::shared_ptr<IRegion> &region) {

	edb::address_t entry = 0;
	if(std::unique_ptr<IBinary> binary_info = edb::v1::get_binary_info(region)) {
		entry = binary_info->entry_point();
	}

	return entry;
}

}

/**
 * @brief Analyzer::Analyzer
 * @param parent
 */
Analyzer::Analyzer(QObject *parent) : QObject(parent) {

}

//------------------------------------------------------------------------------
// Name: options_page
// Desc:
//------------------------------------------------------------------------------
QWidget *Analyzer::options_page() {
	return new OptionsPage;
}

//------------------------------------------------------------------------------
// Name: menu
// Desc:
//------------------------------------------------------------------------------
QMenu *Analyzer::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if(!menu_) {
		menu_ = new QMenu(tr("Analyzer"), parent);
		menu_->addAction(tr("Show &Specified Functions"), this, SLOT(show_specified()));

		if(edb::v1::debugger_core) {
			menu_->addAction(tr("&Analyze %1's Region").arg(edb::v1::debugger_core->instruction_pointer().toUpper()), this, SLOT(do_ip_analysis()), QKeySequence(tr("Ctrl+A")));
		}

		menu_->addAction(tr("&Analyze Viewed Region"), this, SLOT(do_view_analysis()), QKeySequence(tr("Ctrl+Shift+A")));

		// if we are dealing with a main window (and we are...)
		// add the dock object
		if(auto main_window = qobject_cast<QMainWindow *>(edb::v1::debugger_ui)) {
			analyzer_widget_ = new AnalyzerWidget;

			// make the toolbar widget and _name_ it, it is important to name it so
			// that it's state is saved in the GUI info
			auto toolbar = new QToolBar(tr("Region Analysis"), main_window);
			toolbar->setAllowedAreas(Qt::TopToolBarArea | Qt::BottomToolBarArea);
			toolbar->setObjectName(QString::fromUtf8("Region Analysis"));
			toolbar->addWidget(analyzer_widget_);

			// add it to the dock
			main_window->addToolBar(Qt::TopToolBarArea, toolbar);

			// make the menu and add the show/hide toggle for the widget
			menu_->addAction(toolbar->toggleViewAction());
		}
	}

	return menu_;
}

//------------------------------------------------------------------------------
// Name: private_init
// Desc:
//------------------------------------------------------------------------------
void Analyzer::private_init() {
	edb::v1::set_analyzer(this);
}

//------------------------------------------------------------------------------
// Name: show_specified
// Desc:
//------------------------------------------------------------------------------
void Analyzer::show_specified() {
	static auto dialog = new SpecifiedFunctions(edb::v1::debugger_ui);
	dialog->show();
}

//------------------------------------------------------------------------------
// Name: do_ip_analysis
// Desc:
//------------------------------------------------------------------------------
void Analyzer::do_ip_analysis() {
	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(std::shared_ptr<IThread> thread = process->current_thread()) {
			State state;
			thread->get_state(&state);

			const edb::address_t address = state.instruction_pointer();
			if(std::shared_ptr<IRegion> region = edb::v1::memory_regions().find_region(address)) {
				do_analysis(region);
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: do_view_analysis
// Desc:
//------------------------------------------------------------------------------
void Analyzer::do_view_analysis() {
	do_analysis(edb::v1::current_cpu_view_region());
}

//------------------------------------------------------------------------------
// Name: mark_function_start
// Desc:
//------------------------------------------------------------------------------
void Analyzer::mark_function_start() {

	const edb::address_t address = edb::v1::cpu_selected_address();
	if(std::shared_ptr<IRegion> region = edb::v1::memory_regions().find_region(address)) {
		qDebug("Added %s to the list of known functions", qPrintable(address.toPointerString()));
		specified_functions_.insert(address);
		invalidate_dynamic_analysis(region);
	}
}

//------------------------------------------------------------------------------
// Name: mark_function_start
// Desc:
//------------------------------------------------------------------------------
void Analyzer::show_xrefs() {

	const edb::address_t address = edb::v1::cpu_selected_address();
	
	auto dialog = new DialogXRefs(edb::v1::debugger_ui);
	
	for(const RegionData &data : analysis_info_) {
		for(const BasicBlock &bb : data.basic_blocks) {	
			std::vector<std::pair<edb::address_t, edb::address_t>> refs = bb.refs();
			
			for(auto it = refs.begin(); it != refs.end(); ++it) {
				if(it->second == address) {					
					dialog->addReference(*it);
				}	
			}
		}
	}
	
	dialog->setWindowTitle(tr("X-Refs For %1").arg(address.toPointerString()));
	dialog->show();
}

//------------------------------------------------------------------------------
// Name: goto_function_start
// Desc:
//------------------------------------------------------------------------------
void Analyzer::goto_function_start() {

	const edb::address_t address = edb::v1::cpu_selected_address();

	Function function;
	if(find_containing_function(address, &function)) {
		edb::v1::jump_to_address(function.entry_address());
		return;
	}

	QMessageBox::critical(
	    nullptr,
		tr("Goto Function Start"),
		tr("The selected instruction is not inside of a known function. Have you run an analysis of this region?"));
}

//------------------------------------------------------------------------------
// Name: goto_function_end
// Desc:
//------------------------------------------------------------------------------
void Analyzer::goto_function_end() {

	const edb::address_t address = edb::v1::cpu_selected_address();

	Function function;
	if(find_containing_function(address, &function)) {
		edb::v1::jump_to_address(function.last_instruction());
		return;
	}

	QMessageBox::critical(
	    nullptr,
		tr("Goto Function End"),
		tr("The selected instruction is not inside of a known function. Have you run an analysis of this region?"));
}

//------------------------------------------------------------------------------
// Name: cpu_context_menu
// Desc:
//------------------------------------------------------------------------------
QList<QAction *> Analyzer::cpu_context_menu() {

	QList<QAction *> ret;

	auto action_find                = new QAction(tr("Analyze Here"), this);
	auto action_goto_function_start = new QAction(tr("Goto Function Start"), this);
	auto action_goto_function_end   = new QAction(tr("Goto Function End"), this);
	auto action_mark_function_start = new QAction(tr("Mark As Function Start"), this);
	auto action_xrefs               = new QAction(tr("Show X-Refs"), this);

	connect(action_find,                &QAction::triggered, this, &Analyzer::do_view_analysis);
	connect(action_goto_function_start, &QAction::triggered, this, &Analyzer::goto_function_start);
	connect(action_goto_function_end,   &QAction::triggered, this, &Analyzer::goto_function_end);
	connect(action_mark_function_start, &QAction::triggered, this, &Analyzer::mark_function_start);
	connect(action_xrefs,               &QAction::triggered, this, &Analyzer::show_xrefs);

	ret << action_find << action_goto_function_start << action_goto_function_end << action_mark_function_start << action_xrefs;

	return ret;
}

//------------------------------------------------------------------------------
// Name: do_analysis
// Desc:
//------------------------------------------------------------------------------
void Analyzer::do_analysis(const std::shared_ptr<IRegion> &region) {
	if(region && region->size() != 0) {
		QProgressDialog progress(tr("Performing Analysis"), nullptr, 0, 100, edb::v1::debugger_ui);
		connect(this, &Analyzer::update_progress, &progress, &QProgressDialog::setValue);
		progress.show();
		progress.setValue(0);
		analyze(region);
		edb::v1::repaint_cpu_view();
	}
}

//------------------------------------------------------------------------------
// Name: bonus_main
// Desc:
//------------------------------------------------------------------------------
void Analyzer::bonus_main(RegionData *data) const {

	Q_ASSERT(data);

	const QString s = edb::v1::debugger_core->process()->executable();
	if(!s.isEmpty()) {
		if(const edb::address_t main = edb::v1::locate_main_function()) {
			if(data->region->contains(main)) {
				data->known_functions.insert(main);
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: bonus_symbols
// Desc:
//------------------------------------------------------------------------------
void Analyzer::bonus_symbols(RegionData *data) {

	Q_ASSERT(data);

	// give bonus if we have a symbol for the address
	const QList<std::shared_ptr<Symbol>> symbols = edb::v1::symbol_manager().symbols();

	for(const std::shared_ptr<Symbol> &sym: symbols) {
		const edb::address_t addr = sym->address;

		if(data->region->contains(addr) && sym->is_code()) {
			qDebug("[Analyzer] adding: %s <%s>", qPrintable(sym->name), qPrintable(addr.toPointerString()));
			data->known_functions.insert(addr);
		}
	}
}

//------------------------------------------------------------------------------
// Name: bonus_marked_functions
// Desc:
//------------------------------------------------------------------------------
void Analyzer::bonus_marked_functions(RegionData *data) {

	Q_ASSERT(data);

	Q_FOREACH(const edb::address_t addr, specified_functions_) {
		if(data->region->contains(addr)) {
			qDebug("[Analyzer] adding user marked function: <%s>", qPrintable(addr.toPointerString()));
			data->known_functions.insert(addr);
		}
	}
}

//------------------------------------------------------------------------------
// Name: ident_header
// Desc:
//------------------------------------------------------------------------------
void Analyzer::ident_header(Analyzer::RegionData *data) {
	Q_UNUSED(data)
}

//------------------------------------------------------------------------------
// Name: analyze
// Desc:
//------------------------------------------------------------------------------
void Analyzer::collect_functions(Analyzer::RegionData *data) {
	Q_ASSERT(data);

	// results
	QHash<edb::address_t, BasicBlock> basic_blocks;
	FunctionMap                       functions;

	// push all known functions onto a stack
	QStack<edb::address_t> known_functions;
	Q_FOREACH(const edb::address_t function, data->known_functions) {
		known_functions.push(function);
	}

	// push all fuzzy function too...
	Q_FOREACH(const edb::address_t function, data->fuzzy_functions) {
		known_functions.push(function);
	}

	// process all functions that are known
	while(!known_functions.empty()) {
		const edb::address_t function_address = known_functions.pop();

		if(!functions.contains(function_address)) {

			QStack<edb::address_t> blocks;
			blocks.push(function_address);

			Function func;

			// process are basic blocks that are known
			while(!blocks.empty()) {

				const edb::address_t block_address = blocks.pop();
				edb::address_t address             = block_address;
				BasicBlock     block;

				if(!basic_blocks.contains(block_address)) {
					while(data->region->contains(address)) {

						quint8 buffer[edb::Instruction::MAX_SIZE];
						const int buf_size = edb::v1::get_instruction_bytes(address, buffer);
						if(buf_size == 0) {
							break;
						}

						auto inst = std::make_shared<edb::Instruction>(buffer, buffer + buf_size, address);
						if(!inst->valid()) {
							break;
						}

						block.push_back(inst);

						if(is_call(*inst)) {

							// note the destination and move on
							// we special case some simple things.
							// also this is an opportunity to find call tables.
							const edb::Operand op = inst->operand(0);
							if(is_immediate(op)) {
								const edb::address_t ea = op->imm;

								// skip over ones which are: "call <label>; label:"
								if(ea != address + inst->byte_size()) {
									known_functions.push(ea);

									if(!will_return(ea)) {
										break;
									}
									
									block.addRef(address, ea);
									
								}
							} else if(is_expression(op)) {
								// looks like: "call [...]", if it is of the form, call [C + REG]
								// then it may be a jump table using REG as an offset
							} else if(is_register(op)) {
								// looks like: "call <reg>", this is this may be a callback
								// if we can use analysis to determine that it's a constant
								// we can figure it out...
								// eventually, we should figure out the parameters of the function
								// to see if we can know what the target is
							}

						} else if(is_unconditional_jump(*inst)) {

							Q_ASSERT(inst->operand_count() >= 1);
							const edb::Operand op = inst->operand(0);

							// TODO(eteran): we need some heuristic for detecting when this is
							//               a call/ret -> jmp optimization
							if(is_immediate(op)) {
								const edb::address_t ea = op->imm;


								if(functions.contains(ea)) {
									functions[ea].add_reference();
								} else if((ea - function_address) > 0x2000u) {
									known_functions.push(ea);
								} else {
									blocks.push(ea);
								}
								
								block.addRef(address, ea);
							}
							break;
						} else if(is_conditional_jump(*inst)) {

							Q_ASSERT(inst->operand_count() == 1);
							const edb::Operand op = inst->operand(0);

							if(is_immediate(op)) {
							
								const edb::address_t ea = op->imm;
							
								blocks.push(ea);
								blocks.push(address + inst->byte_size());
								
								block.addRef(address, ea);
							}
							break;
						} else if(is_terminator(*inst)) {
							break;
						}

						address += inst->byte_size();
					}

					if(!block.empty()) {
						basic_blocks.insert(block_address, block);

						if(block_address >= function_address) {
							func.insert(block);
						}
					}
				}
			}

			if(!func.empty()) {
				functions.insert(function_address, func);
			}
		} else {
			functions[function_address].add_reference();
		}
	}

#if 0
	qDebug() << "----------Basic Blocks----------";
	for(auto it = basic_blocks.begin(); it != basic_blocks.end(); ++it) {
		qDebug("%s:", qPrintable(it.key().toPointerString()));

		for(auto &&inst : it.value()) {
			qDebug("\t%s: %s", qPrintable(edb::address_t(inst->rva()).toPointerString()), edb::v1::formatter().to_string(*inst).c_str());
		}
	}
	qDebug() << "----------Basic Blocks----------";
#endif

	std::swap(data->basic_blocks, basic_blocks);
	std::swap(data->functions, functions);
}

//------------------------------------------------------------------------------
// Name: collect_fuzzy_functions
// Desc:
//------------------------------------------------------------------------------
void Analyzer::collect_fuzzy_functions(RegionData *data) {
	Q_ASSERT(data);

	data->fuzzy_functions.clear();

	if(data->fuzzy) {

		QHash<edb::address_t, int> fuzzy_functions;

		quint8 *const first = &data->memory[0];
		quint8 *const last  = &first[data->memory.size()];
		quint8 *p           = first;

		// fuzzy_functions, known_functions
		for(edb::address_t addr = data->region->start(); addr != data->region->end(); ++addr) {
			const edb::Instruction inst(p, last, addr);
			if(inst) {
				if(is_call(inst)) {

					// note the destination and move on
					// we special case some simple things.
					// also this is an opportunity to find call tables.
					const edb::Operand op = inst[0];
					if(is_immediate(op)) {
						const edb::address_t ea = op->imm;

						// skip over ones which are: "call <label>; label:"
						if(ea != addr + inst.byte_size()) {

							if(!data->known_functions.contains(ea)) {
								fuzzy_functions[ea]++;
							}
						}
					}
				}
			}

			++p;
		}

		// transfer results to data->fuzzy_functions
		for(auto it = fuzzy_functions.begin(); it != fuzzy_functions.end(); ++it) {
			if(it.value() > MIN_REFCOUNT) {
				data->fuzzy_functions.insert(it.key());
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: analyze
// Desc:
//------------------------------------------------------------------------------
void Analyzer::analyze(const std::shared_ptr<IRegion> &region) {

	QTime t;
	t.start();

	RegionData &region_data = analysis_info_[region->start()];
	qDebug() << "[Analyzer] Region name:" << region->name();

	QSettings settings;
	const bool fuzzy = settings.value("Analyzer/fuzzy_logic_functions.enabled", true).toBool();

	const size_t page_size  = edb::v1::debugger_core->page_size();
	const size_t page_count = region->size() / page_size;

	QVector<quint8> memory = edb::v1::read_pages(region->start(), page_count);

	const QByteArray md5      = (!memory.isEmpty()) ? edb::v1::get_md5(memory) : QByteArray();
	const QByteArray prev_md5 = region_data.md5;

	if(md5 != prev_md5 || fuzzy != region_data.fuzzy) {

		region_data.basic_blocks.clear();
		region_data.functions.clear();
		region_data.fuzzy_functions.clear();
		region_data.known_functions.clear();

		region_data.memory = memory;
		region_data.region = region;
		region_data.md5    = md5;
		region_data.fuzzy  = fuzzy;

		const struct {
			const char             *message;
			std::function<void()> function;
		} analysis_steps[] = {
			{ "identifying executable headers...",                       [this, &region_data]() { ident_header(&region_data);            } },
			{ "adding entry points to the list...",                      [this, &region_data]() { bonus_entry_point(&region_data);       } },
			{ "attempting to add 'main' to the list...",                 [this, &region_data]() { bonus_main(&region_data);              } },
			{ "attempting to add functions with symbols to the list...", [this, &region_data]() { bonus_symbols(&region_data);           } },
			{ "attempting to add marked functions to the list...",       [this, &region_data]() { bonus_marked_functions(&region_data);  } },
			{ "attempting to collect functions with fuzzy analysis...",  [this, &region_data]() { collect_fuzzy_functions(&region_data); } },
			{ "collecting basic blocks...",                              [this, &region_data]() { collect_functions(&region_data);       } },
		};

		const int total_steps = sizeof(analysis_steps) / sizeof(analysis_steps[0]);

		Q_EMIT update_progress(util::percentage(0, total_steps));
		for(int i = 0; i < total_steps; ++i) {
			qDebug("[Analyzer] %s", analysis_steps[i].message);
			analysis_steps[i].function();
			Q_EMIT update_progress(util::percentage(i + 1, total_steps));
		}

		qDebug("[Analyzer] determining function types...");

		set_function_types(&region_data.functions);

		qDebug("[Analyzer] complete");
		Q_EMIT update_progress(100);

		if(analyzer_widget_) {
			analyzer_widget_->update();
		}


	} else {
		qDebug("[Analyzer] region unchanged, using previous analysis");
	}

	qDebug("[Analyzer] elapsed: %d ms", t.elapsed());
}

//------------------------------------------------------------------------------
// Name: category
// Desc:
//------------------------------------------------------------------------------
IAnalyzer::AddressCategory Analyzer::category(edb::address_t address) const {

	Function func;
	if(find_containing_function(address, &func)) {
		if(address == func.entry_address()) {
			return ADDRESS_FUNC_START;
		} else if(address == func.end_address()) {
			return ADDRESS_FUNC_END;
		} else {
			return ADDRESS_FUNC_BODY;
		}
	}
	return ADDRESS_FUNC_UNKNOWN;
}

//------------------------------------------------------------------------------
// Name: functions
// Desc:
//------------------------------------------------------------------------------
IAnalyzer::FunctionMap Analyzer::functions(const std::shared_ptr<IRegion> &region) const {
	return analysis_info_[region->start()].functions;
}

//------------------------------------------------------------------------------
// Name: functions
// Desc:
//------------------------------------------------------------------------------
IAnalyzer::FunctionMap Analyzer::functions() const {
	FunctionMap results;
	for(auto &it : analysis_info_) {
		results.unite(it.functions);
	}
	return results;
}

//------------------------------------------------------------------------------
// Name: find_containing_function
// Desc:
//------------------------------------------------------------------------------
bool Analyzer::find_containing_function(edb::address_t address, Function *function) const {

	Q_ASSERT(function);

	if(std::shared_ptr<IRegion> region = edb::v1::memory_regions().find_region(address)) {
		const FunctionMap &funcs = functions(region);

		// upperBound returns the first item that is >= address here, or end().
		// Note the ">", it does not do the opposite of lowerBound!
		auto iter = funcs.upperBound(address);
		if (iter == funcs.end()) {
			return false;
		}

		// handle address == entrypoint case
		if ((*iter).entry_address() == address) {
			*function = *iter;
			return true;
		}

		// go to previous function and test it
		if (iter == funcs.begin()) {
			return false;
		}

		const Function& func = *(--iter);
		if (address >= func.entry_address() && address <= func.end_address()) {
			*function = func;
			return true;
		}
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: for_funcs_in_range
// Desc: Calls functor once for every function that exists between the start and
// end addresses. This includes functions whose bodies include the start address.
// start and end must reside in the same region. If the functor returns false,
// iteration is halted. Return value is true if all functions were iterated,
// false if the iteration was halted early.
//------------------------------------------------------------------------------
bool Analyzer::for_funcs_in_range(const edb::address_t start, const edb::address_t end, std::function<bool(const Function*)> functor) const {
	if (std::shared_ptr<IRegion> region = edb::v1::memory_regions().find_region(start)) {
		const FunctionMap &funcs = functions(region);
		auto it = funcs.lowerBound(start - 4096);

		while (it != funcs.end()) {
			edb::address_t f_start = it->entry_address();
			edb::address_t f_end = it->end_address();

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

//------------------------------------------------------------------------------
// Name: bonus_entry_point
// Desc:
//------------------------------------------------------------------------------
void Analyzer::bonus_entry_point(RegionData *data) const {

	Q_ASSERT(data);

	if(edb::address_t entry = module_entry_point(data->region)) {

		// if the entry seems like a relative one (like for a library)
		// then add the base of its image
		if(entry < data->region->start()) {
			entry += data->region->start();
		}

		qDebug("[Analyzer] found entry point: %s", qPrintable(entry.toPointerString()));

		if(data->region->contains(entry)) {
			data->known_functions.insert(entry);
		}
	}
}

//------------------------------------------------------------------------------
// Name: invalidate_analysis
// Desc:
//------------------------------------------------------------------------------
void Analyzer::invalidate_analysis(const std::shared_ptr<IRegion> &region) {
	invalidate_dynamic_analysis(region);
	Q_FOREACH(const edb::address_t addr, specified_functions_) {
		if(addr >= region->start() && addr < region->end()) {
			specified_functions_.remove(addr);
		}
	}
}

//------------------------------------------------------------------------------
// Name: invalidate_dynamic_analysis
// Desc:
//------------------------------------------------------------------------------
void Analyzer::invalidate_dynamic_analysis(const std::shared_ptr<IRegion> &region) {

	RegionData info;
	info.region = region;
	info.fuzzy  = false;

	analysis_info_[region->start()] = info;
}

//------------------------------------------------------------------------------
// Name: invalidate_analysis
// Desc:
//------------------------------------------------------------------------------
void Analyzer::invalidate_analysis() {
	analysis_info_.clear();
	specified_functions_.clear();
}

//------------------------------------------------------------------------------
// Name: find_containing_function
// Desc: returns the entry point of the function which contains <address>
//------------------------------------------------------------------------------
Result<edb::address_t, QString> Analyzer::find_containing_function(edb::address_t address) const {

	Function function;
	if(find_containing_function(address, &function)) {
		return function.entry_address();
	} else {
		return make_unexpected(tr("Containing Function Not Found"));
	}
}

//------------------------------------------------------------------------------
// Name: will_return
// Desc:
//------------------------------------------------------------------------------
bool Analyzer::will_return(edb::address_t address) const {

	const std::shared_ptr<Symbol> symbol = edb::v1::symbol_manager().find(address);
	if(symbol) {
		const QString symname = symbol->name_no_prefix;
		const QString func_name = symname.mid(0, symname.indexOf("@"));
		
		if(const edb::Prototype *const info = edb::v1::get_function_info(func_name)) {
			if(info->noreturn) {
				return false;
			}
		}
	}


	return true;
}

}

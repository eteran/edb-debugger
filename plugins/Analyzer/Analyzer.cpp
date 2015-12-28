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
#include "OptionsPage.h"
#include "AnalyzerWidget.h"
#include "SpecifiedFunctions.h"
#include "IBinary.h"
#include "IDebugger.h"
#include "ISymbolManager.h"
#include "Instruction.h"
#include "MemoryRegions.h"
#include "State.h"
#include "Util.h"
#include "edb.h"

#include <QCoreApplication>
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

#if QT_VERSION >= 0x050000
#ifdef QT_CONCURRENT_LIB
#include <QtConcurrent>
#endif
#elif QT_VERSION >= 0x040800
#include <QtConcurrentMap>

#ifndef QT_NO_CONCURRENT
#define QT_CONCURRENT_LIB
#endif

#endif

namespace Analyzer {

namespace {

const int MIN_REFCOUNT = 2;

//------------------------------------------------------------------------------
// Name: module_entry_point
// Desc:
//------------------------------------------------------------------------------
edb::address_t module_entry_point(const IRegion::pointer &region) {

	edb::address_t entry = 0;
	if(auto binary_info = edb::v1::get_binary_info(region)) {
		entry = binary_info->entry_point();
	}

	return entry;
}

}

//------------------------------------------------------------------------------
// Name: Analyzer
// Desc:
//------------------------------------------------------------------------------
Analyzer::Analyzer() : menu_(0), analyzer_widget_(0) {
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
		if(IThread::pointer thread = process->current_thread()) {
			State state;
			thread->get_state(&state);
		
			const edb::address_t address = state.instruction_pointer();
			if(IRegion::pointer region = edb::v1::memory_regions().find_region(address)) {
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
	if(IRegion::pointer region = edb::v1::memory_regions().find_region(address)) {
		qDebug("Added %s to the list of known functions",qPrintable(address.toPointerString()));
		specified_functions_.insert(address);
		invalidate_dynamic_analysis(region);
	}
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

	QMessageBox::information(
		0,
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

	QMessageBox::information(
		0,
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

	connect(action_find, SIGNAL(triggered()), this, SLOT(do_view_analysis()));
	connect(action_goto_function_start, SIGNAL(triggered()), this, SLOT(goto_function_start()));
	connect(action_goto_function_end, SIGNAL(triggered()),   this, SLOT(goto_function_end()));
	connect(action_mark_function_start, SIGNAL(triggered()), this, SLOT(mark_function_start()));
	ret << action_find << action_goto_function_start << action_goto_function_end << action_mark_function_start;

	return ret;
}

//------------------------------------------------------------------------------
// Name: do_analysis
// Desc:
//------------------------------------------------------------------------------
void Analyzer::do_analysis(const IRegion::pointer &region) {
	if(region && region->size() != 0) {
		QProgressDialog progress(tr("Performing Analysis"), 0, 0, 100, edb::v1::debugger_ui);
		connect(this, SIGNAL(update_progress(int)), &progress, SLOT(setValue(int)));
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
	const QList<Symbol::pointer> symbols = edb::v1::symbol_manager().symbols();

	for(const Symbol::pointer &sym: symbols) {
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

	for(const edb::address_t addr: specified_functions_) {
		if(data->region->contains(addr)) {
			qDebug("[Analyzer] adding user marked function: <%s>", qPrintable(addr.toPointerString()));
			data->known_functions.insert(addr);
		}
	}
}

//------------------------------------------------------------------------------
// Name: is_thunk
// Desc: basically returns true if the first instruction of the function is a
//       jmp
//------------------------------------------------------------------------------
bool Analyzer::is_thunk(edb::address_t address) const {

	quint8 buf[edb::Instruction::MAX_SIZE];
	if(const int buf_size = edb::v1::get_instruction_bytes(address, buf)) {
		const edb::Instruction inst(buf, buf + buf_size, address);
		return is_unconditional_jump(inst);
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: set_function_types_helper
// Desc:
//------------------------------------------------------------------------------
void Analyzer::set_function_types_helper(Function &function) const {

	if(is_thunk(function.entry_address())) {
		function.set_type(Function::FUNCTION_THUNK);
	} else {
		function.set_type(Function::FUNCTION_STANDARD);
	}
}

//------------------------------------------------------------------------------
// Name: set_function_types
// Desc:
//------------------------------------------------------------------------------
void Analyzer::set_function_types(FunctionMap *results) {

	Q_ASSERT(results);

	// give bonus if we have a symbol for the address
#if QT_VERSION >= 0x040800 && defined(QT_CONCURRENT_LIB)
	QtConcurrent::blockingMap(*results, [this](Function &function) {
		set_function_types_helper(function);
	});
#else
	std::for_each(results->begin(), results->end(), [this](Function &function) {
		set_function_types_helper(function);
	});
#endif
}

//------------------------------------------------------------------------------
// Name: ident_header
// Desc:
//------------------------------------------------------------------------------
void Analyzer::ident_header(Analyzer::RegionData *data) {
	Q_UNUSED(data);
}

//------------------------------------------------------------------------------
// Name: analyze
// Desc:
//------------------------------------------------------------------------------
void Analyzer::collect_functions(Analyzer::RegionData *data) {
	Q_ASSERT(data);

	// results
	QHash<edb::address_t, BasicBlock> basic_blocks;
	QHash<edb::address_t, Function>   functions;

	// push all known functions onto a stack
	QStack<edb::address_t> known_functions;
	for(const edb::address_t function: data->known_functions) {
		known_functions.push(function);
	}

	// push all fuzzy function too...
	for(const edb::address_t function: data->fuzzy_functions) {
		known_functions.push(function);
	}

	// process all functions that are known
	while(!known_functions.empty()) {
		const edb::address_t function_address = known_functions.pop();

		if(!functions.contains(function_address)) {

			QStack<edb::address_t> blocks;
			blocks.push(function_address);

			Function func(function_address);

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

						const edb::Instruction inst(buffer, buffer + buf_size, address);
						if(!inst) {
							break;
						}

						block.push_back(std::make_shared<edb::Instruction>(inst));

						if(is_call(inst)) {

							// note the destination and move on
							// we special case some simple things.
							// also this is an opportunity to find call tables.
							const edb::Operand &op = inst.operands()[0];
							if(op.general_type() == edb::Operand::TYPE_REL) {
								const edb::address_t ea = op.relative_target();

								// skip over ones which are: "call <label>; label:"
								if(ea != address + inst.size()) {
									known_functions.push(ea);
									
									if(!will_return(ea)) {
										break;
									}
								}
							} else if(op.general_type() == edb::Operand::TYPE_EXPRESSION) {
								// looks like: "call [...]", if it is of the form, call [C + REG]
								// then it may be a jump table using REG as an offset
							} else if(op.general_type() == edb::Operand::TYPE_REGISTER) {
								// looks like: "call <reg>", this is this may be a callback
								// if we can use analysis to determine that it's a constant
								// we can figure it out...
								// eventually, we should figure out the parameters of the function
								// to see if we can know what the target is
							}

							
						} else if(is_unconditional_jump(inst)) {

							Q_ASSERT(inst.operand_count() >= 1);
							const edb::Operand &op = inst.operands()[0];

							// TODO: we need some heuristic for detecting when this is
							//       a call/ret -> jmp optimization
							if(op.general_type() == edb::Operand::TYPE_REL) {
								const edb::address_t ea = op.relative_target();

								
								if(functions.contains(ea)) {
									functions[ea].add_reference();
								} else if((ea - function_address) > 0x2000u) {
									known_functions.push(ea);
								} else {
									blocks.push(ea);
								}
							}
							break;
						} else if(is_conditional_jump(inst)) {

							Q_ASSERT(inst.operand_count() == 1);
							const edb::Operand &op = inst.operands()[0];

							if(op.general_type() == edb::Operand::TYPE_REL) {
								blocks.push(op.relative_target());
								blocks.push(address + inst.size());
							}
							break;
						} else if(is_terminator(inst)) {
							break;
						}

						address += inst.size();
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

	qDebug() << "----------Basic Blocks----------";
	for(auto it = basic_blocks.begin(); it != basic_blocks.end(); ++it) {
		qDebug("%s:", qPrintable(it.key().toPointerString()));

		for(auto j = it.value().begin(); j != it.value().end(); ++j) {
			const instruction_pointer &inst = *j;
			qDebug("\t%s: %s", qPrintable(edb::address_t(inst->rva()).toPointerString()), edb::v1::formatter().to_string(*inst).c_str());
		}
	}
	qDebug() << "----------Basic Blocks----------";

	qSwap(data->basic_blocks, basic_blocks);
	qSwap(data->functions, functions);
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

		// fuzzy_functions, known_functions
		for(edb::address_t addr = data->region->start(); addr != data->region->end(); ++addr) {

			quint8 buf[edb::Instruction::MAX_SIZE];
			if(const int buf_size = edb::v1::get_instruction_bytes(addr, buf)) {
				const edb::Instruction inst(buf, buf + buf_size, addr);
				if(inst) {
					if(is_call(inst)) {

						// note the destination and move on
						// we special case some simple things.
						// also this is an opportunity to find call tables.
						const edb::Operand &op = inst.operands()[0];
						if(op.general_type() == edb::Operand::TYPE_REL) {
							const edb::address_t ea = op.relative_target();

							// skip over ones which are: "call <label>; label:"
							if(ea != addr + inst.size()) {

								if(!data->known_functions.contains(ea)) {
									fuzzy_functions[ea]++;
								}
							}
						}
					}
				}
			}
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
void Analyzer::analyze(const IRegion::pointer &region) {

	QTime t;
	t.start();

	RegionData &region_data = analysis_info_[region->start()];

	QSettings settings;
	const bool fuzzy          = settings.value("Analyzer/fuzzy_logic_functions.enabled", true).toBool();
	const QByteArray md5      = md5_region(region);
	const QByteArray prev_md5 = region_data.md5;

	if(md5 != prev_md5 || fuzzy != region_data.fuzzy) {

		region_data.basic_blocks.clear();
		region_data.functions.clear();
		region_data.fuzzy_functions.clear();
		region_data.known_functions.clear();

		region_data.region = region;
		region_data.md5    = md5;
		region_data.fuzzy  = fuzzy;

		const struct {
			const char             *message;
			std::function<void()> function;
		} analysis_steps[] = {
			{ "identifying executable headers...",                       std::bind(&Analyzer::ident_header,            this, &region_data) },
			{ "adding entry points to the list...",                      std::bind(&Analyzer::bonus_entry_point,       this, &region_data) },
			{ "attempting to add 'main' to the list...",                 std::bind(&Analyzer::bonus_main,              this, &region_data) },
			{ "attempting to add functions with symbols to the list...", std::bind(&Analyzer::bonus_symbols,           this, &region_data) },
			{ "attempting to add marked functions to the list...",       std::bind(&Analyzer::bonus_marked_functions,  this, &region_data) },
			{ "attempting to collect functions with fuzzy analysis...",  std::bind(&Analyzer::collect_fuzzy_functions, this, &region_data) },
			{ "collecting basic blocks...",                              std::bind(&Analyzer::collect_functions,       this, &region_data) },
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
IAnalyzer::FunctionMap Analyzer::functions(const IRegion::pointer &region) const {
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

	if(IRegion::pointer region = edb::v1::memory_regions().find_region(address)) {
		const FunctionMap &funcs = functions(region);
		for(const Function &f: funcs) {
			if(address >= f.entry_address() && address <= f.end_address()) {
				*function = f;
				return true;
			}
		}
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: md5_region
// Desc: returns a byte array representing the MD5 of a region
//------------------------------------------------------------------------------
QByteArray Analyzer::md5_region(const IRegion::pointer &region) const{

	const edb::address_t page_size = edb::v1::debugger_core->page_size();
	const size_t page_count        = region->size() / page_size;

	const QVector<quint8> pages = edb::v1::read_pages(region->start(), page_count);
	if(!pages.isEmpty()) {
		return edb::v1::get_md5(pages);
	}


	return QByteArray();
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
void Analyzer::invalidate_analysis(const IRegion::pointer &region) {
	invalidate_dynamic_analysis(region);
	for(const edb::address_t addr: specified_functions_) {
		if(addr >= region->start() && addr < region->end()) {
			specified_functions_.remove(addr);
		}
	}
}

//------------------------------------------------------------------------------
// Name: invalidate_dynamic_analysis
// Desc:
//------------------------------------------------------------------------------
void Analyzer::invalidate_dynamic_analysis(const IRegion::pointer &region) {

	RegionData info;
	info.region = region;

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
edb::address_t Analyzer::find_containing_function(edb::address_t address, bool *ok) const {
	Q_ASSERT(ok);

	Function function;
	*ok = find_containing_function(address, &function);
	if(*ok) {
		return function.entry_address();
	} else {
		return 0;
	}
}

//------------------------------------------------------------------------------
// Name: will_return
// Desc:
//------------------------------------------------------------------------------
bool Analyzer::will_return(edb::address_t address) const {


	const QList<Symbol::pointer> symbols = edb::v1::symbol_manager().symbols();
	for(const Symbol::pointer &symbol: symbols) {
		if(symbol->address == address) {
			const QString symname = symbol->name_no_prefix;
			const QString func_name = symname.mid(0, symname.indexOf("@"));

			if(func_name == "__assert_fail" || func_name == "abort" || func_name == "_exit" || func_name == "_Exit") {
				return false;
			}

#ifdef Q_OS_LINUX
			if(func_name == "_Unwind_Resume") {
				return false;
			}
#endif
		}
	}


	return true;
}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(Analyzer, Analyzer)
#endif

}

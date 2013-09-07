/*
Copyright (C) 2006 - 2013 Evan Teran
                          eteran@alum.rit.edu

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
#include "IDebuggerCore.h"
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

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <cstring>

#if QT_VERSION >= 0x050000
#include <QtConcurrent>
#elif QT_VERSION >= 0x040800
#include <QtConcurrentMap>
#endif

#define MIN_REFCOUNT 2
//#define OLD_ANALYSIS

namespace {
#if defined(EDB_X86)
	const edb::Operand::Register STACK_REG = edb::Operand::REG_ESP;
	const edb::Operand::Register FRAME_REG = edb::Operand::REG_EBP;
#elif defined(EDB_X86_64)
	const edb::Operand::Register STACK_REG = edb::Operand::REG_RSP;
	const edb::Operand::Register FRAME_REG = edb::Operand::REG_RBP;
#endif

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
	return new analyzer::OptionsPage;
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
#if defined(EDB_X86)
		menu_->addAction(tr("&Analyze EIP's Region"), this, SLOT(do_ip_analysis()), QKeySequence(tr("Ctrl+A")));
#elif defined(EDB_X86_64)
		menu_->addAction(tr("&Analyze RIP's Region"), this, SLOT(do_ip_analysis()), QKeySequence(tr("Ctrl+A")));
#endif
		menu_->addAction(tr("&Analyze Viewed Region"), this, SLOT(do_view_analysis()), QKeySequence(tr("Ctrl+Shift+A")));

		// if we are dealing with a main window (and we are...)
		// add the dock object
		if(QMainWindow *const main_window = qobject_cast<QMainWindow *>(edb::v1::debugger_ui)) {
			analyzer_widget_ = new AnalyzerWidget;

			// make the toolbar widget and _name_ it, it is important to name it so
			// that it's state is saved in the GUI info
			QToolBar *const toolbar = new QToolBar(tr("Region Analysis"), main_window);
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
	static QDialog *dialog = new SpecifiedFunctions(edb::v1::debugger_ui);
	dialog->show();
}

//------------------------------------------------------------------------------
// Name: do_ip_analysis
// Desc:
//------------------------------------------------------------------------------
void Analyzer::do_ip_analysis() {
	State state;
	edb::v1::debugger_core->get_state(&state);

	const edb::address_t address = state.instruction_pointer();
	if(IRegion::pointer region = edb::v1::memory_regions().find_region(address)) {
		do_analysis(region);
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
		qDebug("Added %p to the list of known functions", reinterpret_cast<void *>(address));
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
		edb::v1::jump_to_address(function.entry_address);
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
		edb::v1::jump_to_address(function.last_instruction);
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

	QAction *const action_find                = new QAction(tr("Analyze Here"), this);
	QAction *const action_goto_function_start = new QAction(tr("Goto Function Start"), this);
	QAction *const action_goto_function_end   = new QAction(tr("Goto Function End"), this);
	QAction *const action_mark_function_start = new QAction(tr("Mark As Function Start"), this);

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
	if(region->size() != 0) {
		QProgressDialog progress(tr("Performing Analysis"), 0, 0, 100, edb::v1::debugger_ui);
		connect(this, SIGNAL(update_progress(int)), &progress, SLOT(setValue(int)));
		progress.show();
		progress.setValue(0);
		analyze(region);
		edb::v1::repaint_cpu_view();
	}
}

//------------------------------------------------------------------------------
// Name: find_function_calls
// Desc:
//------------------------------------------------------------------------------
void Analyzer::find_function_calls(const IRegion::pointer &region, FunctionMap *found_functions) {

	Q_ASSERT(found_functions);

	const edb::address_t page_size = edb::v1::debugger_core->page_size();
	const size_t page_count        = region->size() / page_size;
	const QVector<quint8> pages    = edb::v1::read_pages(region->start(), page_count);
	
	if(!pages.isEmpty()) {
		for(int i = 0; i < pages.size(); ++i) {
		
			const quint8 *p = &pages[0];
			const quint8 *const pages_end = &pages[0] + pages.size();
		
			const edb::Instruction insn(p, pages_end, region->start() + i, std::nothrow);

			if(is_call(insn)) {

				const edb::address_t ip = region->start() + i;
				const edb::Operand &op = insn.operands()[0];

				if(op.general_type() == edb::Operand::TYPE_REL) {
					const edb::address_t ea = op.relative_target();

					// skip over ones which are : call <label>; label:
					if(ea != ip + insn.size()) {
						if(region->contains(ea)) {
							// avoid calls which land in the middle of a function...
							// this may or may not be the best approach
							if(!is_inside_known(region, ea)) {
								(*found_functions)[ea].entry_address = ea;
								(*found_functions)[ea].end_address   = ea;
								(*found_functions)[ea].reference_count++;
							}
						}
					}
				}

				emit update_progress(util::percentage(6, 10, i, region->size()));
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: is_stack_frame
// Desc:
//------------------------------------------------------------------------------
bool Analyzer::is_stack_frame(edb::address_t addr) const {

	quint8 buf[edb::Instruction::MAX_SIZE];

	unsigned int i = 0;

	while(i < 2) {
		// gets the bytes for the instruction
		int buf_size = sizeof(buf);
		if(!edb::v1::get_instruction_bytes(addr, buf, &buf_size)) {
			return false;
		}

		// decode it
		const edb::Instruction insn(buf, buf + buf_size, addr, std::nothrow);
		if(!insn) {
			return false;
		}

		// which part of the sequence are we looking at?
		switch(i) {
		case 0:
			switch(insn.type()) {
			case edb::Instruction::OP_PUSH:
				{
					// are we looking at a 'push rBP' ?
					Q_ASSERT(insn.operand_count() == 1);
					const edb::Operand &op = insn.operands()[0];
					if(op.complete_type() == edb::Operand::TYPE_REGISTER) {
						if(op.reg() == FRAME_REG) {
							return false;
						}
					}
				}
				break;
				
			case edb::Instruction::OP_ENTER:
				{
					// if it is an 'enter 0,0' instruction, then it's a stack frame right away
					Q_ASSERT(insn.operand_count() == 2);
					const edb::Operand &op0 = insn.operands()[0];
					const edb::Operand &op1 = insn.operands()[1];
					if(op0.immediate() == 0 && op1.immediate() == 0) {
						return true;
					}
				}
				break;
				
			default:
				return false;
			}
			break;
		case 1:
			// are we looking at a 'mov rBP, rSP' ?
			if(insn.type() == edb::Instruction::OP_MOV) {
				Q_ASSERT(insn.operand_count() == 2);
				const edb::Operand &op0 = insn.operands()[0];
				const edb::Operand &op1 = insn.operands()[1];
				if(op0.complete_type() == edb::Operand::TYPE_REGISTER && op1.complete_type() == edb::Operand::TYPE_REGISTER) {
					if(op0.reg() == FRAME_REG && op1.reg() == STACK_REG) {
						return true;
					}
				}
			}
			break;
		}

		addr += insn.size();
		++i;
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: bonus_stack_frames_helper
// Desc:
//------------------------------------------------------------------------------
void Analyzer::bonus_stack_frames_helper(Function &info) const {

	if(is_stack_frame(info.entry_address)) {
		info.reference_count++;
	}
}

//------------------------------------------------------------------------------
// Name: bonus_stack_frames
// Desc: give bonus if we see a "push ebp; mov ebp, esp;"
//------------------------------------------------------------------------------
void Analyzer::bonus_stack_frames(RegionData *data) {

	Q_ASSERT(data);

#if QT_VERSION >= 0x040800
	QtConcurrent::blockingMap(
		data->analysis,
		boost::bind(&Analyzer::bonus_stack_frames_helper, this, _1));
#else
	std::for_each(
		data->analysis.begin(),
		data->analysis.end(),
		boost::bind(&Analyzer::bonus_stack_frames_helper, this, _1));
#endif
}

//------------------------------------------------------------------------------
// Name: bonus_main
// Desc:
//------------------------------------------------------------------------------
void Analyzer::bonus_main(RegionData *data) const {

	Q_ASSERT(data);

	const QString s = edb::v1::debugger_core->process_exe(edb::v1::debugger_core->pid());
	if(!s.isEmpty()) {
		const edb::address_t main = edb::v1::locate_main_function();

		if(main && data->region->contains(main)) {
			data->known_functions.insert(main);
		}
	}
}

//------------------------------------------------------------------------------
// Name: bonus_symbols_helper
// Desc:
//------------------------------------------------------------------------------
void Analyzer::bonus_symbols_helper(RegionData *data, const Symbol::pointer &sym) {

	Q_ASSERT(data);

	const edb::address_t addr = sym->address;

	if(data->region->contains(addr) && sym->is_code()) {
		qDebug("[Analyzer] adding: %s <%p>", qPrintable(sym->name), reinterpret_cast<void *>(addr));
		data->known_functions.insert(addr);
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

	std::for_each(
		symbols.begin(),
		symbols.end(),
		boost::bind(&Analyzer::bonus_symbols_helper, this, data, _1));
}

//------------------------------------------------------------------------------
// Name: bonus_marked_functions
// Desc:
//------------------------------------------------------------------------------
void Analyzer::bonus_marked_functions(RegionData *data) {

	Q_ASSERT(data);

	Q_FOREACH(edb::address_t addr, specified_functions_) {
		if(data->region->contains(addr)) {
			qDebug("[Analyzer] adding: <%p>", reinterpret_cast<void *>(addr));
			data->known_functions.insert(addr);
		}
	}
}

//------------------------------------------------------------------------------
// Name: fix_overlaps
// Desc: ensures that no function overlaps another
//------------------------------------------------------------------------------
void Analyzer::fix_overlaps(FunctionMap *function_map) {

	Q_ASSERT(function_map);

	for(FunctionMap::iterator it = function_map->begin(); it != function_map->end(); ) {
		Function &func = *it++;
		if(it != function_map->end()) {
			const Function &next_func = *it;
			if(next_func.entry_address <= func.end_address) {
				func.end_address = next_func.entry_address - 1;
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: find_function_end
// Desc:
//------------------------------------------------------------------------------
void Analyzer::find_function_end(Function *function, edb::address_t end_address, QSet<edb::address_t> *found_functions, const FunctionMap &results) {

	Q_ASSERT(function);
	Q_ASSERT(found_functions);

	QStack<edb::address_t>     jump_targets;
	QHash<edb::address_t, int> visited_addresses;

	// we start with the entry point of the function
	jump_targets.push(function->entry_address);

	// while no more jump targets... (includes entry point)
	while(!jump_targets.empty()) {

		edb::address_t addr = jump_targets.pop();

		// for certain forward jump scenarioes this is possible.
		if(visited_addresses.contains(addr)) {
			continue;
		}

		// keep going until we go out of bounds
		while(addr >= function->entry_address && addr < end_address) {

			quint8 buf[edb::Instruction::MAX_SIZE];
			int buf_size = sizeof(buf);
			if(!edb::v1::get_instruction_bytes(addr, buf, &buf_size)) {
				break;
			}

			// an invalid instruction ends this "block"
			const edb::Instruction insn(buf, buf + buf_size, addr, std::nothrow);
			if(!insn) {
				break;
			}

			// ok, it was a valid instruction, let's add it to the
			// list of possible 'code addresses'
			visited_addresses.insert(addr, insn.size());

			if(is_ret(insn) || insn.type() == edb::Instruction::OP_HLT) {
				// instructions that clearly terminate the current block...
				break;

			} else if(is_conditional_jump(insn)) {

				// note if neccessary the Jcc target and move on, yes this can be fooled by "conditional"
				// jumps which are always true or false, not much we can do about it at this level.
				const edb::Operand &op = insn.operands()[0];
				if(op.general_type() == edb::Operand::TYPE_REL) {
					const edb::address_t ea = op.relative_target();

					if(!visited_addresses.contains(ea) && !jump_targets.contains(ea)) {
						jump_targets.push(ea);
					}
				}
			} else if(is_call(insn)) {

				// similar to above, note the destination and move on
				// we special case simple things for speed.
				// also this is an opportunity to find call tables.
				const edb::Operand &op = insn.operands()[0];
				if(op.general_type() == edb::Operand::TYPE_REL) {
					const edb::address_t ea = op.relative_target();

					// skip over ones which are: "call <label>; label:"
					if(ea != addr + insn.size()) {
						found_functions->insert(ea);
					}
				} else if(op.general_type() == edb::Operand::TYPE_EXPRESSION) {
					// looks like: "call [...]", if it is of the form, call [C + REG]
					// then it may be a jump table using REG as an offset
				}
			} else if(is_unconditional_jump(insn)) {

				const edb::Operand &op = insn.operands()[0];
				if(op.general_type() == edb::Operand::TYPE_REL) {
					const edb::address_t ea = op.relative_target();


					// an absolute jump within this function
					if(ea >= function->entry_address && ea < addr) {
					
						if(ea <= addr) {
							break;
						}
					
						addr += insn.size();
						continue;
					}


					// is it a jump to another function's entry point?
					// if so, this is a dead end, resume from other branches
					// but give the target a bonus reference
					FunctionMap::const_iterator it = results.find(ea);
					if(it != results.end()) {
						found_functions->insert(ea);
						break;
					}

#if 0
					// ok, it is a jmp to an address we haven't seen yet
					// so just continue processing from there
					if(!visited_addresses.contains(ea)) {
						addr = ea;
						continue;
					} else {
						break;
					}
#else
					break;
#endif
				}
			}

			addr += insn.size();
		}
	}

	function->last_instruction = function->entry_address;
	function->end_address      = function->entry_address;

	// get the last instruction and the last byte of the function
	for(QHash<edb::address_t, int>::const_iterator it = visited_addresses.begin(); it != visited_addresses.end(); ++it) {
		function->end_address      = qMax(function->end_address,      it.key() + it.value() - 1);
		function->last_instruction = qMax(function->last_instruction, it.key());
	}
}

//------------------------------------------------------------------------------
// Name: is_thunk
// Desc: basically returns true if the first instruction of the function is a
//       jmp
//------------------------------------------------------------------------------
bool Analyzer::is_thunk(edb::address_t address) const {
	quint8 buf[edb::Instruction::MAX_SIZE];
	int buf_size = sizeof(buf);
	if(edb::v1::get_instruction_bytes(address, buf, &buf_size)) {
		const edb::Instruction insn(buf, buf + buf_size, address, std::nothrow);
		return is_unconditional_jump(insn);
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: set_function_types_helper
// Desc:
//------------------------------------------------------------------------------
void Analyzer::set_function_types_helper(Function &info) const {

	if(is_thunk(info.entry_address)) {
		info.type = Function::FUNCTION_THUNK;
	} else {
		info.type = Function::FUNCTION_STANDARD;
	}
}

//------------------------------------------------------------------------------
// Name: set_function_types
// Desc:
//------------------------------------------------------------------------------
void Analyzer::set_function_types(FunctionMap *results) {

	Q_ASSERT(results);

	// give bonus if we have a symbol for the address
#if QT_VERSION >= 0x040800
	QtConcurrent::blockingMap(
		*results,
		boost::bind(&Analyzer::set_function_types_helper, this, _1));
#else
	std::for_each(
		results->begin(),
		results->end(),
		boost::bind(&Analyzer::set_function_types_helper, this, _1));
#endif
}

//------------------------------------------------------------------------------
// Name: is_inside_known
// Desc:
//------------------------------------------------------------------------------
bool Analyzer::is_inside_known(const IRegion::pointer &region, edb::address_t address) {

	const FunctionMap &funcs = functions(region);
	Q_FOREACH(const Function &func, funcs) {
		if(address >= func.entry_address && address <= func.end_address) {
			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: ident_header
// Desc:
//------------------------------------------------------------------------------
void Analyzer::ident_header() {

}

//------------------------------------------------------------------------------
// Name: analyze
// Desc:
//------------------------------------------------------------------------------
void Analyzer::collect_basic_blocks(Analyzer::RegionData *data) {
	Q_ASSERT(data);
	
	QHash<edb::address_t, BasicBlock> &basic_blocks = data->basic_blocks;
	QStack<edb::address_t> jump_targets;
	
	Q_FOREACH(edb::address_t function, data->known_functions) {	
		jump_targets.push(function);
	}
	
	while(!jump_targets.empty()) {
		edb::address_t block_start = jump_targets.pop();
		edb::address_t address     = block_start;
		BasicBlock     block;
		
		if(!basic_blocks.contains(block_start)) {
			while(data->region->contains(address)) {

				quint8 buffer[edb::Instruction::MAX_SIZE];
				int buf_size = sizeof(buffer);
				if(!edb::v1::get_instruction_bytes(address, buffer, &buf_size)) {
					break;
				}

				const edb::Instruction insn(buffer, buffer + buf_size, address, std::nothrow);
				if(!insn) {
					break;
				}
				
				block.push_back(instruction_pointer(new edb::Instruction(insn)));

				if(is_call(insn)) {
				
					// note the destination and move on
					// we special case simple things for speed.
					// also this is an opportunity to find call tables.
					const edb::Operand &op = insn.operands()[0];
					if(op.general_type() == edb::Operand::TYPE_REL) {
						const edb::address_t ea = op.relative_target();

						// skip over ones which are: "call <label>; label:"
						if(ea != address + insn.size()) {
							data->known_functions.insert(ea);
							jump_targets.push(ea);
						}
					} else if(op.general_type() == edb::Operand::TYPE_EXPRESSION) {
						// looks like: "call [...]", if it is of the form, call [C + REG]
						// then it may be a jump table using REG as an offset
					} else if(op.general_type() == edb::Operand::TYPE_REGISTER) {
						// looks like: "call <reg>", this is this may be a callback
						// if we can use analysis to determine that it's a constant
						// we can figure it out...
					}
				}

				if(is_unconditional_jump(insn)) {

					Q_ASSERT(insn.operand_count() == 1);
					const edb::Operand &op = insn.operands()[0];

					if(op.general_type() == edb::Operand::TYPE_REL) {
						jump_targets.push(op.relative_target());
					}
					break;
				}
				
				if(is_conditional_jump(insn)) {

					Q_ASSERT(insn.operand_count() == 1);
					const edb::Operand &op = insn.operands()[0];

					if(op.general_type() == edb::Operand::TYPE_REL) {
						jump_targets.push(op.relative_target());
						jump_targets.push(address + insn.size());
					}
					break;
				}
				
				if(is_ret(insn) || insn.type() == edb::Instruction::OP_HLT) {
					break;
				}

				address += insn.size();
			}

			basic_blocks[block_start] = block;
		}
	}
	
	qDebug() << "----------Basic Blocks----------";
	for(QHash<edb::address_t, BasicBlock>::iterator it = basic_blocks.begin(); it != basic_blocks.end(); ++it) {
		qDebug("%p:", reinterpret_cast<void *>(it.key()));
		
		for(BasicBlock::const_iterator j = it.value().begin(); j != it.value().end(); ++j) {
			const instruction_pointer &insn = *j;
			qDebug("\t%p: %s", reinterpret_cast<void *>(insn->rva()), to_string(*insn).c_str());
		}
	}
	qDebug() << "----------Basic Blocks----------";
}

//------------------------------------------------------------------------------
// Name: collect_function_blocks
// Desc:
//------------------------------------------------------------------------------
void Analyzer::collect_function_blocks(RegionData *data, edb::address_t address) {
	qDebug("Analyzing basic blocks of: %p", reinterpret_cast<void *>(address));

	QStack<edb::address_t> block_addresses;
	QSet<edb::address_t>   processed_addresses;

	block_addresses.push(address);
	while(!block_addresses.empty()) {
		const edb::address_t current_address = block_addresses.pop();
		processed_addresses.insert(current_address);

		QHash<edb::address_t, BasicBlock>::const_iterator it = data->basic_blocks.find(current_address);
		if(it != data->basic_blocks.end()) {
			const BasicBlock &basic_block = it.value();
			if(!basic_block.empty()) {
				const instruction_pointer &last_instruction = basic_block.back();

				if(is_conditional_jump(*last_instruction)) {
					// add both the jump target and the instructions which follow this block
					const edb::address_t next_block = current_address + basic_block.byte_size();
					if(!processed_addresses.contains(next_block)) {
						block_addresses.push(next_block);
					}

					const edb::Operand &op = last_instruction->operands()[0];
					if(op.general_type() == edb::Operand::TYPE_REL) {
						const edb::address_t target = op.relative_target();
						if(!processed_addresses.contains(target)) {
							block_addresses.push(target);
						}
					}
				} else if(is_unconditional_jump(*last_instruction)) {
					const edb::Operand &op = last_instruction->operands()[0];
					if(op.general_type() == edb::Operand::TYPE_REL) {
						const edb::address_t target = op.relative_target();

						// a JMP <func> is simply an optimization for "CALL <func>; RET"
						if(data->known_functions.contains(target)) {
							//break;						
						} else if(target <= address) {
							//break;
						} else {
							if(!processed_addresses.contains(target)) {
								block_addresses.push(target);
							}
						}
					}
				}
			}
		}
	}
	
	if(!processed_addresses.isEmpty()) {
		QList<edb::address_t> block_list = processed_addresses.toList();
		qSort(block_list);
		const edb::address_t last_block = block_list.back();
		
		QHash<edb::address_t, BasicBlock>::const_iterator it = data->basic_blocks.find(last_block);
		if(it != data->basic_blocks.end()) {
			
			const BasicBlock &basic_block = it.value();
						
			if(!basic_block.empty()) {
			
				Function func;
				func.entry_address      = address;
				func.reference_count    = MIN_REFCOUNT;
				func.end_address        = basic_block.last_address() - 1;
				func.last_instruction   = basic_block.back()->rva();
				data->analysis[address] = func;
			}	
		}
	}
}

//------------------------------------------------------------------------------
// Name: collect_known_functions
// Desc:
//------------------------------------------------------------------------------
void Analyzer::collect_known_functions(RegionData *data) {
	
	// can't use concurrent (yet) because it creates new entries in 
	// data->analysis
	Q_FOREACH(const edb::address_t address, data->known_functions) {
		collect_function_blocks(data, address);
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
		
		region_data.analysis.clear();
		region_data.basic_blocks.clear();
		region_data.known_functions.clear();
		
		region_data.region = region;
		region_data.md5    = md5;
		region_data.fuzzy  = fuzzy;

		QSet<edb::address_t> walked_functions;
		FunctionMap          found_functions;

		const struct {
			const char             *message;
			boost::function<void()> function;
		} analysis_steps[] = {
			{ "identifying executable headers...",                       boost::bind(&Analyzer::ident_header,            this) },
			{ "adding entry points to the list...",                      boost::bind(&Analyzer::bonus_entry_point,       this, &region_data) },
			{ "attempting to add 'main' to the list...",                 boost::bind(&Analyzer::bonus_main,              this, &region_data) },
			{ "attempting to add functions with symbols to the list...", boost::bind(&Analyzer::bonus_symbols,           this, &region_data) },
			{ "attempting to add marked functions to the list...",       boost::bind(&Analyzer::bonus_marked_functions,  this, &region_data) },			
			{ "collecting basic blocks...",                              boost::bind(&Analyzer::collect_basic_blocks,    this, &region_data) },
			{ "collecting known functions...",                           boost::bind(&Analyzer::collect_known_functions, this, &region_data) },
		};

		const struct {
			const char             *message;
			boost::function<void()> function;
		} fuzzy_analysis_steps[] = {
#if defined(OLD_ANALYSIS)
			{ "finding possible function calls...",      boost::bind(&Analyzer::find_function_calls,      this, boost::cref(region), &found_functions) },
			{ "bonusing stack frames...",                boost::bind(&Analyzer::bonus_stack_frames,       this, &region_data) },
#endif
		};

		const int analysis_steps_count       = sizeof(analysis_steps) / sizeof(analysis_steps[0]);
		const int fuzzy_analysis_steps_count = sizeof(fuzzy_analysis_steps) / sizeof(fuzzy_analysis_steps[0]);
		const int total_steps = analysis_steps_count + fuzzy_analysis_steps_count + 1;

		emit update_progress(util::percentage(0, total_steps));
		for(int i = 0; i < analysis_steps_count; ++i) {
			qDebug("[Analyzer] %s", analysis_steps[i].message);
			analysis_steps[i].function();
			emit update_progress(util::percentage(i + 1, total_steps));
		}

		fix_overlaps(&region_data.analysis);

		// ok, at this point, we've done the best we can with knowns
		// we should have a full analysis of all functions which are
		// reachable from known entry points in the code
		// let's start looking for unknowns
		if(fuzzy) {
			for(int i = 0; i < fuzzy_analysis_steps_count; ++i) {
				qDebug("[Analyzer] %s", fuzzy_analysis_steps[i].message);
				fuzzy_analysis_steps[i].function();
				emit update_progress(util::percentage(analysis_steps_count + i + 1, total_steps));
			}
		}

		qDebug("[Analyzer] determining function types...");
		set_function_types(&region_data.analysis);

		qDebug("[Analyzer] complete");
		emit update_progress(100);

		if(analyzer_widget_) {
			analyzer_widget_->repaint();
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
		if(address == func.entry_address) {
			return ADDRESS_FUNC_START;
		} else if(address == func.end_address) {
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
	return analysis_info_[region->start()].analysis;
}

//------------------------------------------------------------------------------
// Name: find_containing_function
// Desc:
//------------------------------------------------------------------------------
bool Analyzer::find_containing_function(edb::address_t address, Function *function) const {

	Q_ASSERT(function);

	if(IRegion::pointer region = edb::v1::memory_regions().find_region(address)) {
		const FunctionMap &funcs = functions(region);
		Q_FOREACH(const Function &f, funcs) {
			if(address >= f.entry_address && address <= f.end_address) {
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
// Name: module_entry_point
// Desc:
//------------------------------------------------------------------------------
edb::address_t Analyzer::module_entry_point(const IRegion::pointer &region) const {

	edb::address_t entry = 0;
	if(IBinary *const binary_info = edb::v1::get_binary_info(region)) {
		entry = binary_info->entry_point();
		delete binary_info;
	}

	return entry;
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

		qDebug("[Analyzer] found entry point: %p", reinterpret_cast<void*>(entry));

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
	Q_FOREACH(edb::address_t addr, specified_functions_) {
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
		return function.entry_address;
	} else {
		return 0;
	}
}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(Analyzer, Analyzer)
#endif

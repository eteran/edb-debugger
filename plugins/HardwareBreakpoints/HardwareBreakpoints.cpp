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

#include "HardwareBreakpoints.h"
#include "edb.h"
#include "IDebugger.h"
#include "DialogHWBreakpoints.h"
#include "State.h"

#include <QMenu>
#include <QDialog>
#include <QMessageBox>
#include <QtDebug>

#include "ui_DialogHWBreakpoints.h"

// TODO: at the moment, nearly this entire file is x86/x86-64 specific
//       we need to figure out a proper way to support (if at all) non
//       x86 arches

#if !(defined(EDB_X86_64) || defined(EDB_X86))
#error "Unsupported Platform"
#endif

namespace HardwareBreakpoints {

//------------------------------------------------------------------------------
// Name: HardwareBreakpoints
// Desc:
//------------------------------------------------------------------------------
HardwareBreakpoints::HardwareBreakpoints() : menu_(0), dialog_(0), old_event_handler_(0) {
	auto dialog = new DialogHWBreakpoints(edb::v1::debugger_ui);
	dialog_ = dialog;

	// indexed access to members for simplicity later
	enabled_[Register1] = dialog->ui->chkBP1;
	enabled_[Register2] = dialog->ui->chkBP2;
	enabled_[Register3] = dialog->ui->chkBP3;
	enabled_[Register4] = dialog->ui->chkBP4;

	types_[Register1] = dialog->ui->cmbType1;
	types_[Register2] = dialog->ui->cmbType2;
	types_[Register3] = dialog->ui->cmbType3;
	types_[Register4] = dialog->ui->cmbType4;

	sizes_[Register1] = dialog->ui->cmbSize1;
	sizes_[Register2] = dialog->ui->cmbSize2;
	sizes_[Register3] = dialog->ui->cmbSize3;
	sizes_[Register4] = dialog->ui->cmbSize4;

	addresses_[Register1] = dialog->ui->txtBP1;
	addresses_[Register2] = dialog->ui->txtBP2;
	addresses_[Register3] = dialog->ui->txtBP3;
	addresses_[Register4] = dialog->ui->txtBP4;
}

//------------------------------------------------------------------------------
// Name: menu
// Desc:
//------------------------------------------------------------------------------
QMenu *HardwareBreakpoints::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if(!menu_) {
		menu_ = new QMenu(tr("Hardware BreakpointManager"), parent);
		menu_->addAction(tr("&Hardware Breakpoints"), this, SLOT(show_menu()), QKeySequence(tr("Ctrl+Shift+H")));
	}

	return menu_;
}

//------------------------------------------------------------------------------
// Name: setupBreakpoints
// Desc:
//------------------------------------------------------------------------------
void HardwareBreakpoints::setupBreakpoints() {

	// TODO: assert that we are paused

	if(IProcess *process = edb::v1::debugger_core->process()) {

		const bool enabled =
			enabled_[Register1]->isChecked() ||
			enabled_[Register2]->isChecked() ||
			enabled_[Register3]->isChecked() ||
			enabled_[Register4]->isChecked();

		if(enabled) {

			edb::address_t addr[RegisterCount];
			bool ok[RegisterCount];


			// evaluate all the expressions
			for(int i = 0; i < RegisterCount; ++i) {
				ok[i] = enabled_[i]->isChecked() && edb::v1::eval_expression(addresses_[i]->text(), &addr[i]);
			}

			if(!ok[Register1] && !ok[Register2] && !ok[Register3] && !ok[Register4]) {
				QMessageBox::information(
					0,
					tr("Address Error"),
					tr("An address expression provided does not appear to be valid"));
				return;
			}

			for(int i = 0; i < RegisterCount; ++i) {
				if(ok[i]) {
				
					const BreakpointStatus status = validateBreakpoint({
						enabled_[i]->isChecked(), 
						addr[i], 
						types_[i]->currentIndex(), 
						sizes_[i]->currentIndex()
					});
					
					switch(status) {
					case AlignmentError:
						QMessageBox::information(
							0,
							tr("Address Alignment Error"),
							tr("Hardware read/write breakpoints must be aligned to there address."));					
						return;
					case SizeError:
					QMessageBox::information(
						0,
						tr("BP Size Error"),
						tr("Hardware read/write breakpoints cannot be 8-bytes in a 32-bit debuggee."));					
						return;
					case Valid:
						break;
					}
				}
			}
			
			// we want to be enabled, if we aren't already hooked,
			// hook it
			if(!old_event_handler_) {
				old_event_handler_ = edb::v1::set_debug_event_handler(this);
			}

			for(IThread::pointer thread : process->threads()) {
				State state;
				thread->get_state(&state);


				for(int i = 0; i < RegisterCount; ++i) {
					if(ok[i]) {
						setBreakpointState(
							&state, 
							i, 
							{
								enabled_[i]->isChecked(), 
								addr[i], 
								types_[i]->currentIndex(), 
								sizes_[i]->currentIndex()
							}
							);
					}
				}

				thread->set_state(state);
			}

		} else {

			for(IThread::pointer thread : process->threads()) {
				State state;
				thread->get_state(&state);
				state.set_debug_register(7, 0);
				thread->set_state(state);
			}

			// we want to be disabled and we have hooked, so unhook
			if(old_event_handler_) {
				edb::v1::set_debug_event_handler(old_event_handler_);
				old_event_handler_ = 0;
			}
		}
	}

	edb::v1::update_ui();

}

//------------------------------------------------------------------------------
// Name: show_menu
// Desc:
//------------------------------------------------------------------------------
void HardwareBreakpoints::show_menu() {

	if(dialog_->exec() == QDialog::Accepted) {
		setupBreakpoints();
	}
}

//------------------------------------------------------------------------------
// Name: handle_event
// Desc: this hooks the debug event handler so we can make the breakpoints
//       able to be resumed
//------------------------------------------------------------------------------
edb::EVENT_STATUS HardwareBreakpoints::handle_event(const IDebugEvent::const_pointer &event) {

	if(event->stopped() && event->is_trap()) {

		if(IProcess *process = edb::v1::debugger_core->process()) {
			if(IThread::pointer thread = process->current_thread()) {
				// check DR6 to see if it was a HW BP event
				// if so, set the resume flag
				State state;
				thread->get_state(&state);
				if((state.debug_register(6) & 0x0f) != 0x00) {
					state.set_flags(state.flags() | (1 << 16));
					thread->set_state(state);
				}
			}
		}
	}

	// pass the event down the stack
	return old_event_handler_->handle_event(event);
}

//------------------------------------------------------------------------------
// Name: stack_context_menu
// Desc:
//------------------------------------------------------------------------------
QList<QAction *> HardwareBreakpoints::stack_context_menu() {
	auto menu = new QMenu(tr("Hardware Breakpoints"));

	auto rw1 = menu->addAction(tr("Hardware, On Read/Write #1"), this, SLOT(set_access1()));
	auto rw2 = menu->addAction(tr("Hardware, On Read/Write #2"), this, SLOT(set_access2()));
	auto rw3 = menu->addAction(tr("Hardware, On Read/Write #3"), this, SLOT(set_access3()));
	auto rw4 = menu->addAction(tr("Hardware, On Read/Write #4"), this, SLOT(set_access4()));

	auto wo1 = menu->addAction(tr("Hardware, On Write #1"), this, SLOT(set_write1()));
	auto wo2 = menu->addAction(tr("Hardware, On Write #2"), this, SLOT(set_write2()));
	auto wo3 = menu->addAction(tr("Hardware, On Write #3"), this, SLOT(set_write3()));
	auto wo4 = menu->addAction(tr("Hardware, On Write #4"), this, SLOT(set_write4()));

	rw1->setData(1);
	rw2->setData(1);
	rw3->setData(1);
	rw4->setData(1);

	wo1->setData(1);
	wo2->setData(1);
	wo3->setData(1);
	wo4->setData(1);

	QList<QAction *> ret;

	auto action = new QAction(tr("Hardware Breakpoints"), this);
	action->setMenu(menu);
	ret << action;
	return ret;
}

//------------------------------------------------------------------------------
// Name: data_context_menu
// Desc:
//------------------------------------------------------------------------------
QList<QAction *> HardwareBreakpoints::data_context_menu() {
	auto menu = new QMenu(tr("Hardware Breakpoints"));

	auto rw1 = menu->addAction(tr("Hardware, On Read/Write #1"), this, SLOT(set_access1()));
	auto rw2 = menu->addAction(tr("Hardware, On Read/Write #2"), this, SLOT(set_access2()));
	auto rw3 = menu->addAction(tr("Hardware, On Read/Write #3"), this, SLOT(set_access3()));
	auto rw4 = menu->addAction(tr("Hardware, On Read/Write #4"), this, SLOT(set_access4()));

	auto wo1 = menu->addAction(tr("Hardware, On Write #1"), this, SLOT(set_write1()));
	auto wo2 = menu->addAction(tr("Hardware, On Write #2"), this, SLOT(set_write2()));
	auto wo3 = menu->addAction(tr("Hardware, On Write #3"), this, SLOT(set_write3()));
	auto wo4 = menu->addAction(tr("Hardware, On Write #4"), this, SLOT(set_write4()));

	rw1->setData(2);
	rw2->setData(2);
	rw3->setData(2);
	rw4->setData(2);

	wo1->setData(2);
	wo2->setData(2);
	wo3->setData(2);
	wo4->setData(2);

	QList<QAction *> ret;

	auto action = new QAction(tr("Hardware Breakpoints"), this);
	action->setMenu(menu);
	ret << action;
	return ret;
}


//------------------------------------------------------------------------------
// Name: cpu_context_menu
// Desc:
//------------------------------------------------------------------------------
QList<QAction *> HardwareBreakpoints::cpu_context_menu() {

	auto menu = new QMenu(tr("Hardware Breakpoints"));
	auto ex1 = menu->addAction(tr("Hardware, On Execute #1"), this, SLOT(set_exec1()));
	auto ex2 = menu->addAction(tr("Hardware, On Execute #2"), this, SLOT(set_exec2()));
	auto ex3 = menu->addAction(tr("Hardware, On Execute #3"), this, SLOT(set_exec3()));
	auto ex4 = menu->addAction(tr("Hardware, On Execute #4"), this, SLOT(set_exec4()));

	auto rw1 = menu->addAction(tr("Hardware, On Read/Write #1"), this, SLOT(set_access1()));
	auto rw2 = menu->addAction(tr("Hardware, On Read/Write #2"), this, SLOT(set_access2()));
	auto rw3 = menu->addAction(tr("Hardware, On Read/Write #3"), this, SLOT(set_access3()));
	auto rw4 = menu->addAction(tr("Hardware, On Read/Write #4"), this, SLOT(set_access4()));

	auto wo1 = menu->addAction(tr("Hardware, On Write #1"), this, SLOT(set_write1()));
	auto wo2 = menu->addAction(tr("Hardware, On Write #2"), this, SLOT(set_write2()));
	auto wo3 = menu->addAction(tr("Hardware, On Write #3"), this, SLOT(set_write3()));
	auto wo4 = menu->addAction(tr("Hardware, On Write #4"), this, SLOT(set_write4()));


	ex1->setData(3);
	ex2->setData(3);
	ex3->setData(3);
	ex4->setData(3);

	rw1->setData(3);
	rw2->setData(3);
	rw3->setData(3);
	rw4->setData(3);

	wo1->setData(3);
	wo2->setData(3);
	wo3->setData(3);
	wo4->setData(3);

	QList<QAction *> ret;

	auto action = new QAction(tr("Hardware Breakpoints"), this);
	action->setMenu(menu);
	ret << action;
	return ret;
}

//------------------------------------------------------------------------------
// Name: setExecuteBP
// Desc: 
//------------------------------------------------------------------------------
void HardwareBreakpoints::setExecuteBP(int index, bool inUse) {
	// we want to be enabled, if we aren't already hooked,
	// hook it
	if(!old_event_handler_) {
		old_event_handler_ = edb::v1::set_debug_event_handler(this);
	}

	if(IProcess *process = edb::v1::debugger_core->process()) {

		if(inUse) {
			QMessageBox::StandardButton button = QMessageBox::question(nullptr, tr("Breakpoint Already In Use"), tr("This breakpoint is already being used. Do you want to replace it?"), QMessageBox::Yes | QMessageBox::Cancel);
			if(button != QMessageBox::Yes) {
				return;
			}
		}

		edb::address_t address = edb::v1::cpu_selected_address();

		for(IThread::pointer thread : process->threads()) {
			State state;
			thread->get_state(&state);
			setBreakpointState(&state, index, { true, address, 0, 0 });
			thread->set_state(state);
		}
	}

	edb::v1::update_ui();
}

//------------------------------------------------------------------------------
// Name: setWriteBP
// Desc: 
//------------------------------------------------------------------------------
void HardwareBreakpoints::setWriteBP(int index, bool inUse, edb::address_t address, int size) {
	// we want to be enabled, if we aren't already hooked,
	// hook it
	if(!old_event_handler_) {
		old_event_handler_ = edb::v1::set_debug_event_handler(this);
	}
	
	if(IProcess *process = edb::v1::debugger_core->process()) {

		if(inUse) {
			QMessageBox::StandardButton button = QMessageBox::question(nullptr, tr("Breakpoint Already In Use"), tr("This breakpoint is already being used. Do you want to replace it?"), QMessageBox::Yes | QMessageBox::Cancel);
			if(button != QMessageBox::Yes) {
				return;
			}
		}

		for(IThread::pointer thread : process->threads()) {
			State state;
			thread->get_state(&state);

			switch(size) {
			case 1:
				setBreakpointState(&state, index, { true, address, 1, 0 });
				break;
			case 2:
				setBreakpointState(&state, index, { true, address, 1, 1 });
				break;
			case 4:
				setBreakpointState(&state, index, { true, address, 1, 2 });
				break;
			case 8:
				setBreakpointState(&state, index, { true, address, 1, 3 });
				break;
			default:
				QMessageBox::information(nullptr, tr("Invalid Selection Size"), tr("Please select 1, 2, 4, or 8 bytes for this type of hardward breakpoint"));
				return;
			}

			thread->set_state(state);
		}
	}

	edb::v1::update_ui();
}

//------------------------------------------------------------------------------
// Name: setReadWriteBP
// Desc: 
//------------------------------------------------------------------------------
void HardwareBreakpoints::setReadWriteBP(int index, bool inUse, edb::address_t address, int size) {
	// we want to be enabled, if we aren't already hooked,
	// hook it
	if(!old_event_handler_) {
		old_event_handler_ = edb::v1::set_debug_event_handler(this);
	}
	
	if(IProcess *process = edb::v1::debugger_core->process()) {

		if(inUse) {
			QMessageBox::StandardButton button = QMessageBox::question(nullptr, tr("Breakpoint Already In Use"), tr("This breakpoint is already being used. Do you want to replace it?"), QMessageBox::Yes | QMessageBox::Cancel);
			if(button != QMessageBox::Yes) {
				return;
			}
		}

		for(IThread::pointer thread : process->threads()) {
			State state;
			thread->get_state(&state);

			switch(size) {
			case 1:
				setBreakpointState(&state, index, { true, address, 2, 0 });
				break;
			case 2:
				setBreakpointState(&state, index, { true, address, 2, 1 });
				break;
			case 4:
				setBreakpointState(&state, index, { true, address, 2, 2 });
				break;
			case 8:
				setBreakpointState(&state, index, { true, address, 2, 3 });
				break;
			default:
				QMessageBox::information(nullptr, tr("Invalid Selection Size"), tr("Please select 1, 2, 4, or 8 bytes for this type of hardward breakpoint"));
				return;
			}

			thread->set_state(state);
		}
	}

	edb::v1::update_ui();
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void HardwareBreakpoints::set_exec(int index) {
	if(auto a = qobject_cast<QAction *>(sender())) {
		switch(a->data().toLongLong()) {
		case 3:
			setExecuteBP(index, enabled_[index]->isChecked());
			break;
		default:
			Q_ASSERT(0 && "Internal Error");
			break;
		}
	}
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void HardwareBreakpoints::set_write(int index) {
	if(auto a = qobject_cast<QAction *>(sender())) {
		switch(a->data().toLongLong()) {
		case 1:
			setStackWriteBP(index, enabled_[index]->isChecked());
			break;
		case 2:
			setDataWriteBP(index, enabled_[index]->isChecked());
			break;
		case 3:
			setCPUWriteBP(index, enabled_[index]->isChecked());
			break;
		default:
			Q_ASSERT(0 && "Internal Error");
			break;
		}
	}
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void HardwareBreakpoints::set_access(int index) {
	if(auto a = qobject_cast<QAction *>(sender())) {
		switch(a->data().toLongLong()) {
		case 1:
			setStackReadWriteBP(index, enabled_[index]->isChecked());
			break;
		case 2:
			setDataReadWriteBP(index, enabled_[index]->isChecked());
			break;
		case 3:
			setCPUReadWriteBP(index, enabled_[index]->isChecked());
			break;
		default:
			Q_ASSERT(0 && "Internal Error");
			break;
		}
	}
}

//------------------------------------------------------------------------------
// Name: set_exec1
//------------------------------------------------------------------------------
void HardwareBreakpoints::set_exec1() {
	set_exec(Register1);
}

//------------------------------------------------------------------------------
// Name: set_exec2
//------------------------------------------------------------------------------
void HardwareBreakpoints::set_exec2() {
	set_exec(Register2);
}

//------------------------------------------------------------------------------
// Name: set_exec3
//------------------------------------------------------------------------------
void HardwareBreakpoints::set_exec3() {
	set_exec(Register3);
}

//------------------------------------------------------------------------------
// Name: set_exec4
//------------------------------------------------------------------------------
void HardwareBreakpoints::set_exec4() {
	set_exec(Register4);
}

//------------------------------------------------------------------------------
// Name: set_access1
//------------------------------------------------------------------------------
void HardwareBreakpoints::set_access1() {
	set_access(Register1);
}

//------------------------------------------------------------------------------
// Name: set_access2
//------------------------------------------------------------------------------
void HardwareBreakpoints::set_access2() {
	set_access(Register2);
}

//------------------------------------------------------------------------------
// Name: set_access3
//------------------------------------------------------------------------------
void HardwareBreakpoints::set_access3() {
	set_access(Register3);
}

//------------------------------------------------------------------------------
// Name: set_access4
//------------------------------------------------------------------------------
void HardwareBreakpoints::set_access4() {
	set_access(Register4);
}

//------------------------------------------------------------------------------
// Name: set_write1
//------------------------------------------------------------------------------
void HardwareBreakpoints::set_write1() {
	set_write(Register1);
}

//------------------------------------------------------------------------------
// Name: set_write2
//------------------------------------------------------------------------------
void HardwareBreakpoints::set_write2() {
	set_write(Register2);
}

//------------------------------------------------------------------------------
// Name: set_write3
//------------------------------------------------------------------------------
void HardwareBreakpoints::set_write3() {
	set_write(Register3);
}

//------------------------------------------------------------------------------
// Name: set_write4
//------------------------------------------------------------------------------
void HardwareBreakpoints::set_write4() {
	set_write(Register4);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void HardwareBreakpoints::setDataReadWriteBP(int index, bool inUse) {
	const edb::address_t address = edb::v1::selected_data_address();
	const size_t         size    = edb::v1::selected_data_size();
	setReadWriteBP(index, inUse, address, size);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void HardwareBreakpoints::setDataWriteBP(int index, bool inUse) {
	const edb::address_t address = edb::v1::selected_data_address();
	const size_t         size    = edb::v1::selected_data_size();
	setReadWriteBP(index, inUse, address, size);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void HardwareBreakpoints::setStackReadWriteBP(int index, bool inUse) {
	const edb::address_t address = edb::v1::selected_stack_address();
	const size_t         size    = edb::v1::selected_stack_size();
	setReadWriteBP(index, inUse, address, size);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void HardwareBreakpoints::setStackWriteBP(int index, bool inUse) {
	const edb::address_t address = edb::v1::selected_stack_address();
	const size_t         size    = edb::v1::selected_stack_size();
	setWriteBP(index, inUse, address, size);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void HardwareBreakpoints::setCPUReadWriteBP(int index, bool inUse) {
	const edb::address_t address = edb::v1::cpu_selected_address();
	const size_t         size    = 1;
	setWriteBP(index, inUse, address, size);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void HardwareBreakpoints::setCPUWriteBP(int index, bool inUse) {
	const edb::address_t address = edb::v1::cpu_selected_address();
	const size_t         size    = 1;
	setWriteBP(index, inUse, address, size);
}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(HardwareBreakpoints, HardwareBreakpoints)
#endif

}


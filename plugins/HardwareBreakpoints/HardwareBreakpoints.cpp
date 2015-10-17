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
	dialog_ = new DialogHWBreakpoints(edb::v1::debugger_ui);
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
// Name: setup_bp
// Desc:
//------------------------------------------------------------------------------
void HardwareBreakpoints::setup_bp(State *state, int num, bool enabled, edb::address_t addr, int type, int size) {

	const int N1 = 16 + (num * 4);
	const int N2 = 18 + (num * 4);


	// default to disabled
	state->set_debug_register(7, (state->debug_register(7) & ~(0x01 << (num * 2))));

	if(enabled) {
		// set the address
		state->set_debug_register(num, addr);

		// enable this breakpoint
		state->set_debug_register(7, state->debug_register(7) | (0x01 << (num * 2)));

		// setup the type
		switch(type) {
		case 2:
			// read/write
			state->set_debug_register(7, (state->debug_register(7) & ~(0x03 << N1)) | (0x03 << N1));
			break;
		case 1:
			// write
			state->set_debug_register(7, (state->debug_register(7) & ~(0x03 << N1)) | (0x01 << N1));
			break;
		case 0:
			// execute
			state->set_debug_register(7, (state->debug_register(7) & ~(0x03 << N1)));
			break;
		}

		if(type != 0) {
			// setup the size
			switch(size) {
			case 3:
				// 8 bytes
				Q_ASSERT(edb::v1::debuggeeIs32Bit());
				state->set_debug_register(7, (state->debug_register(7) & ~(0x03 << N2)) | (0x02 << N2));
				break;
			case 2:
				// 4 bytes
				state->set_debug_register(7, (state->debug_register(7) & ~(0x03 << N2)) | (0x03 << N2));
				break;
			case 1:
				// 2 bytes
				state->set_debug_register(7, (state->debug_register(7) & ~(0x03 << N2)) | (0x01 << N2));
				break;
			case 0:
				// 1 byte
				state->set_debug_register(7, (state->debug_register(7) & ~(0x03 << N2)));
				break;
			}
		} else {
			state->set_debug_register(7, (state->debug_register(7) & ~(0x03 << N2)));
		}
	}
}

//------------------------------------------------------------------------------
// Name: setup_breakpoints
// Desc:
//------------------------------------------------------------------------------
void HardwareBreakpoints::setup_breakpoints() {

	// TODO: assert that we are paused

	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(auto p = qobject_cast<DialogHWBreakpoints *>(dialog_)) {

			const bool enabled =
				p->ui->chkBP1->isChecked() ||
				p->ui->chkBP2->isChecked() ||
				p->ui->chkBP3->isChecked() ||
				p->ui->chkBP4->isChecked();

			if(enabled) {

				edb::address_t addr[4];
				bool ok[4];

				// validate all of the entries
				addr[0] = edb::v1::string_to_address(p->ui->txtBP1->text(), &ok[0]);
				addr[1] = edb::v1::string_to_address(p->ui->txtBP2->text(), &ok[1]);
				addr[2] = edb::v1::string_to_address(p->ui->txtBP3->text(), &ok[2]);
				addr[3] = edb::v1::string_to_address(p->ui->txtBP4->text(), &ok[3]);


				if(!ok[0] && !ok[1] && !ok[2] && !ok[3]) {
					QMessageBox::information(
						0,
						tr("Address Error"),
						tr("The address provided does not appear to be valid"));
					return;
				}

				if(ok[0] && !validate_bp(p->ui->chkBP1->isChecked(), addr[0], p->ui->cmbType1->currentIndex(), p->ui->cmbSize1->currentIndex())) {
					return;
				}

				if(ok[1] && !validate_bp(p->ui->chkBP2->isChecked(), addr[0], p->ui->cmbType2->currentIndex(), p->ui->cmbSize2->currentIndex())) {
					return;
				}

				if(ok[2] && !validate_bp(p->ui->chkBP3->isChecked(), addr[0], p->ui->cmbType3->currentIndex(), p->ui->cmbSize3->currentIndex())) {
					return;
				}

				if(ok[3] && !validate_bp(p->ui->chkBP4->isChecked(), addr[0], p->ui->cmbType4->currentIndex(), p->ui->cmbSize4->currentIndex())) {
					return;
				}

				// we want to be enabled, if we aren't already hooked,
				// hook it
				if(!old_event_handler_) {
					old_event_handler_ = edb::v1::set_debug_event_handler(this);
				}

				for(IThread::pointer thread : process->threads()) {
					State state;
					thread->get_state(&state);

					if(ok[0]) {
						setup_bp(&state, 0, p->ui->chkBP1->isChecked(), addr[0], p->ui->cmbType1->currentIndex(), p->ui->cmbSize1->currentIndex());
					}

					if(ok[1]) {
						setup_bp(&state, 1, p->ui->chkBP2->isChecked(), addr[1], p->ui->cmbType2->currentIndex(), p->ui->cmbSize2->currentIndex());
					}

					if(ok[2]) {
						setup_bp(&state, 2, p->ui->chkBP3->isChecked(), addr[2], p->ui->cmbType3->currentIndex(), p->ui->cmbSize3->currentIndex());
					}

					if(ok[3]) {
						setup_bp(&state, 3, p->ui->chkBP4->isChecked(), addr[3], p->ui->cmbType4->currentIndex(), p->ui->cmbSize4->currentIndex());
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
	}
}

//------------------------------------------------------------------------------
// Name: show_menu
// Desc:
//------------------------------------------------------------------------------
void HardwareBreakpoints::show_menu() {

	if(dialog_->exec() == QDialog::Accepted) {
		setup_breakpoints();
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
// Name: validate_bp
// Desc:
//------------------------------------------------------------------------------
bool HardwareBreakpoints::validate_bp(bool enabled, edb::address_t addr, int type, int size) {

	if(enabled) {
		switch(type) {
		case 2:
		case 1:
			{
				const edb::address_t address_mask = (1u << size) - 1;
				if((addr & address_mask) != 0) {
					QMessageBox::information(
						0,
						tr("Address Alignment Error"),
						tr("Hardware read/write breakpoints must be aligned to there address."));
					return false;
				}
			}
			break;
		default:
			break;
		}

		if(edb::v1::debuggeeIs32Bit()) {
			if(size == 3) {
				QMessageBox::information(
					0,
					tr("BP Size Error"),
					tr("Hardware read/write breakpoints cannot be 8-bytes in a 32-bit debuggee."));
				return false;
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------------
// Name: cpu_context_menu
// Desc:
//------------------------------------------------------------------------------
QList<QAction *> HardwareBreakpoints::cpu_context_menu() {

	auto menu = new QMenu(tr("Hardware Breakpoints"));
	menu->addAction(tr("Hardware, On Execute #1"), this, SLOT(set_exec1()));
	menu->addAction(tr("Hardware, On Execute #2"), this, SLOT(set_exec2()));
	menu->addAction(tr("Hardware, On Execute #3"), this, SLOT(set_exec3()));
	menu->addAction(tr("Hardware, On Execute #4"), this, SLOT(set_exec4()));

	QList<QAction *> ret;

	auto action = new QAction(tr("Hardware Breakpoints"), this);
	action->setMenu(menu);
	ret << action;
	return ret;
}

//------------------------------------------------------------------------------
// Name: set_exec1
// Desc: TODO(eteran): this code is so much more complex than I'd like...
//------------------------------------------------------------------------------
void HardwareBreakpoints::set_exec1() {

	// we want to be enabled, if we aren't already hooked,
	// hook it
	if(!old_event_handler_) {
		old_event_handler_ = edb::v1::set_debug_event_handler(this);
	}

	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(auto p = qobject_cast<DialogHWBreakpoints *>(dialog_)) {

			if(p->ui->chkBP1->isChecked()) {
				QMessageBox::StandardButton button = QMessageBox::question(nullptr, tr("Breakpoint Already In Use"), tr("This breakpoint is already being used. Do you want to replace it?"), QMessageBox::Yes | QMessageBox::Cancel);
				if(button != QMessageBox::Yes) {
					return;
				}
			}

			edb::address_t address = edb::v1::cpu_selected_address();

			p->ui->chkBP1->setChecked(true);
			p->ui->cmbType1->setCurrentIndex(0);
			p->ui->cmbSize1->setCurrentIndex(0);

			for(IThread::pointer thread : process->threads()) {
				State state;
				thread->get_state(&state);
				setup_bp(&state, 0, p->ui->chkBP1->isChecked(), address, p->ui->cmbType1->currentIndex(), p->ui->cmbSize1->currentIndex());
				thread->set_state(state);
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: set_exec2
// Desc: TODO(eteran): this code is so much more complex than I'd like...
//------------------------------------------------------------------------------
void HardwareBreakpoints::set_exec2() {
	// we want to be enabled, if we aren't already hooked,
	// hook it
	if(!old_event_handler_) {
		old_event_handler_ = edb::v1::set_debug_event_handler(this);
	}

	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(auto p = qobject_cast<DialogHWBreakpoints *>(dialog_)) {


			if(p->ui->chkBP2->isChecked()) {
				QMessageBox::StandardButton button = QMessageBox::question(nullptr, tr("Breakpoint Already In Use"), tr("This breakpoint is already being used. Do you want to replace it?"), QMessageBox::Yes | QMessageBox::Cancel);
				if(button != QMessageBox::Yes) {
					return;
				}
			}


			edb::address_t address = edb::v1::cpu_selected_address();

			p->ui->chkBP2->setChecked(true);
			p->ui->cmbType2->setCurrentIndex(0);
			p->ui->cmbSize2->setCurrentIndex(0);

			for(IThread::pointer thread : process->threads()) {
				State state;
				thread->get_state(&state);
				setup_bp(&state, 1, p->ui->chkBP2->isChecked(), address, p->ui->cmbType2->currentIndex(), p->ui->cmbSize2->currentIndex());
				thread->set_state(state);
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: set_exec3
// Desc: TODO(eteran): this code is so much more complex than I'd like...
//------------------------------------------------------------------------------
void HardwareBreakpoints::set_exec3() {
	// we want to be enabled, if we aren't already hooked,
	// hook it
	if(!old_event_handler_) {
		old_event_handler_ = edb::v1::set_debug_event_handler(this);
	}

	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(auto p = qobject_cast<DialogHWBreakpoints *>(dialog_)) {


			if(p->ui->chkBP3->isChecked()) {
				QMessageBox::StandardButton button = QMessageBox::question(nullptr, tr("Breakpoint Already In Use"), tr("This breakpoint is already being used. Do you want to replace it?"), QMessageBox::Yes | QMessageBox::Cancel);
				if(button != QMessageBox::Yes) {
					return;
				}
			}

			edb::address_t address = edb::v1::cpu_selected_address();

			p->ui->chkBP3->setChecked(true);
			p->ui->cmbType3->setCurrentIndex(0);
			p->ui->cmbSize3->setCurrentIndex(0);

			for(IThread::pointer thread : process->threads()) {
				State state;
				thread->get_state(&state);
				setup_bp(&state, 2, p->ui->chkBP3->isChecked(), address, p->ui->cmbType3->currentIndex(), p->ui->cmbSize3->currentIndex());
				thread->set_state(state);
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: set_exec4
// Desc: TODO(eteran): this code is so much more complex than I'd like...
//------------------------------------------------------------------------------
void HardwareBreakpoints::set_exec4() {
	// we want to be enabled, if we aren't already hooked,
	// hook it
	if(!old_event_handler_) {
		old_event_handler_ = edb::v1::set_debug_event_handler(this);
	}

	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(auto p = qobject_cast<DialogHWBreakpoints *>(dialog_)) {

			if(p->ui->chkBP4->isChecked()) {
				QMessageBox::StandardButton button = QMessageBox::question(nullptr, tr("Breakpoint Already In Use"), tr("This breakpoint is already being used. Do you want to replace it?"), QMessageBox::Yes | QMessageBox::Cancel);
				if(button != QMessageBox::Yes) {
					return;
				}
			}

			edb::address_t address = edb::v1::cpu_selected_address();

			p->ui->chkBP4->setChecked(true);
			p->ui->cmbType4->setCurrentIndex(0);
			p->ui->cmbSize4->setCurrentIndex(0);

			for(IThread::pointer thread : process->threads()) {
				State state;
				thread->get_state(&state);
				setup_bp(&state, 3, p->ui->chkBP1->isChecked(), address, p->ui->cmbType4->currentIndex(), p->ui->cmbSize4->currentIndex());
				thread->set_state(state);
			}
		}
	}
}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(HardwareBreakpoints, HardwareBreakpoints)
#endif

}


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
#include "DialogHwBreakpoints.h"
#include "IDebugEvent.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "IThread.h"
#include "State.h"
#include "edb.h"

#include <QDialog>
#include <QMenu>
#include <QMessageBox>
#include <QtDebug>

#include "ui_DialogHwBreakpoints.h"

// TODO: at the moment, nearly this entire file is x86/x86-64 specific
//       we need to figure out a proper way to support (if at all) non
//       x86 arches

#if !(defined(EDB_X86_64) || defined(EDB_X86))
#error "Unsupported Platform"
#endif

namespace HardwareBreakpointsPlugin {

/**
 * @brief HardwareBreakpoints::HardwareBreakpoints
 * @param parent
 */
HardwareBreakpoints::HardwareBreakpoints(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief HardwareBreakpoints::private_init
 */
void HardwareBreakpoints::privateInit() {

	auto dialog = new DialogHwBreakpoints(edb::v1::debugger_ui);
	dialog_     = dialog;

	// indexed access to members for simplicity later
	enabled_[Register1] = dialog->ui.chkBP1;
	enabled_[Register2] = dialog->ui.chkBP2;
	enabled_[Register3] = dialog->ui.chkBP3;
	enabled_[Register4] = dialog->ui.chkBP4;

	types_[Register1] = dialog->ui.cmbType1;
	types_[Register2] = dialog->ui.cmbType2;
	types_[Register3] = dialog->ui.cmbType3;
	types_[Register4] = dialog->ui.cmbType4;

	sizes_[Register1] = dialog->ui.cmbSize1;
	sizes_[Register2] = dialog->ui.cmbSize2;
	sizes_[Register3] = dialog->ui.cmbSize3;
	sizes_[Register4] = dialog->ui.cmbSize4;

	addresses_[Register1] = dialog->ui.txtBP1;
	addresses_[Register2] = dialog->ui.txtBP2;
	addresses_[Register3] = dialog->ui.txtBP3;
	addresses_[Register4] = dialog->ui.txtBP4;

	edb::v1::add_debug_event_handler(this);
}

/**
 * @brief HardwareBreakpoints::privateFini
 */
void HardwareBreakpoints::privateFini() {
	edb::v1::remove_debug_event_handler(this);
}

/**
 * @brief HardwareBreakpoints::menu
 * @param parent
 * @return
 */
QMenu *HardwareBreakpoints::menu(QWidget *parent) {

	Q_ASSERT(parent);

	if (!menu_) {
		menu_ = new QMenu(tr("Hardware BreakpointManager"), parent);
		menu_->addAction(tr("&Hardware Breakpoints"), this, SLOT(showMenu()), QKeySequence(tr("Ctrl+Shift+H")));
	}

	return menu_;
}

/**
 * @brief HardwareBreakpoints::setupBreakpoints
 */
void HardwareBreakpoints::setupBreakpoints() {

	if (IProcess *process = edb::v1::debugger_core->process()) {

		if (!process->isPaused()) {
			QMessageBox::warning(
				nullptr,
				tr("Process Not Paused"),
				tr("Unable to update hardware breakpoints because the process does not appear to be currently paused. Please suspend the process."));
			return;
		}

		const bool enabled =
			enabled_[Register1]->isChecked() ||
			enabled_[Register2]->isChecked() ||
			enabled_[Register3]->isChecked() ||
			enabled_[Register4]->isChecked();

		if (enabled) {

			edb::address_t addr[RegisterCount];
			bool ok[RegisterCount];

			// evaluate all the expressions
			for (int i = 0; i < RegisterCount; ++i) {
				ok[i] = enabled_[i]->isChecked() && edb::v1::eval_expression(addresses_[i]->text(), &addr[i]);
			}

			if (!ok[Register1] && !ok[Register2] && !ok[Register3] && !ok[Register4]) {
				QMessageBox::critical(
					nullptr,
					tr("Address Error"),
					tr("An address expression provided does not appear to be valid"));
				return;
			}

			for (int i = 0; i < RegisterCount; ++i) {
				if (ok[i]) {

					const BreakpointStatus status = validate_breakpoint({enabled_[i]->isChecked(),
																		 addr[i],
																		 types_[i]->currentIndex(),
																		 sizes_[i]->currentIndex()});

					switch (status) {
					case AlignmentError:
						QMessageBox::critical(
							nullptr,
							tr("Address Alignment Error"),
							tr("Hardware read/write breakpoint address must be aligned to breakpoint size."));
						return;
					case SizeError:
						QMessageBox::critical(
							nullptr,
							tr("BP Size Error"),
							tr("Hardware read/write breakpoints cannot be 8-bytes in a 32-bit debuggee."));
						return;
					case Valid:
						break;
					}
				}
			}

			for (std::shared_ptr<IThread> &thread : process->threads()) {
				State state;
				thread->getState(&state);

				for (int i = 0; i < RegisterCount; ++i) {
					if (ok[i]) {
						set_breakpoint_state(
							&state,
							i,
							{enabled_[i]->isChecked(),
							 addr[i],
							 types_[i]->currentIndex(),
							 sizes_[i]->currentIndex()});
					}
				}

				thread->setState(state);
			}

		} else {

			for (std::shared_ptr<IThread> &thread : process->threads()) {
				State state;
				thread->getState(&state);
				state.setDebugRegister(7, 0);
				thread->setState(state);
			}
		}
	}

	edb::v1::update_ui();
}

/**
 * @brief HardwareBreakpoints::showMenu
 */
void HardwareBreakpoints::showMenu() {

	if (dialog_->exec() == QDialog::Accepted) {
		setupBreakpoints();
	}
}

/**
 * @brief HardwareBreakpoints::handleEvent
 *
 * this hooks the debug event handler so we can make the breakpoints able to be resumed
 *
 * @param event
 * @return
 */
edb::EventStatus HardwareBreakpoints::handleEvent(const std::shared_ptr<IDebugEvent> &event) {

	if (event->stopped() && event->isTrap()) {

		if (IProcess *process = edb::v1::debugger_core->process()) {
			if (std::shared_ptr<IThread> thread = process->currentThread()) {
				// check DR6 to see if it was a HW BP event
				// if so, set the resume flag
				State state;
				thread->getState(&state);
				if ((state.debugRegister(6) & 0x0f) != 0x00) {
					state.setFlags(state.flags() | (1 << 16));
					thread->setState(state);
				}
			}
		}
	}

	// pass the event down the stack
	return edb::DEBUG_NEXT_HANDLER;
}

/**
 * @brief HardwareBreakpoints::stackContextMenu
 * @return
 */
QList<QAction *> HardwareBreakpoints::stackContextMenu() {
	auto menu = new QMenu(tr("Hardware Breakpoints"));

	auto rw1 = menu->addAction(tr("Hardware, On Read/Write #1"), this, SLOT(setAccess1()));
	auto rw2 = menu->addAction(tr("Hardware, On Read/Write #2"), this, SLOT(setAccess2()));
	auto rw3 = menu->addAction(tr("Hardware, On Read/Write #3"), this, SLOT(setAccess3()));
	auto rw4 = menu->addAction(tr("Hardware, On Read/Write #4"), this, SLOT(setAccess4()));

	auto wo1 = menu->addAction(tr("Hardware, On Write #1"), this, SLOT(setWrite1()));
	auto wo2 = menu->addAction(tr("Hardware, On Write #2"), this, SLOT(setWrite2()));
	auto wo3 = menu->addAction(tr("Hardware, On Write #3"), this, SLOT(setWrite3()));
	auto wo4 = menu->addAction(tr("Hardware, On Write #4"), this, SLOT(setWrite4()));

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

/**
 * @brief HardwareBreakpoints::dataContextMenu
 * @return
 */
QList<QAction *> HardwareBreakpoints::dataContextMenu() {
	auto menu = new QMenu(tr("Hardware Breakpoints"));

	auto rw1 = menu->addAction(tr("Hardware, On Read/Write #1"), this, SLOT(setAccess1()));
	auto rw2 = menu->addAction(tr("Hardware, On Read/Write #2"), this, SLOT(setAccess2()));
	auto rw3 = menu->addAction(tr("Hardware, On Read/Write #3"), this, SLOT(setAccess3()));
	auto rw4 = menu->addAction(tr("Hardware, On Read/Write #4"), this, SLOT(setAccess4()));

	auto wo1 = menu->addAction(tr("Hardware, On Write #1"), this, SLOT(setWrite1()));
	auto wo2 = menu->addAction(tr("Hardware, On Write #2"), this, SLOT(setWrite2()));
	auto wo3 = menu->addAction(tr("Hardware, On Write #3"), this, SLOT(setWrite3()));
	auto wo4 = menu->addAction(tr("Hardware, On Write #4"), this, SLOT(setWrite4()));

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

/**
 * @brief HardwareBreakpoints::cpuContextMenu
 * @return
 */
QList<QAction *> HardwareBreakpoints::cpuContextMenu() {

	auto menu = new QMenu(tr("Hardware Breakpoints"));
	auto ex1  = menu->addAction(tr("Hardware, On Execute #1"), this, SLOT(setExec1()));
	auto ex2  = menu->addAction(tr("Hardware, On Execute #2"), this, SLOT(setExec2()));
	auto ex3  = menu->addAction(tr("Hardware, On Execute #3"), this, SLOT(setExec3()));
	auto ex4  = menu->addAction(tr("Hardware, On Execute #4"), this, SLOT(setExec4()));

	auto rw1 = menu->addAction(tr("Hardware, On Read/Write #1"), this, SLOT(setAccess1()));
	auto rw2 = menu->addAction(tr("Hardware, On Read/Write #2"), this, SLOT(setAccess2()));
	auto rw3 = menu->addAction(tr("Hardware, On Read/Write #3"), this, SLOT(setAccess3()));
	auto rw4 = menu->addAction(tr("Hardware, On Read/Write #4"), this, SLOT(setAccess4()));

	auto wo1 = menu->addAction(tr("Hardware, On Write #1"), this, SLOT(setWrite1()));
	auto wo2 = menu->addAction(tr("Hardware, On Write #2"), this, SLOT(setWrite2()));
	auto wo3 = menu->addAction(tr("Hardware, On Write #3"), this, SLOT(setWrite3()));
	auto wo4 = menu->addAction(tr("Hardware, On Write #4"), this, SLOT(setWrite4()));

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

/**
 * @brief HardwareBreakpoints::setExecuteBP
 * @param index
 * @param inUse
 */
void HardwareBreakpoints::setExecuteBP(int index, bool inUse) {

	if (IProcess *process = edb::v1::debugger_core->process()) {

		if (!process->isPaused()) {
			QMessageBox::warning(
				nullptr,
				tr("Process Not Paused"),
				tr("Unable to update hardware breakpoints because the process does not appear to be currently paused. Please suspend the process."));
			return;
		}

		if (inUse) {
			QMessageBox::StandardButton button = QMessageBox::question(nullptr, tr("Breakpoint Already In Use"), tr("This breakpoint is already being used. Do you want to replace it?"), QMessageBox::Yes | QMessageBox::Cancel);
			if (button != QMessageBox::Yes) {
				return;
			}
		}

		edb::address_t address = edb::v1::cpu_selected_address();

		for (std::shared_ptr<IThread> &thread : process->threads()) {
			State state;
			thread->getState(&state);
			set_breakpoint_state(&state, index, {true, address, 0, 0});
			thread->setState(state);
		}
	}

	edb::v1::update_ui();
}

/**
 * @brief HardwareBreakpoints::setWriteBP
 * @param index
 * @param inUse
 * @param address
 * @param size
 */
void HardwareBreakpoints::setWriteBP(int index, bool inUse, edb::address_t address, size_t size) {

	if (IProcess *process = edb::v1::debugger_core->process()) {

		if (!process->isPaused()) {
			QMessageBox::warning(
				nullptr,
				tr("Process Not Paused"),
				tr("Unable to update hardware breakpoints because the process does not appear to be currently paused. Please suspend the process."));
			return;
		}

		if (inUse) {
			QMessageBox::StandardButton button = QMessageBox::question(nullptr, tr("Breakpoint Already In Use"), tr("This breakpoint is already being used. Do you want to replace it?"), QMessageBox::Yes | QMessageBox::Cancel);
			if (button != QMessageBox::Yes) {
				return;
			}
		}

		for (std::shared_ptr<IThread> &thread : process->threads()) {
			State state;
			thread->getState(&state);

			switch (size) {
			case 1:
				set_breakpoint_state(&state, index, {true, address, 1, 0});
				break;
			case 2:
				set_breakpoint_state(&state, index, {true, address, 1, 1});
				break;
			case 4:
				set_breakpoint_state(&state, index, {true, address, 1, 2});
				break;
			case 8:
				set_breakpoint_state(&state, index, {true, address, 1, 3});
				break;
			default:
				QMessageBox::critical(nullptr, tr("Invalid Selection Size"), tr("Please select 1, 2, 4, or 8 bytes for this type of hardware breakpoint"));
				return;
			}

			thread->setState(state);
		}
	}

	edb::v1::update_ui();
}

/**
 * @brief HardwareBreakpoints::setReadWriteBP
 * @param index
 * @param inUse
 * @param address
 * @param size
 */
void HardwareBreakpoints::setReadWriteBP(int index, bool inUse, edb::address_t address, size_t size) {

	if (IProcess *process = edb::v1::debugger_core->process()) {

		if (!process->isPaused()) {
			QMessageBox::warning(
				nullptr,
				tr("Process Not Paused"),
				tr("Unable to update hardware breakpoints because the process does not appear to be currently paused. Please suspend the process."));
			return;
		}

		if (inUse) {
			QMessageBox::StandardButton button = QMessageBox::question(
				nullptr,
				tr("Breakpoint Already In Use"),
				tr("This breakpoint is already being used. Do you want to replace it?"),
				QMessageBox::Yes | QMessageBox::Cancel);
			if (button != QMessageBox::Yes) {
				return;
			}
		}

		for (std::shared_ptr<IThread> &thread : process->threads()) {
			State state;
			thread->getState(&state);

			switch (size) {
			case 1:
				set_breakpoint_state(&state, index, {true, address, 2, 0});
				break;
			case 2:
				set_breakpoint_state(&state, index, {true, address, 2, 1});
				break;
			case 4:
				set_breakpoint_state(&state, index, {true, address, 2, 2});
				break;
			case 8:
				set_breakpoint_state(&state, index, {true, address, 2, 3});
				break;
			default:
				QMessageBox::critical(nullptr, tr("Invalid Selection Size"), tr("Please select 1, 2, 4, or 8 bytes for this type of hardward breakpoint"));
				return;
			}

			thread->setState(state);
		}
	}

	edb::v1::update_ui();
}

/**
 * @brief HardwareBreakpoints::setExec
 * @param index
 */
void HardwareBreakpoints::setExec(int index) {
	if (auto a = qobject_cast<QAction *>(sender())) {
		switch (a->data().toLongLong()) {
		case 3:
			setExecuteBP(index, enabled_[index]->isChecked());
			break;
		default:
			Q_ASSERT(0 && "Internal Error");
			break;
		}
	}
}

/**
 * @brief HardwareBreakpoints::setWrite
 * @param index
 */
void HardwareBreakpoints::setWrite(int index) {
	if (auto a = qobject_cast<QAction *>(sender())) {
		switch (a->data().toLongLong()) {
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

/**
 * @brief HardwareBreakpoints::setAccess
 * @param index
 */
void HardwareBreakpoints::setAccess(int index) {
	if (auto a = qobject_cast<QAction *>(sender())) {
		switch (a->data().toLongLong()) {
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

/**
 * @brief HardwareBreakpoints::setDataReadWriteBP
 * @param index
 * @param inUse
 */
void HardwareBreakpoints::setDataReadWriteBP(int index, bool inUse) {
	const edb::address_t address = edb::v1::selected_data_address();
	const size_t size            = edb::v1::selected_data_size();
	setReadWriteBP(index, inUse, address, size);
}

/**
 * @brief HardwareBreakpoints::setDataWriteBP
 * @param index
 * @param inUse
 */
void HardwareBreakpoints::setDataWriteBP(int index, bool inUse) {
	const edb::address_t address = edb::v1::selected_data_address();
	const size_t size            = edb::v1::selected_data_size();
	setReadWriteBP(index, inUse, address, size);
}

/**
 * @brief HardwareBreakpoints::setStackReadWriteBP
 * @param index
 * @param inUse
 */
void HardwareBreakpoints::setStackReadWriteBP(int index, bool inUse) {
	const edb::address_t address = edb::v1::selected_stack_address();
	const size_t size            = edb::v1::selected_stack_size();
	setReadWriteBP(index, inUse, address, size);
}

/**
 * @brief HardwareBreakpoints::setStackWriteBP
 * @param index
 * @param inUse
 */
void HardwareBreakpoints::setStackWriteBP(int index, bool inUse) {
	const edb::address_t address = edb::v1::selected_stack_address();
	const size_t size            = edb::v1::selected_stack_size();
	setWriteBP(index, inUse, address, size);
}

/**
 * @brief HardwareBreakpoints::setCPUReadWriteBP
 * @param index
 * @param inUse
 */
void HardwareBreakpoints::setCPUReadWriteBP(int index, bool inUse) {
	const edb::address_t address = edb::v1::cpu_selected_address();
	constexpr size_t Size        = 1;
	setWriteBP(index, inUse, address, Size);
}

/**
 * @brief HardwareBreakpoints::setCPUWriteBP
 * @param index
 * @param inUse
 */
void HardwareBreakpoints::setCPUWriteBP(int index, bool inUse) {
	const edb::address_t address = edb::v1::cpu_selected_address();
	constexpr size_t Size        = 1;
	setWriteBP(index, inUse, address, Size);
}

/**
 * @brief HardwareBreakpoints::setExec1
 */
void HardwareBreakpoints::setExec1() {
	setExec(Register1);
}

/**
 * @brief HardwareBreakpoints::setExec2
 */
void HardwareBreakpoints::setExec2() {
	setExec(Register2);
}

/**
 * @brief HardwareBreakpoints::setExec3
 */
void HardwareBreakpoints::setExec3() {
	setExec(Register3);
}

/**
 * @brief HardwareBreakpoints::setExec4
 */
void HardwareBreakpoints::setExec4() {
	setExec(Register4);
}

/**
 * @brief HardwareBreakpoints::setAccess1
 */
void HardwareBreakpoints::setAccess1() {
	setAccess(Register1);
}

/**
 * @brief HardwareBreakpoints::setAccess2
 */
void HardwareBreakpoints::setAccess2() {
	setAccess(Register2);
}

/**
 * @brief HardwareBreakpoints::setAccess3
 */
void HardwareBreakpoints::setAccess3() {
	setAccess(Register3);
}

/**
 * @brief HardwareBreakpoints::setAccess4
 */
void HardwareBreakpoints::setAccess4() {
	setAccess(Register4);
}

/**
 * @brief HardwareBreakpoints::setWrite1
 */
void HardwareBreakpoints::setWrite1() {
	setWrite(Register1);
}

/**
 * @brief HardwareBreakpoints::setWrite2
 */
void HardwareBreakpoints::setWrite2() {
	setWrite(Register2);
}

/**
 * @brief HardwareBreakpoints::setWrite3
 */
void HardwareBreakpoints::setWrite3() {
	setWrite(Register3);
}

/**
 * @brief HardwareBreakpoints::setWrite4
 */
void HardwareBreakpoints::setWrite4() {
	setWrite(Register4);
}

}

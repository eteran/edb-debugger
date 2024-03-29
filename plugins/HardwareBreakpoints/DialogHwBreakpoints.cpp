/*
Copyright (C) 2006 - 2023 Evan Teran
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

#include "DialogHwBreakpoints.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "IThread.h"
#include "State.h"
#include "edb.h"
#include "libHardwareBreakpoints.h"

namespace HardwareBreakpointsPlugin {

/**
 * @brief DialogHwBreakpoints::DialogHwBreakpoints
 * @param parent
 * @param f
 */
DialogHwBreakpoints::DialogHwBreakpoints(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);

	connect(ui.cmbType1, SIGNAL(currentIndexChanged(int)), this, SLOT(type1IndexChanged(int)));
	connect(ui.cmbType2, SIGNAL(currentIndexChanged(int)), this, SLOT(type2IndexChanged(int)));
	connect(ui.cmbType3, SIGNAL(currentIndexChanged(int)), this, SLOT(type3IndexChanged(int)));
	connect(ui.cmbType4, SIGNAL(currentIndexChanged(int)), this, SLOT(type4IndexChanged(int)));
}

/**
 * @brief DialogHwBreakpoints::type1IndexChanged
 * @param index
 */
void DialogHwBreakpoints::type1IndexChanged(int index) {
	ui.cmbSize1->setEnabled(index != 0);
}

/**
 * @brief DialogHwBreakpoints::type2IndexChanged
 * @param index
 */
void DialogHwBreakpoints::type2IndexChanged(int index) {
	ui.cmbSize2->setEnabled(index != 0);
}

/**
 * @brief DialogHwBreakpoints::type3IndexChanged
 * @param index
 */
void DialogHwBreakpoints::type3IndexChanged(int index) {
	ui.cmbSize3->setEnabled(index != 0);
}

/**
 * @brief DialogHwBreakpoints::type4IndexChanged
 * @param index
 */
void DialogHwBreakpoints::type4IndexChanged(int index) {
	ui.cmbSize4->setEnabled(index != 0);
}

/**
 * @brief DialogHwBreakpoints::showEvent
 * @param event
 */
void DialogHwBreakpoints::showEvent(QShowEvent *event) {
	Q_UNUSED(event)

	if (IProcess *process = edb::v1::debugger_core->process()) {

		State state;
		process->currentThread()->getState(&state);

		const BreakpointState bp_state1 = breakpoint_state(&state, Register1);
		const BreakpointState bp_state2 = breakpoint_state(&state, Register2);
		const BreakpointState bp_state3 = breakpoint_state(&state, Register3);
		const BreakpointState bp_state4 = breakpoint_state(&state, Register4);

		ui.chkBP1->setChecked(bp_state1.enabled);
		ui.chkBP2->setChecked(bp_state2.enabled);
		ui.chkBP3->setChecked(bp_state3.enabled);
		ui.chkBP4->setChecked(bp_state4.enabled);

		if (bp_state1.enabled) {
			ui.txtBP1->setText(bp_state1.addr.toPointerString());
			ui.cmbSize1->setCurrentIndex(bp_state1.size);
			ui.cmbType1->setCurrentIndex(bp_state1.type);
		}

		if (bp_state2.enabled) {
			ui.txtBP2->setText(bp_state2.addr.toPointerString());
			ui.cmbSize2->setCurrentIndex(bp_state2.size);
			ui.cmbType2->setCurrentIndex(bp_state2.type);
		}

		if (bp_state3.enabled) {
			ui.txtBP3->setText(bp_state3.addr.toPointerString());
			ui.cmbSize3->setCurrentIndex(bp_state3.size);
			ui.cmbType3->setCurrentIndex(bp_state3.type);
		}

		if (bp_state4.enabled) {
			ui.txtBP4->setText(bp_state4.addr.toPointerString());
			ui.cmbSize4->setCurrentIndex(bp_state4.size);
			ui.cmbType4->setCurrentIndex(bp_state4.type);
		}
	}
}

}

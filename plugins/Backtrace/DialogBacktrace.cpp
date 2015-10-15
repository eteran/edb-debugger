/*
Copyright (C) 2015	Armen Boursalian
					aboursalian@gmail.com

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

#include "DialogBacktrace.h"
#include "CallStack.h"
#include "Expression.h"
#include "IBreakpoint.h"
#include "IDebugger.h"
#include "ui_DialogBacktrace.h"

#include <QTableWidget>
#include <QMessageBox>

//Default values in the table
#define FIRST_ROW 0
#define CALLER_COLUMN 0
#define RETURN_COLUMN 1

//------------------------------------------------------------------------------
// Name: DialogBacktrace
// Desc: Initializes the Dialog with its QTableWidget.  This class over all is
//			designed to analyze the stack for return addresses to show the user
//			the runtime call stack.  The user can double-click addresses to go
//			to them in the CPU Disassembly view or click "Run To Return" on
//			return addresses to return to those addresses.  The first item on
//			the stack should be the current RIP/PC, and "Run To Return" should
//			do a "Step Out" (the behavior for the 1st row should be different
//			than all others.
//------------------------------------------------------------------------------
DialogBacktrace::DialogBacktrace(QWidget *parent) : QDialog(parent), ui(new Ui::DialogBacktrace) {
	ui->setupUi(this);
	table_ = ui->tableWidgetCallStack;
}

DialogBacktrace::~DialogBacktrace() {
	delete ui;
}

//------------------------------------------------------------------------------
// Name: showEvent
// Desc: Ensures the column sizes are correct, connects the sig/slot for
//			syncing with the Debugger UI, then populates the Call Stack table.
//------------------------------------------------------------------------------
void DialogBacktrace::showEvent(QShowEvent *) {
	resizeEvent(NULL);

	//Sync with the Debugger UI.
	connect(edb::v1::debugger_ui, SIGNAL(gui_updated()),
			this, SLOT(populate_table()));

	//Populate the tabel with our call stack info.
	populate_table();
}

//------------------------------------------------------------------------------
// Name: resizeEvent
// Desc: Stretches columns to the table width so that they get equal size.
//------------------------------------------------------------------------------
void DialogBacktrace::resizeEvent(QResizeEvent *) {
	//Because on first open of plugin, there is a Resize followed by Show.
	//We want the opposite, so block the first Resize and let Show call this.
	static bool first_time = true;
	if (first_time) {
		first_time = false;
		return;
	}

	table_->horizontalHeader()->resizeSections(QHeaderView::Stretch);
}

//------------------------------------------------------------------------------
// Name: populate_table
// Desc: Populates the Call Stack table with stack frame entries.
//------------------------------------------------------------------------------
//TODO: The first row should break protocol and display the current RIP/PC.
//		It should be treated specially on "Run To Return" and do a "Step Out"
void DialogBacktrace::populate_table() {

	//Remove rows of the table (clearing does not remove rows)
	//Yes, we depend on i going negative.
	for (int i = table_->rowCount() - 1; i >= 0; i--) {
		table_->removeRow(i); }

	//Get the call stack and populate the table with entries.
	CallStack call_stack;
	const int size = call_stack.size();
	for (int i = 0; i < size; i++) {

		//Create the row to insert info
		table_->insertRow(i);

		//Get the stack frame so that we can insert its info
		CallStack::stack_frame *frame = call_stack[i];

		//Get the caller & ret addresses and put them in the table
		QList<edb::address_t> stack_entry;
		edb::address_t caller = frame->caller;
		edb::address_t ret = frame->ret;
		stack_entry.append(caller);
		stack_entry.append(ret);

		//Put them in the table: create string from address and set item flags.
		for (int j = 0; j < stack_entry.size() && j < table_->columnCount(); j++) {

			//Turn the address into a string prefixed with "0x"
			int base = 16;
			auto item = new QTableWidgetItem;
			item->setText(QString("0x") + QString::number(stack_entry.at(j), base));

			//Remove all flags (namely Edit), then put the flags that we want.
			Qt::ItemFlags flags = Qt::NoItemFlags;
			flags |= Qt::ItemIsEnabled | Qt::ItemIsSelectable;
			item->setFlags(flags);

			table_->setItem(i, j, item);
		}
	}

	//1st ret is selected on every refresh so that we can just click "Return To"
	//Turn Run To button off if no item.
	QTableWidgetItem *item = table_->item(FIRST_ROW, RETURN_COLUMN);
	if (item) {
		table_->setCurrentItem(item);
		ui->pushButtonReturnTo->setEnabled(true);
	} else {
		ui->pushButtonReturnTo->setDisabled(true);
	}
}

//------------------------------------------------------------------------------
// Name: hideEvent
// Desc: Disconnects the signal/slot when the dialog goes away so that
//			populate_table() is not called unnecessarily.
//------------------------------------------------------------------------------
void DialogBacktrace::hideEvent(QHideEvent *) {
	disconnect(edb::v1::debugger_ui, SIGNAL(gui_updated()),
			   this, SLOT(populate_table()));
}

//------------------------------------------------------------------------------
// Name: on_pushButtonClose_clicked()
// Desc: Triggered when the dialog is closed which signals hideEvent & closeEvent.
//------------------------------------------------------------------------------
void DialogBacktrace::on_pushButtonClose_clicked() {
	close();
}

//------------------------------------------------------------------------------
// Name: on_tableWidgetCallStack_itemDoubleClicked
// Desc: Jumps to the double-clicked address in the CPU/Disassembly view.
//------------------------------------------------------------------------------
void DialogBacktrace::on_tableWidgetCallStack_itemDoubleClicked(QTableWidgetItem *item) {
	bool ok;
	edb::address_t address = address_from_table(&ok, item);
	if (ok) {
		edb::v1::jump_to_address(address);
	}
}

//------------------------------------------------------------------------------
// Name: on_tableWidgetCallStack_cellClicked
// Desc: Enables the "Run To Return" button if the selected cell is in the
//			column for return addresses.  Disables it, otherwise.
//------------------------------------------------------------------------------
void DialogBacktrace::on_tableWidgetCallStack_cellClicked(int row, int column)
{
	row = row;	//Not used, and the warning is annoying.

	QPushButton *return_to = ui->pushButtonReturnTo;
	if (is_ret(column)) {
		return_to->setEnabled(true);
	} else {
		return_to->setDisabled(true);
	}
}

//------------------------------------------------------------------------------
// Name: on_pushButtonReturnTo_clicked()
// Desc: Ensures that the selected item is a return address.  If so, sets a
//			breakpoint at that address and continues execution.
//------------------------------------------------------------------------------
void DialogBacktrace::on_pushButtonReturnTo_clicked()
{
	//Make sure our current item is in the RETURN_COLUMN
	QTableWidgetItem *item = table_->currentItem();
	if (!is_ret(item)) { return; }

	bool ok;
	edb::address_t address = address_from_table(&ok, item);

	//If we didn't get a valid address, then fail.
	//TODO: Make sure "ok" actually signifies success of getting an address...
	if (!ok) {
		int base = 16;
		QString msg("Could not return to 0x%x" + QString::number(address, base));
		QMessageBox::information( this,	"Error", msg);
		return;
	}
	
	if(IProcess *process = edb::v1::debugger_core->process()) {

		//Now that we got the address, we can run.  First check if bp @ that address
		//already exists.
		IBreakpoint::pointer bp = edb::v1::debugger_core->find_breakpoint(address);
		if (bp) {
			process->resume(edb::DEBUG_CONTINUE);
			return;
		}

		//Using the non-debugger_core version ensures bp is set in a valid region
		edb::v1::create_breakpoint(address);
		bp = edb::v1::debugger_core->find_breakpoint(address);
		if (bp) {
			bp->set_internal(true);
			bp->set_one_time(true);
			process->resume(edb::DEBUG_CONTINUE);
			return;
		}
	}


}

//------------------------------------------------------------------------------
// Name: is_ret (QTableWidgetItem version)
// Desc: Returns true if the selected item is in the column for return addresses.
//			Returns false otherwise.
//------------------------------------------------------------------------------
bool DialogBacktrace::is_ret(const QTableWidgetItem *item) {
	if (!item) { return false; }
	return item->column() == RETURN_COLUMN;
}

//------------------------------------------------------------------------------
// Name: is_ret (column number version)
// Desc: Returns true if the column number is the one dedicated to return addresses.
//			Returns false otherwise.
//------------------------------------------------------------------------------
bool DialogBacktrace::is_ret(int column) {
	return column == RETURN_COLUMN;
}

//------------------------------------------------------------------------------
// Name: address_from_table
// Desc: Returns the edb::address_t represented by the given *item and sets *ok
//			to true if successful or false, otherwise.
//------------------------------------------------------------------------------
edb::address_t DialogBacktrace::address_from_table(bool *ok, const QTableWidgetItem *item) {
	QString addr_text = item->text();
	Expression<edb::address_t> expr(addr_text, edb::v1::get_variable, edb::v1::get_value);
	ExpressionError err;
	const edb::address_t address = expr.evaluate_expression(ok, &err);
	return address;
}

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
#include "IBreakpoint.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "ISymbolManager.h"
#include "Symbol.h"

#include <QPushButton>
#include <QTableWidget>

namespace BacktracePlugin {
namespace {

// Default values in the table
constexpr int FirstRow     = 0;
constexpr int ReturnColumn = 1;

/**
 * @brief address_from_table
 * @param item
 * @return the edb::address_t represented by the given *item and sets *ok to
 * true if successful or false, otherwise.
 */
edb::address_t address_from_table(const QTableWidgetItem *item) {
	return static_cast<edb::address_t>(item->data(Qt::UserRole).value<qulonglong>());
}

/**
 * @brief is_ret
 * @param column
 * @return true if the column number is the one dedicated to return addresses.
 * otherwise, false.
 */
bool is_ret(int column) {
	return column == ReturnColumn;
}

/**
 * @brief is_ret
 * @param item
 * @return true if the selected item is in the column for return addresses.
 * otherwise, false.
 */
bool is_ret(const QTableWidgetItem *item) {
	if (!item) {
		return false;
	}

	return is_ret(item->column());
}

}

/**
 * @brief DialogBacktrace::DialogBacktrace
 *
 * Initializes the Dialog with its QTableWidget.  This class over all is
 * designed to analyze the stack for return addresses to show the user the
 * runtime call stack.  The user can double-click addresses to go to them in
 * the CPU Disassembly view or click "Run To Return" on return addresses to
 * return to those addresses.  The first item on the stack should be the
 * current RIP/PC, and "Run To Return" should do a "Step Out"
 * (the behavior for the 1st row should be different than all others.
 *
 * @param parent
 * @param f
 */
DialogBacktrace::DialogBacktrace(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);

	table_ = ui.tableWidgetCallStack;
	table_->verticalHeader()->hide();
	table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	buttonReturnTo_ = new QPushButton(QIcon::fromTheme("edit-undo"), tr("Return To"));
	connect(buttonReturnTo_, &QPushButton::clicked, this, [this]() {
		// Desc: Ensures that the selected item is a return address.  If so, sets a
		//       breakpoint at that address and continues execution.

		//Make sure our current item is in the RETURN_COLUMN
		QTableWidgetItem *item = table_->currentItem();
		if (!is_ret(item)) {
			return;
		}

		edb::address_t address = address_from_table(item);

		if (IProcess *process = edb::v1::debugger_core->process()) {

			// Now that we got the address, we can run.  First check if bp @ that address
			// already exists.
			if (std::shared_ptr<IBreakpoint> bp = edb::v1::debugger_core->findBreakpoint(address)) {
				process->resume(edb::DEBUG_CONTINUE);
				return;
			}

			// Using the non-debugger_core version ensures bp is set in a valid region
			// TODO(eteran): I think it's safe to just use the return value of create_breakpoint here...
			edb::v1::create_breakpoint(address);
			if (std::shared_ptr<IBreakpoint> bp = edb::v1::debugger_core->findBreakpoint(address)) {
				bp->setInternal(true);
				bp->setOneTime(true);
				process->resume(edb::DEBUG_CONTINUE);
			}
		}
	});

	ui.buttonBox->addButton(buttonReturnTo_, QDialogButtonBox::ActionRole);
}

/**
 * @brief DialogBacktrace::showEvent
 *
 * Ensures the column sizes are correct, connects the sig/slot for syncing with
 * the Debugger UI, then populates the Call Stack table.
 *
 */
void DialogBacktrace::showEvent(QShowEvent *) {

	// Sync with the Debugger UI.
	connect(edb::v1::debugger_ui, SIGNAL(uiUpdated()), this, SLOT(populateTable()));

	// Populate the tabel with our call stack info.
	populateTable();

	table_->horizontalHeader()->resizeSections(QHeaderView::Stretch);
}

/**
 * @brief DialogBacktrace::populateTable
 *
 * Populates the Call Stack table with stack frame entries.
 *
 */
void DialogBacktrace::populateTable() {

	//TODO: The first row should break protocol and display the current RIP/PC.
	//		It should be treated specially on "Run To Return" and do a "Step Out"

	//Remove rows of the table (clearing does not remove rows)
	//Yes, we depend on i going negative.
	for (int i = table_->rowCount() - 1; i >= 0; i--) {
		table_->removeRow(i);
	}

	//Get the call stack and populate the table with entries.
	CallStack call_stack;
	const size_t size = call_stack.size();
	for (size_t i = 0; i < size; i++) {

		//Create the row to insert info
		table_->insertRow(i);

		//Get the stack frame so that we can insert its info
		CallStack::StackFrame *frame = call_stack[i];

		//Get the caller & ret addresses and put them in the table
		QList<edb::address_t> stack_entry;
		edb::address_t caller = frame->caller;
		edb::address_t ret    = frame->ret;
		stack_entry.append(caller);
		stack_entry.append(ret);

		//Put them in the table: create string from address and set item flags.
		for (int j = 0; j < stack_entry.size() && j < table_->columnCount(); j++) {

			edb::address_t address              = stack_entry.at(j);
			std::shared_ptr<Symbol> near_symbol = edb::v1::symbol_manager().findNearSymbol(address);

			//Turn the address into a string prefixed with "0x"
			auto item = new QTableWidgetItem;
			item->setData(Qt::UserRole, static_cast<qlonglong>(address));

			if (near_symbol) {
				const QString function = near_symbol->name;
				const uint64_t offset  = address - near_symbol->address;
				item->setText(tr("0x%1 <%2+%3>").arg(QString::number(address, 16), function).arg(offset));
			} else {
				item->setText(tr("0x%1").arg(QString::number(address, 16)));
			}

			//Remove all flags (namely Edit), then put the flags that we want.
			Qt::ItemFlags flags = Qt::NoItemFlags;
			flags |= Qt::ItemIsEnabled | Qt::ItemIsSelectable;
			item->setFlags(flags);

			table_->setItem(i, j, item);
		}
	}

	//1st ret is selected on every refresh so that we can just click "Return To"
	//Turn Run To button off if no item.
	QTableWidgetItem *item = table_->item(FirstRow, ReturnColumn);
	if (item) {
		table_->setCurrentItem(item);
		buttonReturnTo_->setEnabled(true);
	} else {
		buttonReturnTo_->setEnabled(false);
	}
}

/**
 * @brief DialogBacktrace::hideEvent
 *
 * Disconnects the signal/slot when the dialog goes away so that
 * populate_table() is not called unnecessarily.
 *
 */
void DialogBacktrace::hideEvent(QHideEvent *) {
	disconnect(edb::v1::debugger_ui, SIGNAL(uiUpdated()), this, SLOT(populateTable()));
}

/**
 * @brief DialogBacktrace::on_tableWidgetCallStack_itemDoubleClicked
 *
 * Jumps to the double-clicked address in the CPU/Disassembly view.
 *
 * @param item
 */
void DialogBacktrace::on_tableWidgetCallStack_itemDoubleClicked(QTableWidgetItem *item) {
	edb::v1::jump_to_address(address_from_table(item));
}

/**
 * @brief DialogBacktrace::on_tableWidgetCallStack_cellClicked
 *
 * Enables the "Run To Return" button if the selected cell is in the column for
 * return addresses.  Disables it, otherwise.
 *
 * @param row
 * @param column
 */
void DialogBacktrace::on_tableWidgetCallStack_cellClicked(int row, int column) {
	Q_UNUSED(row)
	if (is_ret(column)) {
		buttonReturnTo_->setEnabled(true);
	} else {
		buttonReturnTo_->setEnabled(false);
	}
}

}

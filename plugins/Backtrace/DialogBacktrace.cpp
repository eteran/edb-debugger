/*
 * Copyright (C) 2015 Armen Boursalian <aboursalian@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
 * @brief Extracts the edb::address_t stored in the UserRole data of the given table widget item.
 *
 * @param item
 * @return the edb::address_t represented by the given *item and sets *ok to
 * true if successful or false, otherwise.
 */
edb::address_t address_from_table(const QTableWidgetItem *item) {
	static_assert(sizeof(qulonglong) >= sizeof(edb::address_t));
	return static_cast<edb::address_t>(item->data(Qt::UserRole).value<qulonglong>());
}

/**
 * @brief Returns true if the given column number corresponds to the return address column.
 *
 * @param column
 * @return true if the column number is the one dedicated to return addresses.
 * otherwise, false.
 */
bool is_ret(int column) {
	return column == ReturnColumn;
}

/**
 * @brief Returns true if the given table widget item resides in the return address column.
 *
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
 * @brief Constructs the call stack backtrace dialog, setting up its table widget and "Return To" button.
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

		// Make sure our current item is in the RETURN_COLUMN
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
			if (std::shared_ptr<IBreakpoint> bp = edb::v1::create_breakpoint(address)) {
				bp->setInternal(true);
				bp->setOneTime(true);
				process->resume(edb::DEBUG_CONTINUE);
			}
		}
	});

	ui.buttonBox->addButton(buttonReturnTo_, QDialogButtonBox::ActionRole);
}

/**
 * @brief Connects the UI update signal and populates the call stack table when the dialog is shown.
 */
void DialogBacktrace::showEvent(QShowEvent *) {

	// Sync with the Debugger UI.
	connect(edb::v1::debugger_ui, SIGNAL(uiUpdated()), this, SLOT(populateTable()));

	// Populate the table with our call stack info.
	populateTable();

	table_->horizontalHeader()->resizeSections(QHeaderView::Stretch);
}

/**
 * @brief Clears and repopulates the call stack table with current stack frame entries.
 */
void DialogBacktrace::populateTable() {

	// TODO: The first row should break protocol and display the current RIP/PC.
	//		It should be treated specially on "Run To Return" and do a "Step Out"

	// Remove rows of the table (clearing does not remove rows)
	// Yes, we depend on i going negative.
	for (int i = table_->rowCount() - 1; i >= 0; i--) {
		table_->removeRow(i);
	}

	// Get the call stack and populate the table with entries.
	CallStack call_stack;
	const size_t size = call_stack.size();
	for (size_t i = 0; i < size; i++) {

		// Create the row to insert info
		table_->insertRow(static_cast<int>(i));

		// Get the stack frame so that we can insert its info
		CallStack::StackFrame *frame = call_stack[i];

		// Get the caller & ret addresses and put them in the table
		QList<edb::address_t> stack_entry;
		edb::address_t caller = frame->caller;
		edb::address_t ret    = frame->ret;
		stack_entry.append(caller);
		stack_entry.append(ret);

		// Put them in the table: create string from address and set item flags.
		for (int j = 0; j < stack_entry.size() && j < table_->columnCount(); j++) {

			edb::address_t address            = stack_entry.at(j);
			std::optional<Symbol> near_symbol = edb::v1::symbol_manager().findNearSymbol(address);

			// Turn the address into a string prefixed with "0x"
			auto item = new QTableWidgetItem;
			item->setData(Qt::UserRole, static_cast<qlonglong>(address));

			if (near_symbol) {
				const QString function = near_symbol->name;
				const uint64_t offset  = address - near_symbol->address;
				item->setText(tr("0x%1 <%2+%3>").arg(QString::number(address, 16), function).arg(offset));
			} else {
				item->setText(tr("0x%1").arg(QString::number(address, 16)));
			}

			// Remove all flags (namely Edit), then put the flags that we want.
			Qt::ItemFlags flags = Qt::NoItemFlags;
			flags |= Qt::ItemIsEnabled | Qt::ItemIsSelectable;
			item->setFlags(flags);

			table_->setItem(static_cast<int>(i), j, item);
		}
	}

	// 1st ret is selected on every refresh so that we can just click "Return To"
	// Turn Run To button off if no item.
	if (QTableWidgetItem *item = table_->item(FirstRow, ReturnColumn)) {
		table_->setCurrentItem(item);
		buttonReturnTo_->setEnabled(true);
	} else {
		buttonReturnTo_->setEnabled(false);
	}
}

/**
 * @brief Disconnects the UI update signal when the dialog is hidden to avoid unnecessary table rebuilds.
 */
void DialogBacktrace::hideEvent(QHideEvent *) {
	disconnect(edb::v1::debugger_ui, SIGNAL(uiUpdated()), this, SLOT(populateTable()));
}

/**
 * @brief Jumps the disassembly view to the address of the double-clicked call stack table entry.
 *
 * @param item
 */
void DialogBacktrace::on_tableWidgetCallStack_itemDoubleClicked(QTableWidgetItem *item) {
	edb::v1::jump_to_address(address_from_table(item));
}

/**
 * @brief Enables or disables the "Return To" button based on whether the clicked cell is a return address column.
 *
 * @param row
 * @param column
 */
void DialogBacktrace::on_tableWidgetCallStack_cellClicked(int row, int column) {
	Q_UNUSED(row)
	buttonReturnTo_->setEnabled(is_ret(column));
}

}

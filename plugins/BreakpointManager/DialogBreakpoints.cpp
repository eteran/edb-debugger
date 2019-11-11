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

#include "DialogBreakpoints.h"
#include "Expression.h"
#include "IBreakpoint.h"
#include "IDebugger.h"
#include "MemoryRegions.h"
#include "edb.h"

#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QStringList>
#include <QTextStream>

namespace BreakpointManagerPlugin {

/**
 * @brief DialogBreakpoints::DialogBreakpoints
 * @param parent
 * @param f
 */
DialogBreakpoints::DialogBreakpoints(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);
	ui.tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

/**
 * @brief DialogBreakpoints::showEvent
 */
void DialogBreakpoints::showEvent(QShowEvent *) {
	connect(edb::v1::disassembly_widget(), SIGNAL(signalUpdated()), this, SLOT(updateList()));
	updateList();
}

/**
 * @brief DialogBreakpoints::hideEvent
 */
void DialogBreakpoints::hideEvent(QHideEvent *) {
	disconnect(edb::v1::disassembly_widget(), SIGNAL(signalUpdated()), this, SLOT(updateList()));
}

/**
 * @brief DialogBreakpoints::updateList
 */
void DialogBreakpoints::updateList() {

	ui.tableWidget->setSortingEnabled(false);
	ui.tableWidget->setRowCount(0);

	const IDebugger::BreakpointList breakpoint_state = edb::v1::debugger_core->backupBreakpoints();

	for (const std::shared_ptr<IBreakpoint> &bp : breakpoint_state) {

		//Skip if it's an internal bp; we don't want to insert a row for it.
		if (bp->internal()) {
			continue;
		}

		const int row = ui.tableWidget->rowCount();
		ui.tableWidget->insertRow(row);

		const edb::address_t address = bp->address();
		const QString condition      = bp->condition;
		const bool onetime           = bp->oneTime();
		const QString symname        = edb::v1::find_function_symbol(address, QString(), nullptr);
		const QString bytes          = edb::v1::format_bytes(bp->originalBytes(), bp->size());

		auto item = new QTableWidgetItem(edb::v1::format_pointer(address));
		item->setData(Qt::UserRole, address.toQVariant());

		ui.tableWidget->setItem(row, 0, item);
		ui.tableWidget->setItem(row, 1, new QTableWidgetItem(condition));
		ui.tableWidget->setItem(row, 2, new QTableWidgetItem(bytes));
		ui.tableWidget->setItem(row, 3, new QTableWidgetItem(onetime ? tr("One Time") : tr("Standard")));
		ui.tableWidget->setItem(row, 4, new QTableWidgetItem(symname));
	}

	ui.tableWidget->setSortingEnabled(true);
}

/**
 * @brief DialogBreakpoints::on_btnAdd_clicked
 */
void DialogBreakpoints::on_btnAdd_clicked() {

	bool ok;
	QString text = QInputDialog::getText(this, tr("Add Breakpoint"), tr("Address:"), QLineEdit::Normal, QString(), &ok);

	if (ok && !text.isEmpty()) {
		Expression<edb::address_t> expr(text, edb::v1::get_variable, edb::v1::get_value);

		const Result<edb::address_t, ExpressionError> address = expr.evaluate();
		if (address) {
			edb::v1::create_breakpoint(*address);
			updateList();

		} else {
			QMessageBox::critical(this, tr("Error In Address Expression!"), address.error().what());
		}
	}
}

/**
 * @brief DialogBreakpoints::on_btnCondition_clicked
 */
void DialogBreakpoints::on_btnCondition_clicked() {
	QList<QTableWidgetItem *> sel = ui.tableWidget->selectedItems();
	if (!sel.empty()) {
		QTableWidgetItem *const item = sel[0];
		bool ok;
		const edb::address_t address = item->data(Qt::UserRole).toULongLong();
		const QString condition      = edb::v1::get_breakpoint_condition(address);
		const QString text           = QInputDialog::getText(this, tr("Set Breakpoint Condition"), tr("Expression:"), QLineEdit::Normal, condition, &ok);
		if (ok) {
			edb::v1::set_breakpoint_condition(address, text);
			updateList();
		}
	}
}

/**
 * @brief DialogBreakpoints::on_btnRemove_clicked
 */
void DialogBreakpoints::on_btnRemove_clicked() {
	QList<QTableWidgetItem *> sel = ui.tableWidget->selectedItems();
	if (!sel.empty()) {
		QTableWidgetItem *const item = sel[0];
		const edb::address_t address = item->data(Qt::UserRole).toULongLong();
		edb::v1::remove_breakpoint(address);
	}
	updateList();
}

/**
 * @brief DialogBreakpoints::on_tableWidget_cellDoubleClicked
 * @param row
 * @param col
 */
void DialogBreakpoints::on_tableWidget_cellDoubleClicked(int row, int col) {
	switch (col) {
	case 0: // address
		if (QTableWidgetItem *const address_item = ui.tableWidget->item(row, 0)) {
			const edb::address_t address = address_item->data(Qt::UserRole).toULongLong();
			edb::v1::jump_to_address(address);
		}
		break;
	case 1: // condition
		if (QTableWidgetItem *const address_item = ui.tableWidget->item(row, 0)) {
			bool ok;
			const edb::address_t address = address_item->data(Qt::UserRole).toULongLong();
			const QString condition      = edb::v1::get_breakpoint_condition(address);
			const QString text           = QInputDialog::getText(this, tr("Set Breakpoint Condition"), tr("Expression:"), QLineEdit::Normal, condition, &ok);
			if (ok) {
				edb::v1::set_breakpoint_condition(address, text);
				updateList();
			}
		}
		break;
	}
}

/**
 * @brief DialogBreakpoints::on_btnImport_clicked
 *
 * Opens a file selection window to choose a file with newline-separated,
 * hex address breakpoints.
 */
void DialogBreakpoints::on_btnImport_clicked() {

	// Let the user choose the file; get the file name.
	QString home_directory = QDir::homePath();
	QString file_name      = QFileDialog::getOpenFileName(this, tr("Breakpoint Import File"), home_directory, nullptr);

	if (file_name.isEmpty()) {
		return;
	}

	// Open the file; fail if error or it doesn't exist.
	QFile file(file_name);
	if (!file.open(QIODevice::ReadOnly)) {
		QMessageBox::critical(this, tr("Error Opening File"), tr("Unable to open breakpoint file: %1").arg(file_name));
		return;
	}

	// Keep a list of any lines in the file that don't make valid breakpoints.
	QStringList errors;

	// Iterate through each line; attempt to make a breakpoint for each line.
	// Addreses should be prefixed with 0x, i.e. a hex number.
	// Count each breakpoint successfully made.
	int count = 0;
	Q_FOREVER {

		// Get the address
		QString line = file.readLine().trimmed();

		if (line.isEmpty()) {
			break;
		}

		bool ok;
		int base               = 16;
		edb::address_t address = line.toULong(&ok, base);

		// Skip if there's an issue.
		if (!ok) {
			errors.append(line);
			continue;
		}

		// If there's an issue with the line or address isn't in any region,
		// add to error list and skip.
		edb::v1::memory_regions().sync();
		std::shared_ptr<IRegion> p = edb::v1::memory_regions().findRegion(address);
		if (!p) {
			errors.append(line);
			continue;
		}

		// If the bp already exists, skip.  No error.
		if (edb::v1::debugger_core->findBreakpoint(address)) {
			continue;
		}

		// If the line was converted to an address, try to create the breakpoint.
		// Access debugger_core directly to avoid many possible error windows by edb::v1::create_breakpoint()
		if (const std::shared_ptr<IBreakpoint> bp = edb::v1::debugger_core->addBreakpoint(address)) {
			count++;
		} else {
			errors.append(line);
		}
	}

	// Report any errors to the user
	if (errors.size() > 0) {
		QMessageBox::warning(this, tr("Invalid Breakpoints"), tr("The following breakpoints were not made:\n%1").arg(errors.join("")));
	}

	// Report breakpoints successfully made
	QMessageBox::information(this, tr("Breakpoint Import"), tr("Imported %1 breakpoints.").arg(count));

	updateList();
}

/**
 * @brief DialogBreakpoints::on_btnExport_clicked
 *
 * Opens a file selection window to choose a file to save newline-separated,
 * hex address breakpoints.
 */
void DialogBreakpoints::on_btnExport_clicked() {

	//Get the current list of breakpoints
	const IDebugger::BreakpointList breakpoint_state = edb::v1::debugger_core->backupBreakpoints();

	//Create a list for addresses to be exported at the end
	QList<edb::address_t> export_list;

	//Go through our breakpoints and add for export if not one-time and not internal.
	for (const std::shared_ptr<IBreakpoint> &bp : breakpoint_state) {
		if (!bp->oneTime() && !bp->internal()) {
			export_list.append(bp->address());
		}
	}

	//If there are no breakpoints, fail
	if (export_list.isEmpty()) {
		QMessageBox::critical(this, tr("No Breakpoints"), tr("There are no breakpoints to export."));
		return;
	}

	//Now ask the user for a file, open it, and write each address to it.
	QString filename = QFileDialog::getSaveFileName(this, tr("Breakpoint Export File"), QDir::homePath());

	if (filename.isEmpty()) {
		return;
	}

	QFile file(filename);

	if (!file.open(QIODevice::WriteOnly)) {
		return;
	}

	for (edb::address_t address : export_list) {
		QString string_address = "0x" + QString::number(address, 16) + "\n";
		file.write(string_address.toLatin1());
	}

	QMessageBox::information(this, tr("Breakpoint Export"), tr("Exported %1 breakpoints").arg(export_list.size()));
}

}

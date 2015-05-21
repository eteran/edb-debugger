/*
Copyright (C) 2006 - 2014 Evan Teran
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

#include "DialogBreakpoints.h"
#include "Expression.h"
#include "IDebuggerCore.h"
#include "edb.h"
#include "MemoryRegions.h"

#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>

#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QStringList>

#include "ui_DialogBreakpoints.h"

namespace BreakpointManager {

//------------------------------------------------------------------------------
// Name: DialogBreakpoints
// Desc:
//------------------------------------------------------------------------------
DialogBreakpoints::DialogBreakpoints(QWidget *parent) : QDialog(parent), ui(new Ui::DialogBreakpoints) {
	ui->setupUi(this);
#if QT_VERSION >= 0x050000
	ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#else
	ui->tableWidget->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
#endif
}

//------------------------------------------------------------------------------
// Name: ~DialogBreakpoints
// Desc:
//------------------------------------------------------------------------------
DialogBreakpoints::~DialogBreakpoints() {
	delete ui;
}

//------------------------------------------------------------------------------
// Name: showEvent
// Desc:
//------------------------------------------------------------------------------
void DialogBreakpoints::showEvent(QShowEvent *) {
	updateList();
}

//------------------------------------------------------------------------------
// Name: updateList
// Desc:
//------------------------------------------------------------------------------
void DialogBreakpoints::updateList() {


	ui->tableWidget->setSortingEnabled(false);
	ui->tableWidget->setRowCount(0);

	const IDebuggerCore::BreakpointList breakpoint_state = edb::v1::debugger_core->backup_breakpoints();

	Q_FOREACH(const IBreakpoint::pointer &bp, breakpoint_state) {
		const int row = ui->tableWidget->rowCount();
		ui->tableWidget->insertRow(row);

		if(!bp->internal()) {

			const edb::address_t address = bp->address();
			const QString condition      = bp->condition;
			const quint8 orig_byte       = bp->original_byte();
			const bool onetime           = bp->one_time();
			const QString symname        = edb::v1::find_function_symbol(address, QString(), 0);
			const QString bytes          = edb::v1::format_bytes(orig_byte);

			QTableWidgetItem *item = new QTableWidgetItem(edb::v1::format_pointer(address));
			item->setData(Qt::UserRole, address);
			
			ui->tableWidget->setItem(row, 0, item);
			ui->tableWidget->setItem(row, 1, new QTableWidgetItem(condition));
			ui->tableWidget->setItem(row, 2, new QTableWidgetItem(bytes));
			ui->tableWidget->setItem(row, 3, new QTableWidgetItem(onetime ? tr("One Time") : tr("Standard")));
			ui->tableWidget->setItem(row, 4, new QTableWidgetItem(symname));
		}
	}

	ui->tableWidget->setSortingEnabled(true);
}

//------------------------------------------------------------------------------
// Name: on_btnAdd_clicked
// Desc:
//------------------------------------------------------------------------------
void DialogBreakpoints::on_btnAdd_clicked() {

	bool ok;
    QString text = QInputDialog::getText(this, tr("Add Breakpoint"), tr("Address:"), QLineEdit::Normal, QString(), &ok);

	if(ok && !text.isEmpty()) {
		Expression<edb::address_t> expr(text, edb::v1::get_variable, edb::v1::get_value);
		ExpressionError err;
		const edb::address_t address = expr.evaluate_expression(&ok, &err);
		if(ok) {
			edb::v1::create_breakpoint(address);
			updateList();

		} else {
			QMessageBox::information(this, tr("Error In Address Expression!"), err.what());
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_btnCondition_clicked
// Desc:
//------------------------------------------------------------------------------
void DialogBreakpoints::on_btnCondition_clicked() {
	QList<QTableWidgetItem *> sel = ui->tableWidget->selectedItems();
	if(!sel.empty()) {
		QTableWidgetItem *const item = sel[0];
		bool ok;
		const edb::address_t address = item->data(Qt::UserRole).toULongLong();
		const QString condition      = edb::v1::get_breakpoint_condition(address);
		const QString text           = QInputDialog::getText(this, tr("Set Breakpoint Condition"), tr("Expression:"), QLineEdit::Normal, condition, &ok);
		if(ok) {
			edb::v1::set_breakpoint_condition(address, text);
			updateList();
		}
	}
}

#if 0
//------------------------------------------------------------------------------
// Name: on_btnAddFunction_clicked
// Desc:
//------------------------------------------------------------------------------
void DialogBreakpoints::on_btnAddFunction_clicked() {
    bool ok;
    const QString text = QInputDialog::getText(this, tr("Add Breakpoint On Library Function"), tr("Function Name:"), QLineEdit::Normal, QString(), &ok);
	if(ok && !text.isEmpty()) {
		const QList<Symbol::pointer> syms = edb::v1::symbol_manager().symbols();
		Q_FOREACH(const Symbol::pointer &current, syms) {
			if(current.name_no_prefix == text) {
				edb::v1::create_breakpoint(current.address);
			}
		}
		updateList();
	}
}
#endif

//------------------------------------------------------------------------------
// Name: on_btnRemove_clicked
// Desc:
//------------------------------------------------------------------------------
void DialogBreakpoints::on_btnRemove_clicked() {
	QList<QTableWidgetItem *> sel = ui->tableWidget->selectedItems();
	if(!sel.empty()) {
		QTableWidgetItem *const item = sel[0];
		const edb::address_t address = item->data(Qt::UserRole).toULongLong();
		edb::v1::remove_breakpoint(address);
	}
	updateList();
}

//------------------------------------------------------------------------------
// Name: on_tableWidget_cellDoubleClicked
// Desc:
//------------------------------------------------------------------------------
void DialogBreakpoints::on_tableWidget_cellDoubleClicked(int row, int col) {
	switch(col) {
	case 0: // address
		if(QTableWidgetItem *const address_item = ui->tableWidget->item(row, 0)) {
			const edb::address_t address = address_item->data(Qt::UserRole).toULongLong();
			edb::v1::jump_to_address(address);
		}
		break;
	case 1: // condition
		if(QTableWidgetItem *const address_item = ui->tableWidget->item(row, 0)) {
			bool ok;
			const edb::address_t address = address_item->data(Qt::UserRole).toULongLong();
			const QString condition      = edb::v1::get_breakpoint_condition(address);
			const QString text           = QInputDialog::getText(this, tr("Set Breakpoint Condition"), tr("Expression:"), QLineEdit::Normal, condition, &ok);
			if(ok) {
				edb::v1::set_breakpoint_condition(address, text);
				updateList();
			}
		}
		break;
	}
}

//------------------------------------------------------------------------------
// Name: on_btnImport_clicked()
// Desc: Opens a file selection window to choose a file with newline-separated,
//          hex address breakpoints.
//------------------------------------------------------------------------------
void DialogBreakpoints::on_btnImport_clicked() {

	//Let the user choose the file; get the file name.
	QString file_name, home_directory;
	home_directory	= QDir::homePath();
	file_name		= QFileDialog::getOpenFileName(this, "Breakpoint Import File", home_directory, NULL);

	if (file_name.isEmpty()) {
		return;	}

	//Open the file; fail if error or it doesn't exist.
	QFile file(file_name);
	if (!file.open(QIODevice::ReadOnly)) {
		QMessageBox::information(this, "Error Opening File", "Unable to open breakpoint file:" + file_name);
		file.close();
		return;
	}

	//Keep a list of any lines in the file that don't make valid breakpoints.
	QStringList errors;

	//Iterate through each line; attempt to make a breakpoint for each line.
	//Addreses should be prefixed with 0x, i.e. a hex number.
	//Count each breakpoint successfully made.
	int count = 0;
	while (!file.atEnd()) {

		//Get the address
		QString line = file.readLine();
		bool ok;
		int base = 16;
		edb::address_t address = line.toULong(&ok, base);

		//Skip if there's an issue.
		if (!ok) {
			errors.append(line);
			continue;
		}

		//If there's an issue with the line or address isn't in any region,
		//add to error list and skip.
		edb::v1::memory_regions().sync();
		IRegion::pointer p = edb::v1::memory_regions().find_region(address);
		if (!p) {;
			errors.append(line);
			continue;
		}

		//If the bp already exists, skip.  No error.
		if (edb::v1::debugger_core->find_breakpoint(address)) {
			continue; }

		//If the line was converted to an address, try to create the breakpoint.
		//Access debugger_core directly to avoid many possible error windows by edb::v1::create_breakpoint()
		const IBreakpoint::pointer bp = edb::v1::debugger_core->add_breakpoint(address);
		if (bp) {
			count++;
		} else{
			errors.append(line);
		}
	}

	//Report any errors to the user
	if (errors.size() > 0) {
		QString msg = "The following breakpoints were not made:\n" + errors.join("");
		QMessageBox::information(this, "Invalid Breakpoints", msg);
	}

	//Report breakpoints successfully made
	QString msg = "Imported ";
	msg += QString().number(count);
	msg += " breakpoints.";

	QMessageBox::information(this, "Breakpoint Import", msg);

	file.close();
	updateList();
}

//------------------------------------------------------------------------------
// Name: on_btnExport_clicked()
// Desc: Opens a file selection window to choose a file to save newline-separated,
//          hex address breakpoints.
//------------------------------------------------------------------------------
void DialogBreakpoints::on_btnExport_clicked() {

	//Get the current list of breakpoints
	const IDebuggerCore::BreakpointList breakpoint_state = edb::v1::debugger_core->backup_breakpoints();

	//Create a list for addresses to be exported at the end
	QList<edb::address_t> export_list;

	//Go through our breakpoints and add for export if not one-time and not internal.
	Q_FOREACH (const IBreakpoint::pointer bp, breakpoint_state) {
		if (!bp->one_time() && !bp->internal()) {
			export_list.append(bp->address()); }
	}

	//Now ask the user for a file, open it, and write each address to it.
	QString filename = QFileDialog::getSaveFileName(this, "Breakpoint Export File", QDir::homePath());

	if (filename.isEmpty()) {
		return; }

	QFile file(filename);

	if (!file.open(QIODevice::WriteOnly)) {
		return; }

	Q_FOREACH (edb::address_t address, export_list) {
		int base = 16;
		QString string_address = "0x" + QString().number(address, base) + "\n";
		file.write(string_address.toAscii());
	}

	file.close();

	QString msg = "Exported ";
	msg += QString().number(export_list.size());
	msg += " breakpoints.";

	QMessageBox::information(this, "Breakpoint Export", msg);
}

}

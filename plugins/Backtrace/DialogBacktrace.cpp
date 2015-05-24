#include "DialogBacktrace.h"
#include "ui_DialogBacktrace.h"
#include "CallStack.h"
#include "Expression.h"
#include "IBreakpoint.h"
#include "IDebuggerCore.h"

#include <QTableWidget>
#include <QMessageBox>

#include <QDebug>

DialogBacktrace::DialogBacktrace(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::DialogBacktrace)
{
	ui->setupUi(this);
	table_ = ui->tableWidgetCallStack;
}

DialogBacktrace::~DialogBacktrace()
{
	delete ui;
}

void DialogBacktrace::showEvent(QShowEvent *) {
	table_->horizontalHeader()->resizeSections(QHeaderView::Stretch);
	qDebug() << "Show event";
	populate_table();
}

void DialogBacktrace::resizeEvent(QResizeEvent *) {
	table_->horizontalHeader()->resizeSections(QHeaderView::Stretch);
	qDebug() << "Resize event";
}

void DialogBacktrace::populate_table() {
	//Remove rows of the table (clearing does not remove rows)
	//Yes, we depend on i going negative.
	//Watch for 1-off. My last implementation used absolute indexing.
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
		edb::address_t caller = frame->caller,
				ret = frame->ret;
		stack_entry.append(caller);
		stack_entry.append(ret);

		//Put them in the table: create string from address and set item flags.
		for (int j = 0; j < stack_entry.size() && j < table_->columnCount(); j++) {

			//Turn the address into a string prefixed with "0x"
			int base = 16;
			QTableWidgetItem *item = new QTableWidgetItem;
			item->setText(QString("0x") + QString().number(stack_entry.at(j), base));

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

void DialogBacktrace::hideEvent(QHideEvent *) {
	qDebug() << "Disconnected from GUI updates (hide)";
}

void DialogBacktrace::on_pushButtonClose_clicked()
{
	//closeEvent() will disconnect from the sig/slot for GUI updates
	close();
}

//When an address is double-clicked, go to it.
void DialogBacktrace::on_tableWidgetCallStack_itemDoubleClicked(QTableWidgetItem *item) {
	bool ok;
	edb::address_t address = address_from_table(&ok, item);
	if (ok) {
		edb::v1::jump_to_address(address);
	}
}

//When an address is selected, make it the "current item" so that we can
//click "Return To".
//Disable "Return To" if it's not an item in RETURN_COLUMN
void DialogBacktrace::on_tableWidgetCallStack_cellClicked(int row, int column)
{
	QPushButton *return_to = ui->pushButtonReturnTo;
	if (is_ret(column)) {
		return_to->setEnabled(true);
	} else {
		return_to->setDisabled(true);
	}
}

//When "Return To" is clicked, check that the item is in RETURN_COLUMN.
//Then set the breakpoint at the address and run.
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
		qDebug() << "Not ok";
		int base = 16;
		QString msg("Could not return to 0x%x" + QString().number(address, base));
		QMessageBox::information( this,	"Error", msg);
		return;
	}

	qDebug() << "Ok";

	//Now that we got the address, we can run.  First check if bp @ that address
	//already exists.
	IBreakpoint::pointer bp = edb::v1::debugger_core->find_breakpoint(address);
	if (bp) {
		edb::v1::debugger_core->resume(edb::DEBUG_CONTINUE);
		return;
	}

	//Using the non-debugger_core version ensures bp is set in a valid region
	edb::v1::create_breakpoint(address);
	bp = edb::v1::debugger_core->find_breakpoint(address);
	if (bp) {
		bp->set_internal(true);
		bp->set_one_time(true);
		edb::v1::debugger_core->resume(edb::DEBUG_CONTINUE);
		return;
	}


}

bool DialogBacktrace::is_ret(const QTableWidgetItem *item) {
	if (!item) { return false; }
	return item->column() == RETURN_COLUMN;
}

bool DialogBacktrace::is_ret(int column) {
	return column == RETURN_COLUMN;
}

edb::address_t DialogBacktrace::address_from_table(bool *ok, const QTableWidgetItem *item) {
	QString addr_text = item->text();
	Expression<edb::address_t> expr(addr_text, edb::v1::get_variable, edb::v1::get_value);
	ExpressionError err;
	const edb::address_t address = expr.evaluate_expression(ok, &err);
	return address;
}

#include "DialogBacktrace.h"
#include "ui_DialogBacktrace.h"
#include "CallStack.h"

#include <QTableWidget>

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
	populate_table();
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
			int base = 16;
			QTableWidgetItem *item = new QTableWidgetItem;
			item->setText(QString("0x") + QString().number(stack_entry.at(j), base));
			Qt::ItemFlags flags = Qt::NoItemFlags;
			flags |= Qt::ItemIsEnabled | Qt::ItemIsSelectable;
			item->setFlags(flags);
			table_->setItem(i, j, item);
		}
	}

	//1st ret is selected on every refresh so that we can just click "Return To"
	QTableWidgetItem *item = table_->item(FIRST_ROW, RETURN_COLUMN);
	if (item) {
		table_->setCurrentItem(item);
	}
}

void DialogBacktrace::closeEvent(QCloseEvent *) {
	//Disconnect from GUI update sig/slot
	qDebug() << "Disconnected from GUI updates";
}

void DialogBacktrace::on_pushButtonClose_clicked()
{
	//closeEvent() will disconnect from the sig/slot for GUI updates
	close();
}

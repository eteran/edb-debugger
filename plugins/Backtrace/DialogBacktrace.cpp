#include "DialogBacktrace.h"
#include "ui_DialogBacktrace.h"

DialogBacktrace::DialogBacktrace(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::DialogBacktrace)
{
	ui->setupUi(this);
}

DialogBacktrace::~DialogBacktrace()
{
	delete ui;
}

#ifndef DIALOGBACKTRACE_H
#define DIALOGBACKTRACE_H

#if QT_VERSION >= 0x050000
#include <QtWidgets/QDialog>
#else
#include <QtGui/QDialog>
#endif

#include "CallStack.h"

#include <QTableWidget>

//Default values in the table
#define FIRST_ROW 0
#define CALLER_COLUMN 0
#define RETURN_COLUMN 1

namespace Ui {
class DialogBacktrace;
}

class DialogBacktrace : public QDialog
{
	Q_OBJECT

public:
	explicit DialogBacktrace(QWidget *parent = 0);
	~DialogBacktrace();

public slots:
	void populate_table();

private slots:
	virtual void showEvent(QShowEvent *);
	virtual void closeEvent(QCloseEvent *);
	void on_pushButtonClose_clicked();

private:
	Ui::DialogBacktrace		*ui;
	QTableWidget			*table_;
};

#endif // DIALOGBACKTRACE_H

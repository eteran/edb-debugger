#ifndef DIALOGBACKTRACE_H
#define DIALOGBACKTRACE_H

#if QT_VERSION >= 0x050000
#include <QtWidgets/QDialog>
#else
#include <QtGui/QDialog>
#endif

namespace Ui {
class DialogBacktrace;
}

class DialogBacktrace : public QDialog
{
	Q_OBJECT

public:
	explicit DialogBacktrace(QWidget *parent = 0);
	~DialogBacktrace();

private:
	Ui::DialogBacktrace *ui;
};

#endif // DIALOGBACKTRACE_H

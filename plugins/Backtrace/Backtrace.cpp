#include "Backtrace.h"
#include "DialogBacktrace.h"
#include "edb.h"

#include <QMenu>
#include <QKeySequence>

#include <QDebug>

namespace Backtrace {

Backtrace::Backtrace() : menu_(0), dialog_(0)
{
	qDebug() << "Backtrace";
}

Backtrace::~Backtrace() {
	delete dialog_;
}

//------------------------------------------------------------------------------
// Name: menu
// Desc: Creates the menu entry.
//------------------------------------------------------------------------------
QMenu *Backtrace::menu(QWidget *parent) {
	qDebug() << "Menu";
	Q_ASSERT(parent);

	if(!menu_) {

		//So it will appear as Plugins > Call Stack > Backtrace in the menu.
		menu_ = new QMenu(tr("Call Stack"), parent);

		//Ctrl + K shortcut, reminiscent of OllyDbg
		menu_->addAction(tr("Backtrace"), this, SLOT(show_menu()), QKeySequence(tr("Ctrl+K")));
	}

	return menu_;
}

//------------------------------------------------------------------------------
// Name: show_menu
// Desc: Shows the window for the plugin
//------------------------------------------------------------------------------
void Backtrace::show_menu() {
	if (!dialog_) {
		dialog_ = new DialogBacktrace(edb::v1::debugger_ui);
	}
	dialog_->show();
}

//Do we need this?  Wasn't included by default; I saw it in other plugins and added it.

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(Backtrace, Backtrace)
#endif


}

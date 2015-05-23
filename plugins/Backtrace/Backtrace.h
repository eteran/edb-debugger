#ifndef BACKTRACE_H
#define BACKTRACE_H

//#include "backtrace_global.h"

#include "IPlugin.h"

class QMenu;
class QDialog;

namespace Backtrace {

class Backtrace : public QObject, public IPlugin
{
	Q_OBJECT
	Q_INTERFACES(IPlugin)
#if QT_VERSION >= 0x050000
	Q_PLUGIN_METADATA(IID, "edb.IPlugin/1.0")
#endif
	Q_CLASSINFO("author", "Armen Boursalian")
	Q_CLASSINFO("url", "https://github.com/Northern-Lights")

public:
	Backtrace();
	virtual ~Backtrace();

public:
	virtual QMenu *menu(QWidget *parent = 0);

public Q_SLOTS:
	void show_menu();

private:
	QMenu	*menu_;
	QDialog	*dialog_;
};

}
#endif // BACKTRACE_H

/*
 * Copyright (C) 2015 Armen Boursalian <aboursalian@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef BACKTRACE_H_20191119_
#define BACKTRACE_H_20191119_

#include "IPlugin.h"

class QMenu;
class QDialog;

namespace BacktracePlugin {

class Backtrace : public QObject, public IPlugin {
	Q_OBJECT
	Q_INTERFACES(IPlugin)
	Q_PLUGIN_METADATA(IID "edb.IPlugin/1.0")
	Q_CLASSINFO("author", "Armen Boursalian")
	Q_CLASSINFO("url", "https://github.com/Northern-Lights")

public:
	explicit Backtrace(QObject *parent = nullptr);
	~Backtrace() override;

public:
	[[nodiscard]] QMenu *menu(QWidget *parent = nullptr) override;

public Q_SLOTS:
	void showMenu();

private:
	QMenu *menu_ = nullptr;
	QPointer<QDialog> dialog_;
};

}
#endif

/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ROPTOOL_H_20100817_
#define ROPTOOL_H_20100817_

#include "IPlugin.h"

class QMenu;
class QDialog;

namespace ROPToolPlugin {

class ROPTool : public QObject, public IPlugin {
	Q_OBJECT
	Q_INTERFACES(IPlugin)
	Q_PLUGIN_METADATA(IID "edb.IPlugin/1.0")
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")

public:
	explicit ROPTool(QObject *parent = nullptr);
	~ROPTool() override;

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

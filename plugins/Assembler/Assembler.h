/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ASSEMBLER_H_20130611_
#define ASSEMBLER_H_20130611_

#include "IPlugin.h"

class QMenu;
class QDialog;

namespace AssemblerPlugin {

class Assembler : public QObject, public IPlugin {
	Q_OBJECT
	Q_INTERFACES(IPlugin)
	Q_PLUGIN_METADATA(IID "edb.IPlugin/1.0")
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")

public:
	explicit Assembler(QObject *parent = nullptr);
	~Assembler() override;

public:
	[[nodiscard]] QMenu *menu(QWidget *parent = nullptr) override;
	[[nodiscard]] QList<QAction *> cpuContextMenu() override;
	[[nodiscard]] QWidget *optionsPage() override;

private:
	void showDialog();

private:
	QPointer<QDialog> dialog_;
};

}

#endif

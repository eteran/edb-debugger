/*
 * Copyright (C) 2016 Ruslan Kabatsayev <b7.10110111@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef INSTRUCTION_INSPECTOR_PLUGIN_H_20160418_
#define INSTRUCTION_INSPECTOR_PLUGIN_H_20160418_

#include "IPlugin.h"
#include "edb.h"
#include <QDialog>
#include <vector>

class QTreeWidget;
class QPushButton;
class QVBoxLayout;
class QTextBrowser;

namespace InstructionInspector {

class Disassembler;

class InstructionDialog : public QDialog {
	Q_OBJECT

public:
	struct DisassemblyFailure {};
	struct InstructionReadFailure {};

public:
	explicit InstructionDialog(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~InstructionDialog() override;

private Q_SLOTS:
	void compareDisassemblers();

private:
	QTreeWidget *tree_           = nullptr;
	QPushButton *buttonCompare_  = nullptr;
	QVBoxLayout *layout_         = nullptr;
	QTextBrowser *disassemblies_ = nullptr;
	void *insn_                  = nullptr;
	Disassembler *disassembler_  = nullptr;
	edb::address_t address_;
	std::vector<std::uint8_t> insnBytes_;
};

class Plugin : public QObject, public IPlugin {
	Q_OBJECT
	Q_INTERFACES(IPlugin)
	Q_PLUGIN_METADATA(IID "edb.IPlugin/1.0")
	Q_CLASSINFO("author", "Ruslan Kabatsayev")
	Q_CLASSINFO("email", "b7.10110111@gmail.com")

public:
	explicit Plugin(QObject *parent = nullptr);
	[[nodiscard]] QMenu *menu(QWidget *parent = nullptr) override;
	[[nodiscard]] QList<QAction *> cpuContextMenu() override;

private:
	void showDialog() const;

private:
	QAction *menuAction_ = nullptr;
};

}

#endif

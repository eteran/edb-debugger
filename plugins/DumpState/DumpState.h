/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DUMP_STATE_H_20061122_
#define DUMP_STATE_H_20061122_

#include "IPlugin.h"
#include "Types.h"

class QMenu;
class State;

namespace DumpStatePlugin {

class DumpState : public QObject, public IPlugin {
	Q_OBJECT
	Q_INTERFACES(IPlugin)
	Q_PLUGIN_METADATA(IID "edb.IPlugin/1.0")
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")

public:
	explicit DumpState(QObject *parent = nullptr);
	~DumpState() override = default;

public:
	[[nodiscard]] QMenu *menu(QWidget *parent = nullptr) override;

public Q_SLOTS:
	void showMenu();

private:
	[[nodiscard]] QWidget *optionsPage() override;

private:
	static void dumpCode(const State &state);
	static void dumpRegisters(const State &state);
	static void dumpStack(const State &state);
	static void dumpData(edb::address_t address);
	static void dumpLines(edb::address_t address, int lines);

private:
	QMenu *menu_ = nullptr;
};

}

#endif

/*
Copyright (C) 2006 - 2015 Evan Teran
                          evan.teran@gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CONFIGURATION_H_20061031_
#define CONFIGURATION_H_20061031_

#include "API.h"
#include "IBreakpoint.h"
#include <QList>
#include <QObject>
#include <QString>

class EDB_EXPORT Configuration : public QObject {
	Q_OBJECT
public:
	Configuration(QObject *parent = nullptr);
	~Configuration() override;

public:
	void sendChangeNotification();

Q_SIGNALS:
	void settingsUpdated();

public:
	enum Syntax {
		Intel,
		Att
	};

	enum CloseBehavior {
		Detach,
		Kill,
		KillIfLaunchedDetachIfAttached
	};

	enum InitialBreakpoint {
		EntryPoint,
		MainSymbol
	};

	enum StartupWindowLocation {
		SystemDefault,
		Centered,
		Restore
	};

public:
	// general tab
	CloseBehavior close_behavior;

	// appearance tab
	bool show_address_separator;
	QString stack_font;
	QString registers_font;
	QString disassembly_font;
	QString data_font;
	bool data_show_address;
	bool data_show_hex;
	bool data_show_ascii;
	bool data_show_comments;
	int data_word_width;
	int data_row_width;
	StartupWindowLocation startup_window_location;
	bool function_offsets_in_hex;
	bool show_jump_arrow;
	QString theme_name;

	// debugging tab
	InitialBreakpoint initial_breakpoint;
	bool warn_on_no_exec_bp;
	bool find_main;
	bool tty_enabled;
	bool remove_stale_symbols;
	bool disableASLR;
	bool disableLazyBinding;
	bool break_on_library_load;
	IBreakpoint::TypeId default_breakpoint_type;
	QString tty_command;

	// disassembly tab
	Syntax syntax;
	bool show_register_badges;
	bool syntax_highlighting_enabled;
	bool zeros_are_filling;
	bool uppercase_disassembly;
	bool small_int_as_decimal;
	bool tab_between_mnemonic_and_operands;
	bool show_local_module_name_in_jump_targets;
	bool show_symbolic_addresses;
	bool simplify_rip_relative_targets;

	// directories tab
	QString symbol_path;
	QString plugin_path;
	QString session_path;

	int min_string_length;

	// Exceptions tab
	bool enable_signals_message_box;
	QList<qlonglong> ignored_exceptions;

protected:
	void readSettings();
	void writeSettings();
};

#endif

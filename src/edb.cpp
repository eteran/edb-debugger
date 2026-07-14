/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "edb.h"
#include "ArchProcessor.h"
#include "BinaryString.h"
#include "Configuration.h"
#include "DebugEventHandlers.h"
#include "Debugger.h"
#include "DebuggerInternal.h"
#include "DialogInputBinaryString.h"
#include "DialogInputValue.h"
#include "DialogOptions.h"
#include "Expression.h"
#include "ExpressionDialog.h"
#include "IBreakpoint.h"
#include "IDebugger.h"
#include "IPlugin.h"
#include "IProcess.h"
#include "IRegion.h"
#include "IThread.h"
#include "MemoryRegions.h"
#include "Module.h"
#include "Prototype.h"
#include "QHexView"
#include "QtHelper.h"
#include "State.h"
#include "Symbol.h"
#include "SymbolManager.h"
#include "version.h"

#include <QAction>
#include <QAtomicPointer>
#include <QByteArray>
#include <QCompleter>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <qplatformdefs.h>

#include <cctype>
#include <optional>

IDebugger *edb::v1::debugger_core = nullptr;
QWidget *edb::v1::debugger_ui     = nullptr;

namespace edb {

Q_DECLARE_NAMESPACE_TR(edb)

namespace {

using BinaryInfoList = QList<IBinary::create_func_ptr_t>;

DebugEventHandlers g_DebugEventHandlers;
QAtomicPointer<IAnalyzer> g_Analyzer = nullptr;
QMap<QString, QObject *> g_GeneralPlugins;
BinaryInfoList g_BinaryInfoList;
CapstoneEDB::Formatter g_Formatter;

QHash<QString, edb::Prototype> g_FunctionDB;

Debugger *ui() {
	return qobject_cast<Debugger *>(edb::v1::debugger_ui);
}

bool function_symbol_base(edb::address_t address, QString *value, int *offset) {

	Q_ASSERT(value);
	Q_ASSERT(offset);

	if (const std::optional<Symbol> s = edb::v1::symbol_manager().findNearSymbol(address)) {
		*value  = s->name;
		*offset = static_cast<int>(address - s->address);
		return true;
	}

	*offset = 0;
	return false;
}
}

namespace internal {

/**
 * @brief
 */
bool register_plugin(const QString &filename, QObject *plugin) {
	if (!g_GeneralPlugins.contains(filename)) {
		g_GeneralPlugins[filename] = plugin;
		return true;
	}

	return false;
}

/**
 * @brief
 */
void load_function_db() {
	QFile file(QStringLiteral(":/debugger/xml/functions.xml"));
	QDomDocument doc;

	if (file.open(QIODevice::ReadOnly)) {
		if (doc.setContent(&file)) {
			QDomElement root     = doc.firstChildElement(QStringLiteral("functions"));
			QDomElement function = root.firstChildElement(QStringLiteral("function"));
			for (; !function.isNull(); function = function.nextSiblingElement(QStringLiteral("function"))) {

				Prototype func;
				func.name     = function.attribute(QStringLiteral("name"));
				func.type     = function.attribute(QStringLiteral("type"));
				func.noreturn = function.attribute(QStringLiteral("noreturn"), QStringLiteral("false")) == QStringLiteral("true");

				QDomElement argument = function.firstChildElement(QStringLiteral("argument"));
				for (; !argument.isNull(); argument = argument.nextSiblingElement(QStringLiteral("argument"))) {

					Argument arg;
					arg.name = argument.attribute(QStringLiteral("name"));
					arg.type = argument.attribute(QStringLiteral("type"));
					func.arguments.push_back(arg);
				}

				g_FunctionDB[func.name] = func;
			}
		}
	}
}

}

namespace v1 {

bool debuggeeIs32Bit() {
	return pointer_size() == sizeof(std::uint32_t);
}

bool debuggeeIs64Bit() {
	return pointer_size() == sizeof(std::uint64_t);
}

/**
 * @brief
 */
void set_cpu_selected_address(address_t address) {
	ui()->cpuView_->setSelectedAddress(address);
	ui()->cpuView_->update();
}

/**
 * @brief
 */
address_t cpu_selected_address() {
	return ui()->cpuView_->selectedAddress();
}

/**
 * @brief
 */
std::shared_ptr<IRegion> current_cpu_view_region() {
	return ui()->cpuView_->region();
}

/**
 * @brief
 */
void repaint_cpu_view() {
	ui()->cpuView_->update();
}

/**
 * @brief
 */
ISymbolManager &symbol_manager() {
	static SymbolManager symbolManager;
	return symbolManager;
}

/**
 * @brief
 */
MemoryRegions &memory_regions() {
	static MemoryRegions memoryRegions;
	return memoryRegions;
}

/**
 * @brief
 */
ArchProcessor &arch_processor() {
	static ArchProcessor archProcessor;
	return archProcessor;
}

/**
 * @brief
 */
IAnalyzer *set_analyzer(IAnalyzer *p) {
	Q_ASSERT(p);
	return g_Analyzer.fetchAndStoreAcquire(p);
}

/**
 * @brief Returns the current active analyzer instance.
 *
 * @return A pointer to the current active analyzer instance, or nullptr if no analyzer is set.
 */
IAnalyzer *analyzer() {
	return g_Analyzer.loadRelaxed();
}

/**
 * @brief Executes all registered debug event handlers for the given debug event.
 *
 * @param e The debug event to execute handlers for.
 * @return The status of the event execution.
 */
EventStatus execute_debug_event_handlers(const std::shared_ptr<IDebugEvent> &e) {
	return g_DebugEventHandlers.execute(e);
}

/**
 * @brief Adds a debug event handler to the list of registered handlers.
 *
 * @param p The debug event handler to add.
 */
void add_debug_event_handler(IDebugEventHandler *p) {
	g_DebugEventHandlers.add(p);
}

/**
 * @brief
 */
void remove_debug_event_handler(IDebugEventHandler *p) {
	g_DebugEventHandlers.remove(p);
}

/**
 * @brief Jumps to a specific address in the debugger UI.
 *
 * @param address The address to jump to.
 * @return true if the jump was successful, false otherwise.
 */
bool jump_to_address(address_t address) {
	Debugger *const gui = ui();
	Q_ASSERT(gui);
	return gui->jumpToAddress(address);
}

/**
 * @brief Shows a given address through a given end address in the data view, optionally in a new tab.
 *
 * @param address The starting address to dump.
 * @param end_address The ending address to dump.
 * @param new_tab If true, opens the dump in a new tab; otherwise, uses the current tab.
 * @return true if the dump was successful, false otherwise.
 */
bool dump_data_range(address_t address, address_t end_address, bool new_tab) {
	Debugger *const gui = ui();
	Q_ASSERT(gui);
	return gui->dumpDataRange(address, end_address, new_tab);
}

/**
 * @brief Shows a given address through a given end address in the data view.
 *
 * @param address The starting address to dump.
 * @param end_address The ending address to dump.
 * @return true if the dump was successful, false otherwise.
 */
bool dump_data_range(address_t address, address_t end_address) {
	return dump_data_range(address, end_address, false);
}

/**
 * @brief Shows a given address in the stack view.
 *
 * @param address The address to dump.
 * @return true if the dump was successful, false otherwise.
 */
bool dump_stack(address_t address) {
	return dump_stack(address, true);
}

/**
 * @brief Shows a given address in the stack view, optionally scrolling to it.
 *
 * @param address The address to dump.
 * @param scroll_to If true, scrolls to the address in the stack view; otherwise, does not scroll.
 * @return true if the dump was successful, false otherwise.
 */
bool dump_stack(address_t address, bool scroll_to) {
	Debugger *const gui = ui();
	Q_ASSERT(gui);
	return gui->dumpStack(address, scroll_to);
}

/**
 * @brief Shows a given address in the data view, optionally in a new tab.
 *
 * @param address The address to dump.
 * @param new_tab If true, opens the dump in a new tab; otherwise, uses the current tab.
 * @return true if the dump was successful, false otherwise.
 */
bool dump_data(address_t address, bool new_tab) {
	Debugger *const gui = ui();
	Q_ASSERT(gui);
	return gui->dumpData(address, new_tab);
}

/**
 * @brief Shows a given address in the data view.
 *
 * @param address The address to dump.
 * @return true if the dump was successful, false otherwise.
 */
bool dump_data(address_t address) {
	return dump_data(address, false);
}

/**
 * @brief Sets the condition for a breakpoint at a given address.
 *
 * @param address The address of the breakpoint.
 * @param condition The condition to set.
 */
void set_breakpoint_condition(address_t address, const QString &condition) {

	if (std::shared_ptr<IBreakpoint> bp = find_breakpoint(address)) {
		bp->condition = condition;
	}
}

/**
 * @brief Returns the condition of a breakpoint at a given address.
 *
 * @param address The address of the breakpoint.
 * @return The condition of the breakpoint, or an empty string if no breakpoint exists at the address.
 */
QString get_breakpoint_condition(address_t address) {
	QString ret;

	if (std::shared_ptr<IBreakpoint> bp = find_breakpoint(address)) {
		ret = bp->condition;
	}

	return ret;
}

/**
 * @brief Creates a breakpoint at the specified address, with user confirmation if necessary.
 *
 * @param address The address at which to create the breakpoint.
 * @return A shared pointer to the created breakpoint, or nullptr if the breakpoint was not created
 */
std::shared_ptr<IBreakpoint> create_breakpoint(address_t address) {

	std::shared_ptr<IBreakpoint> bp;

	memory_regions().sync();
	if (std::shared_ptr<IRegion> region = memory_regions().findRegion(address)) {
		int ret = QMessageBox::Yes;

		if (!region->executable() && config().warn_on_no_exec_bp) {
			ret = QMessageBox::question(
				nullptr,
				tr("Suspicious breakpoint"),
				tr("You want to place a breakpoint in a non-executable region.\n"
				   "An INT3 breakpoint set on data will not execute and may cause incorrect results or crashes.\n"
				   "Do you really want to set a breakpoint here?"),
				QMessageBox::Yes | QMessageBox::No);
		} else {
			uint8_t buffer[Instruction::MaxSize + 1];
			if (const int size = get_instruction_bytes(address, buffer)) {
				Instruction inst(buffer, buffer + size, address);
				if (!inst) {
					ret = QMessageBox::question(
						nullptr,
						tr("Suspicious breakpoint"),
						tr("It looks like you may be setting an INT3 breakpoint on data.\n"
						   "An INT3 breakpoint set on data will not execute and may cause incorrect results or crashes.\n"
						   "Do you really want to set a breakpoint here?"),
						QMessageBox::Yes | QMessageBox::No);
				}
			}
		}

		if (ret == QMessageBox::Yes) {
			bp = debugger_core->addBreakpoint(address);
			if (!bp) {
				QMessageBox::critical(
					nullptr,
					tr("Error Setting Breakpoint"),
					tr("Failed to set breakpoint at address %1").arg(address.toPointerString()));
				return bp;
			}
			repaint_cpu_view();
		}

	} else {
		QMessageBox::critical(
			nullptr,
			tr("Error Setting Breakpoint"),
			tr("Sorry, but setting a breakpoint which is not in a valid region is not allowed."));
	}

	return bp;
}

/**
 * @brief Enables a breakpoint at the specified address.
 *
 * @param address The address of the breakpoint to enable.
 * @return The address of the enabled breakpoint, or 0 if the breakpoint could not be enabled.
 */
address_t enable_breakpoint(address_t address) {
	if (address != 0) {
		std::shared_ptr<IBreakpoint> bp = find_breakpoint(address);
		if (bp && bp->enable()) {
			return address;
		}
	}
	return 0;
}

/**
 * @brief Disables a breakpoint at the specified address.
 *
 * @param address The address of the breakpoint to disable.
 * @return The address of the disabled breakpoint, or 0 if the breakpoint was not found
 */
address_t disable_breakpoint(address_t address) {
	if (address != 0) {
		std::shared_ptr<IBreakpoint> bp = find_breakpoint(address);
		if (bp && bp->disable()) {
			return address;
		}
	}
	return 0;
}

/**
 * @brief Toggles the existence of a breakpoint at a given address.
 *
 * @param address The address at which to toggle the breakpoint.
 */
void toggle_breakpoint(address_t address) {
	if (find_breakpoint(address)) {
		remove_breakpoint(address);
	} else {
		create_breakpoint(address);
	}
}

/**
 * @brief Removes a breakpoint at the specified address.
 *
 * @param address The address of the breakpoint to remove.
 */
void remove_breakpoint(address_t address) {
	debugger_core->removeBreakpoint(address);
	repaint_cpu_view();
}

/**
 * @brief Evaluates an expression and returns the result.
 *
 * @param expression The expression to evaluate.
 * @param value A pointer to the variable where the result will be stored.
 * @return true if the expression was evaluated successfully, false otherwise.
 */
bool eval_expression(const QString &expression, address_t *value) {

	Q_ASSERT(value);

	Expression<address_t> expr(expression, get_variable, get_value);

	const Result<edb::address_t, ExpressionError> address = expr.evaluate();
	if (!address) {
		QMessageBox::critical(debugger_ui, tr("Error In Expression!"), QString::fromLatin1(address.error().what()));
		return false;
	}

	*value = *address;
	return true;
}

/**
 * @brief Gets an expression from the user and evaluates it.
 *
 * @param title The title of the input dialog.
 * @param prompt The prompt for the input dialog.
 * @param value A pointer to the variable where the result will be stored.
 * @return true if the expression was evaluated successfully, false otherwise.
 */
bool get_expression_from_user(const QString &title, const QString &prompt, address_t *value) {

	bool retval      = false;
	auto inputDialog = std::make_unique<ExpressionDialog>(title, prompt, edb::v1::debugger_ui);

	if (inputDialog->exec()) {
		*value = inputDialog->getAddress();
		retval = true;
	}

	return retval;
}

/**
 * @brief Gets a value from the user.
 *
 * @param value A reference to the register where the value will be stored.
 * @return true if the value was obtained successfully, false otherwise.
 */
bool get_value_from_user(Register &value) {
	return get_value_from_user(value, tr("Input Value"));
}

/**
 * @brief Gets a value from the user with a custom title.
 *
 * @param value A reference to the register where the value will be stored.
 * @param title The title of the input dialog.
 * @return true if the value was obtained successfully, false otherwise.
 */
bool get_value_from_user(Register &value, const QString &title) {

	static auto dlg = new DialogInputValue(debugger_ui);
	bool ret        = false;

	dlg->setWindowTitle(title);
	dlg->setValue(value);
	if (dlg->exec() == QDialog::Accepted) {
		value.setScalarValue(dlg->value());
		ret = true;
	}

	return ret;
}

/**
 * @brief Gets a binary string from the user.
 *
 * @param value A reference to the QByteArray where the value will be stored.
 * @param title The title of the input dialog.
 * @param max_length The maximum length of the binary string.
 * @return true if the value was obtained successfully, false otherwise.
 */
bool get_binary_string_from_user(QByteArray &value, const QString &title, int max_length) {

	static auto dlg = new DialogInputBinaryString(debugger_ui);

	bool ret = false;

	dlg->setWindowTitle(title);

	BinaryString *const bs = dlg->binaryString();

	// set the max length BEFORE the value! (or else we truncate incorrectly)
	if (value.length() <= max_length) {
		bs->setMaxLength(max_length);
	}

	bs->setValue(value);

	if (dlg->exec() == QDialog::Accepted) {
		value = bs->value();
		ret   = true;
	}

	return ret;
}

/**
 * @brief Returns a pointer to the options dialog, creating it if it doesn't exist.
 *
 * @return A pointer to the options dialog.
 */
QPointer<QDialog> dialog_options() {
	static QPointer<QDialog> dialog = new DialogOptions(debugger_ui);
	return dialog;
}

/**
 * @brief Returns a reference to the global configuration object.
 *
 * @return A reference to the global configuration object.
 */
Configuration &config() {
	static Configuration g_Configuration;
	return g_Configuration;
}

/**
 * @brief Attempts to create a summary of the content at the specified address, suitable for display in a user interface.
 *
 * @param address The address to examine.
 * @param s A reference to a QString where the summary will be stored.
 * @return true if a human-readable string was found at the address, false otherwise.
 *
 * @note Strings are comprised of printable characters and whitespace.
 * @note The found_length parameter is needed because characters that require an escape
 * sequence may result in a longer resultant string. The found_length represents the
 * original length of the string.
 */
bool get_human_string_at_address(address_t address, QString &s) {
	bool ret = false;
	if (address > 0x10000ULL) { // FIXME use page size
		QString string_param;
		int string_length;

		if (get_ascii_string_at_address(address, string_param, edb::v1::config().min_string_length, 256, string_length)) {
			ret = true;
			s.append(
				QStringLiteral("ASCII \"%1\" ").arg(string_param));
		} else if (get_utf16_string_at_address(address, string_param, edb::v1::config().min_string_length, 256, string_length)) {
			ret = true;
			s.append(
				QStringLiteral("UTF16 \"%1\" ").arg(string_param));
		}
	}
	return ret;
}

/**
 * @brief Attempts to get an ASCII string at a given address whose length is >= min_length and < max_length.
 *
 * @param address The address to read the string from.
 * @param s A reference to a QString where the found string will be stored.
 * @param min_length The minimum length of the string to be considered valid.
 * @param max_length The maximum length of the string to be considered valid.
 * @param found_length A reference to an integer where the original length of the found string will be stored.
 * @return true if a valid ASCII string was found, false otherwise.
 *
 * @note Strings are comprised of printable characters and whitespace.
 * @note The found_length parameter is needed because characters that require an escape
 * sequence may result in a longer resultant string. The found_length represents the
 * original length of the string.
 */
bool get_ascii_string_at_address(address_t address, QString &s, int min_length, int max_length, int &found_length) {

	bool is_string = false;

	if (debugger_core) {
		if (IProcess *process = debugger_core->process()) {
			s.clear();

			if (min_length <= max_length) {
				while (max_length--) {
					char ch;
					if (!process->readBytes(address++, &ch, sizeof(ch))) {
						break;
					}

					const int ascii_char = static_cast<unsigned char>(ch);
					if (ascii_char < 0x80 && (std::isprint(ascii_char) || std::isspace(ascii_char))) {
						s += QChar::fromLatin1(ch);
					} else {
						break;
					}
				}
			}

			is_string = s.length() >= min_length;

			if (is_string) {
				found_length = static_cast<int>(s.length());
				s.replace(QLatin1String("\r"), QLatin1String("\\r"));
				s.replace(QLatin1String("\n"), QLatin1String("\\n"));
				s.replace(QLatin1String("\t"), QLatin1String("\\t"));
				s.replace(QLatin1String("\v"), QLatin1String("\\v"));
				s.replace(QLatin1String("\""), QLatin1String("\\\""));
			}
		}
	}

	return is_string;
}

/**
 * @brief Attempts to get a UTF-16 string at a given address whose length is >= min_length and < max_length.
 *
 * @param address The address to read the string from.
 * @param s A reference to a QString where the found string will be stored.
 * @param min_length The minimum length of the string to be considered valid.
 * @param max_length The maximum length of the string to be considered valid.
 * @param found_length A reference to an integer where the original length of the found string will be stored.
 * @return true if a valid UTF-16 string was found, false otherwise.
 *
 * @note Strings are comprised of printable characters and whitespace.
 * @note The found_length parameter is needed because characters that require an escape
 * sequence may result in a longer resultant string. The found_length represents the
 * original length of the string.
 */
bool get_utf16_string_at_address(address_t address, QString &s, int min_length, int max_length, int &found_length) {
	bool is_string = false;
	if (debugger_core) {
		if (IProcess *process = debugger_core->process()) {
			s.clear();

			if (min_length <= max_length) {
				while (max_length--) {

					uint16_t val;
					if (!process->readBytes(address, &val, sizeof(val))) {
						break;
					}

					address += sizeof(val);

					QChar ch(val);

					// for now, we only acknowledge ASCII chars encoded as unicode
					const int ascii_char = ch.toLatin1();
					if (ascii_char >= 0x20 && ascii_char < 0x80) {
						s += ch;
					} else {
						break;
					}
				}
			}

			is_string = s.length() >= min_length;

			if (is_string) {
				found_length = static_cast<int>(s.length());
				s.replace(QLatin1String("\r"), QLatin1String("\\r"));
				s.replace(QLatin1String("\n"), QLatin1String("\\n"));
				s.replace(QLatin1String("\t"), QLatin1String("\\t"));
				s.replace(QLatin1String("\v"), QLatin1String("\\v"));
				s.replace(QLatin1String("\""), QLatin1String("\\\""));
			}
		}
	}
	return is_string;
}

/**
 * @brief
 */
QString find_function_symbol(address_t address, const QString &default_value, int *offset) {

	QString symname(default_value);
	int off;

	if (function_symbol_base(address, &symname, &off)) {

		if (config().function_offsets_in_hex) {
			symname = QStringLiteral("%1+0x%2").arg(symname).arg(off, 0, 16);
		} else {
			symname = QStringLiteral("%1+%2").arg(symname).arg(off, 0, 10);
		}

		if (offset) {
			*offset = off;
		}
	}

	return symname;
}

/**
 * @brief
 */
QString find_function_symbol(address_t address, const QString &default_value) {
	return find_function_symbol(address, default_value, nullptr);
}

/**
 * @brief
 */
QString find_function_symbol(address_t address) {
	return find_function_symbol(address, QString(), nullptr);
}

/**
 * @brief
 */
address_t get_variable(const QString &s, bool *ok, ExpressionError *err) {

	Q_ASSERT(debugger_core);
	Q_ASSERT(ok);
	Q_ASSERT(err);

	if (IProcess *process = debugger_core->process()) {
		if (std::shared_ptr<IThread> thread = process->currentThread()) {

			State state;
			thread->getState(&state);
			const Register reg = state.value(s);
			*ok                = reg.valid();
			if (!*ok) {
				if (const std::optional<Symbol> sym = edb::v1::symbol_manager().find(s)) {
					*ok = true;
					return sym->address;
				}

				*err = ExpressionError(ExpressionError::UnknownVariable);
				return 0;
			}

			// FIXME: should this really return segment base, not selector?
			// FIXME: if it's really meant to return base, then need to check whether
			//        State::operator[]() returned valid Register
			if (reg.name() == QLatin1String("fs")) {
				return state[QStringLiteral("fs_base")].valueAsAddress();
			}

			if (reg.name() == QLatin1String("gs")) {
				return state[QStringLiteral("gs_base")].valueAsAddress();
			}

			if (reg.bitSize() > 8 * sizeof(edb::address_t)) {
				*err = ExpressionError(ExpressionError::UnknownVariable);
				return 0;
			}

			return reg.valueAsAddress();
		}
	}

	*err = ExpressionError(ExpressionError::UnknownVariable);
	return 0;
}

/**
 * @brief
 */
address_t get_value(address_t address, bool *ok, ExpressionError *err) {

	Q_ASSERT(debugger_core);
	Q_ASSERT(ok);
	Q_ASSERT(err);

	address_t ret = 0;
	*ok           = false;

	if (IProcess *process = edb::v1::debugger_core->process()) {
		*ok = process->readBytes(address, &ret, pointer_size());

		if (!*ok) {
			*err = ExpressionError(ExpressionError::CannotReadMemory);
		}
	}

	return ret;
}

/**
 * @brief Attempts to read instruction bytes from a given address into a buffer.
 *
 * @param address The address to read the instruction bytes from.
 * @param buf A pointer to the buffer where the instruction bytes will be stored.
 * @param size A pointer to an integer representing the maximum number of bytes to read.
 *             On success, this will be updated to the actual number of bytes read.
 * @return true if instruction bytes were successfully read, false otherwise.
 */
bool get_instruction_bytes(address_t address, uint8_t *buf, int *size) {

	Q_ASSERT(debugger_core);
	Q_ASSERT(size);
	Q_ASSERT(*size >= 0);

	if (IProcess *process = edb::v1::debugger_core->process()) {
		*size = static_cast<int>(process->readBytes(address, buf, static_cast<size_t>(*size)));
		if (*size) {
			return true;
		}
	}

	return false;
}

/**
 * @brief Attempts to read instruction bytes from a given address into a buffer.
 *
 * @param address The address to read the instruction bytes from.
 * @param buf A pointer to the buffer where the instruction bytes will be stored.
 * @param size A pointer to an integer representing the maximum number of bytes to read.
 *             On success, this will be updated to the actual number of bytes read.
 * @return true if instruction bytes were successfully read, false otherwise.
 */
bool get_instruction_bytes(address_t address, uint8_t *buf, size_t *size) {

	Q_ASSERT(debugger_core);
	Q_ASSERT(size);

	if (IProcess *process = edb::v1::debugger_core->process()) {
		*size = process->readBytes(address, buf, *size);
		if (*size) {
			return true;
		}
	}

	return false;
}

/**
 * @brief Gets an object which knows how to analyze the binary file provided.
 *
 * @param region The memory region representing the binary file to analyze.
 * @return A unique pointer to an IBinary object that can analyze the binary file, or nullptr if no suitable analyzer was found.
 */
std::unique_ptr<IBinary> get_binary_info(const std::shared_ptr<IRegion> &region) {
	const auto binaryInfoList = g_BinaryInfoList;
	for (IBinary::create_func_ptr_t f : binaryInfoList) {
		try {
			std::unique_ptr<IBinary> p((*f)(region));
			// reorder the list to put this successful plugin
			// in front.
			if (g_BinaryInfoList[0] != f) {
				g_BinaryInfoList.removeOne(f);
				g_BinaryInfoList.push_front(f);
			}
			return p;

		} catch (const std::exception &) {
			// let's just ignore it...
		}
	}

	return nullptr;
}

/**
 * @brief Locates the main function in the debugged process.
 *
 * @return The address of the main function, or 0 if not found.
 *
 * @note This function currently only works for glibc linked ELF files.
 */
address_t locate_main_function() {

	if (debugger_core) {
		if (IProcess *process = debugger_core->process()) {
			const address_t main_func = process->calculateMain();
			if (main_func != 0) {
				return main_func;
			}

			return process->entryPoint();
		}
	}

	return 0;
}

/**
 * @brief Returns a reference to the global list of loaded plugins.
 *
 * @return A QMap containing the loaded plugins, where the key is the plugin filename and the value is a pointer to the plugin object.
 */
const QMap<QString, QObject *> &plugin_list() {
	return g_GeneralPlugins;
}

/**
 * @brief Finds a plugin by its class name.
 *
 * @param name The class name of the plugin to find.
 * @return A pointer to the IPlugin interface of the found plugin, or nullptr if no plugin with the specified class name was found.
 */
IPlugin *find_plugin_by_name(const QString &name) {
	for (QObject *p : g_GeneralPlugins) {
		if (name == QString::fromLocal8Bit(p->metaObject()->className())) {
			return qobject_cast<IPlugin *>(p);
		}
	}
	return nullptr;
}

/**
 * @brief Reloads the symbol information.
 */
void reload_symbols() {
	symbol_manager().clear();
}

/**
 * @brief Gets information about a function.
 *
 * @param function The name of the function.
 * @return A pointer to the function's prototype, or nullptr if not found.
 */
const Prototype *get_function_info(const QString &function) {

	auto it = g_FunctionDB.find(function);
	if (it != g_FunctionDB.end()) {
		return &(it.value());
	}

	return nullptr;
}

/**
 * @brief Returns the primary data region of the main executable module.
 *
 * @return A shared pointer to the primary data region, or nullptr if not found.
 *
 * @note Make sure that memory regions have been synchronized first, or you may get a null-region result.
 */
std::shared_ptr<IRegion> primary_data_region() {

	if (debugger_core) {
		if (IProcess *process = debugger_core->process()) {
			const address_t address = process->dataAddress();
			memory_regions().sync();
			if (std::shared_ptr<IRegion> region = memory_regions().findRegion(address)) {
				return region;
			}
		}
	}

	qDebug() << "primary data region not found!";
	return nullptr;
}

/**
 * @brief Returns the primary code region of the main executable module.
 *
 * @return A shared pointer to the primary code region, or nullptr if not found.
 *
 * @note Make sure that memory regions have been synchronized first, or you may get a null-region result.
 */
std::shared_ptr<IRegion> primary_code_region() {

#if defined(Q_OS_LINUX)
	if (debugger_core) {
		if (IProcess *process = debugger_core->process()) {
			const address_t address = process->codeAddress();
			memory_regions().sync();
			if (std::shared_ptr<IRegion> region = memory_regions().findRegion(address)) {
				return region;
			}
		}
	}
#else
	const QString process_executable = debugger_core->process()->name();

	memory_regions().sync();
	const QList<std::shared_ptr<IRegion>> r = memory_regions().regions();
	for (const std::shared_ptr<IRegion> &region : r) {
		if (region->executable() && region->name() == process_executable) {
			return region;
		}
	}
#endif
	return nullptr;
}

/**
 * @brief Pops a value from the stack of the given state.
 *
 * @param state A pointer to the State object from which to pop the value.
 */
void pop_value(State *state) {
	Q_ASSERT(state);
	state->adjustStack(static_cast<int>(pointer_size()));
}

/**
 * @brief Pushes a value onto the stack of the given state.
 *
 * @param state A pointer to the State object onto which to push the value.
 * @param value The value to push onto the stack.
 */
void push_value(State *state, reg_t value) {
	Q_ASSERT(state);

	if (IProcess *process = edb::v1::debugger_core->process()) {
		state->adjustStack(-static_cast<int>(pointer_size()));
		process->writeBytes(state->stackPointer(), &value, pointer_size());
	}
}

/**
 * @brief Registers a binary information creation function pointer to the global list of binary info creators.
 *
 * @param fptr A function pointer that creates an instance of IBinary for analyzing binary files.
 */
void register_binary_info(IBinary::create_func_ptr_t fptr) {
	if (!g_BinaryInfoList.contains(fptr)) {
		g_BinaryInfoList.push_back(fptr);
	}
}

/**
 * @brief Returns an integer representation of the current EDB version string.
 *
 * @return A 32-bit unsigned integer representing the EDB version,
 * where the major, minor, and revision numbers are packed into the integer.
 */
quint32 edb_version() {
	return int_version(QStringLiteral(EDB_VERSION_STRING));
}

/**
 * @brief Checks if overwriting bytes at a given address would conflict with existing breakpoints.
 *
 * @param address The address of the bytes to be overwritten.
 * @param size The number of bytes to be overwritten.
 * @return True if the overwrite is safe, false otherwise.
 */
bool overwrite_check(address_t address, size_t size) {
	bool firstConflict = true;
	for (address_t addr = address; addr != (address + size); ++addr) {
		std::shared_ptr<IBreakpoint> bp = find_breakpoint(addr);

		if (bp && bp->enabled()) {
			if (firstConflict) {
				const int ret = QMessageBox::question(
					nullptr,
					tr("Overwriting breakpoint"),
					tr("You are attempting to modify bytes which overlap with a software breakpoint. Doing this will implicitly remove any breakpoints which are a conflict. Are you sure you want to do this?"),
					QMessageBox::Yes | QMessageBox::No);

				if (ret == QMessageBox::No) {
					return false;
				}
				firstConflict = false;
			}

			remove_breakpoint(addr);
		}
	}
	return true;
}

/**
 * @brief Updates the user interface.
 */
void update_ui() {
	// force a full update
	Debugger *const gui = ui();
	Q_ASSERT(gui);
	gui->updateUi();
}

/**
 * @brief Modifies bytes at a given address with the provided byte array, filling any remaining space with a specified fill byte.
 *
 * @param address The starting address where the bytes will be modified.
 * @param size The number of bytes to modify.
 * @param bytes A reference to a QByteArray containing the new byte values to write.
 * @param fill The byte value to use for filling any remaining space if the provided byte array is smaller than the specified size.
 * @return true if the bytes were successfully modified, false otherwise.
 */
bool modify_bytes(address_t address, size_t size, QByteArray &bytes, uint8_t fill) {

	if (!edb::v1::overwrite_check(address, size)) {
		return false;
	}

	if (IProcess *process = edb::v1::debugger_core->process()) {
		if (size != 0) {
			// fill bytes
			while (bytes.size() < static_cast<int>(size)) {
				bytes.push_back(fill);
			}

			process->writeBytes(address, bytes.data(), size);

			// do a refresh, not full update
			Debugger *const gui = ui();
			Q_ASSERT(gui);
			gui->refreshUi();
		}
	}

	return true;
}

/**
 * @brief
 */
QByteArray get_md5(const QVector<uint8_t> &bytes) {
	return get_md5(bytes.data(), bytes.size());
}

/**
 * @brief
 */
QByteArray get_md5(const void *p, size_t n) {
	auto b = QByteArray::fromRawData(reinterpret_cast<const char *>(p), static_cast<int>(n));
	return QCryptographicHash::hash(b, QCryptographicHash::Md5);
}

/**
 * @brief Returns a byte array representing the MD5 of a file.
 *
 * @param s The path to the file.
 * @return A QByteArray containing the MD5 hash of the file.
 */
QByteArray get_file_md5(const QString &s) {

	QFile file(s);
	file.open(QIODevice::ReadOnly);
	if (file.isOpen()) {
		if (file.size() != 0) {
			QCryptographicHash hasher(QCryptographicHash::Md5);
			hasher.addData(file.readAll());
			return hasher.result();
		}
	}

	return QByteArray();
}

/**
 * @brief Returns the target of a symbolic link.
 *
 * @param s The path to the symbolic link.
 * @return The path to the target of the symbolic link.
 */
QString symlink_target(const QString &s) {
	return QFileInfo(s).symLinkTarget();
}

/**
 * @brief Converts a version string in the format "x.y.z" into a 32-bit unsigned integer representation.
 *
 * @param s The version string.
 * @return The integer representation of the version string.
 */
quint32 int_version(const QString &s) {

	quint32 ret            = 0;
	const QStringList list = s.split(QLatin1Char('.'));
	if (list.size() == 3) {
		bool ok[3];
		const unsigned int maj = list[0].toUInt(&ok[0]);
		const unsigned int min = list[1].toUInt(&ok[1]);
		const unsigned int rev = list[2].toUInt(&ok[2]);
		if (ok[0] && ok[1] && ok[2]) {
			ret = (maj << 12) | (min << 8) | (rev);
		}
	}
	return ret;
}

/**
 * @brief Parses a command line string into a list of arguments.
 *
 * @param cmdline The command line string to parse.
 * @return A QStringList containing the parsed arguments.
 */
QStringList parse_command_line(const QString &cmdline) {

	QStringList args;
	QString arg;

	int bcount     = 0;
	bool in_quotes = false;

	auto s = cmdline.begin();

	while (s != cmdline.end()) {
		if (!in_quotes && s->isSpace()) {

			// Close the argument and copy it
			args << arg;
			arg.clear();

			// skip the remaining spaces
			do {
				++s;
			} while (s->isSpace());

			// Start with a new argument
			bcount = 0;
		} else if (*s == QLatin1Char('\\')) {

			// '\\'
			arg += *s++;
			++bcount;

		} else if (*s == QLatin1Char('"')) {

			// '"'
			if ((bcount & 1) == 0) {
				/* Preceded by an even number of '\', this is half that
				 * number of '\', plus a quote which we erase.
				 */

				arg.chop(bcount / 2);
				in_quotes = !in_quotes;
			} else {
				/* Preceded by an odd number of '\', this is half that
				 * number of '\' followed by a '"'
				 */

				arg.chop(bcount / 2 + 1);
				arg += QLatin1Char('"');
			}

			++s;
			bcount = 0;
		} else {
			arg += *s++;
			bcount = 0;
		}
	}

	if (!arg.isEmpty()) {
		args << arg;
	}

	return args;
}

/**
 * @brief
 */
Result<address_t, QString> string_to_address(const QString &s) {
	QString hex(s);
	hex.replace(QLatin1String("0x"), QString());

	bool ok;
	auto r = edb::address_t::fromHexString(hex.left(2 * sizeof(edb::address_t)), &ok);
	if (ok) {
		return r;
	}

	return make_unexpected(tr("Error converting string to address"));
}

/**
 * @brief
 */
QString format_bytes(const uint8_t *buffer, size_t count) {
	return v2::format_bytes(buffer, count);
}

/**
 * @brief
 */
QString format_bytes(const QByteArray &x) {
	return v2::format_bytes(x.data(), x.size());
}

/**
 * @brief
 */
QString format_bytes(uint8_t byte) {
	char buf[4];
	qsnprintf(buf, sizeof(buf), "%02x", byte & 0xff);
	return QString::fromLatin1(buf);
}

/**
 * @brief
 */
QString format_pointer(address_t p) {
	return p.toPointerString();
}

/**
 * @brief
 */
address_t current_data_view_address() {
	return qobject_cast<QHexView *>(ui()->tabWidget_->currentWidget())->firstVisibleAddress();
}

/**
 * @brief Returns the address of the instruction pointer of the current thread in the debugged process.
 *
 * @return The address of the instruction pointer, or an empty address_t if the process or thread is not available.
 */
address_t instruction_pointer_address() {
	if (IProcess *process = debugger_core->process()) {
		if (std::shared_ptr<IThread> thread = process->currentThread()) {
			State state;
			thread->getState(&state);
			return state.instructionPointer();
		}
	}

	return address_t{};
}

/**
 * @brief Sets the status message in the status bar.
 *
 * @param message The message to display.
 * @param timeoutMillisecs The timeout for the message in milliseconds.
 */
void set_status(const QString &message, int timeoutMillisecs) {
	ui()->ui.statusbar->showMessage(message, timeoutMillisecs);
	// FIXME: For some reason, despite showMessage() calls repaint, there's some
	// hysteresis in actual look of the status bar: in a busy loop of calls to set_status()
	// it updates to previous content. In some cases it even doesn't actually update.
	// This happens at least on Qt 4.
	// Manual call to repaint makes it show the correct text immediately.
	// TODO(eteran): still true in Qt5.x?
	ui()->ui.statusbar->repaint();
}

/**
 * @brief Clears the status message in the status bar.
 */
void clear_status() {
	ui()->ui.statusbar->clearMessage();
	// FIXME: same comment applies as in set_status()
	// TODO(eteran): still true in Qt5.x?
	ui()->ui.statusbar->repaint();
}

/**
 * @brief Finds a breakpoint at the specified address.
 *
 * @param address The address to search for.
 * @return A shared pointer to the breakpoint, or nullptr if not found.
 */
std::shared_ptr<IBreakpoint> find_breakpoint(address_t address) {
	if (debugger_core) {
		return debugger_core->findBreakpoint(address);
	}
	return nullptr;
}

/**
 * @brief Finds a triggered breakpoint at the specified address.
 *
 * @param address The address to search for.
 * @return A shared pointer to the triggered breakpoint, or nullptr if not found.
 */
std::shared_ptr<IBreakpoint> find_triggered_breakpoint(address_t address) {
	if (debugger_core) {
		return debugger_core->findTriggeredBreakpoint(address);
	}
	return nullptr;
}

/**
 * @brief
 */
size_t pointer_size() {

	if (debugger_core) {
		return debugger_core->pointerSize();
	}

	// default to sizeof the native pointer for sanity!
	return sizeof(void *);
}

/**
 * @brief
 */
QAbstractScrollArea *disassembly_widget() {
	return ui()->cpuView_;
}

/**
 * @brief
 */
QVector<uint8_t> read_pages(address_t address, size_t page_count) {

	if (debugger_core) {
		if (IProcess *process = edb::v1::debugger_core->process()) {
			try {
				const size_t page_size = debugger_core->pageSize();
				QVector<uint8_t> pages(static_cast<int>(page_count * page_size));

				if (process->readPages(address, pages.data(), page_count)) {
					return pages;
				}

			} catch (const std::bad_alloc &) {
				QMessageBox::critical(
					nullptr,
					tr("Memory Allocation Error"),
					tr("Unable to satisfy memory allocation request for requested region"));
			}
		}
	}

	return {};
}

/**
 * @brief Disassembles the instruction at the specified address and returns a human-readable string representation of it.
 *
 * @param address The address of the instruction to disassemble.
 * @return A QString containing the disassembled instruction, or an empty QString if disassembly failed.
 */
QString disassemble_address(address_t address) {
	uint8_t buffer[edb::Instruction::MaxSize];
	if (const int size = edb::v1::get_instruction_bytes(address, buffer)) {
		edb::Instruction inst(buffer, buffer + size, address);
		if (inst) {
			return QString::fromStdString(g_Formatter.toString(inst));
		}
	}

	return {};
}

/**
 * @brief Returns a reference to the global instruction formatter.
 *
 * @return A reference to the global instruction formatter.
 */
CapstoneEDB::Formatter &formatter() {
	return g_Formatter;
}

/**
 * @brief Returns the address of the selected bytes in the stack view, or (address_t)-1 if no selection exists.
 *
 * @return The address of the selected bytes in the stack view, or (address_t)-1 if no selection exists.
 */
address_t selected_stack_address() {

	if (auto hexview = qobject_cast<QHexView *>(ui()->stackDock_->widget())) {
		if (hexview->hasSelectedText()) {
			return hexview->selectedBytesAddress();
		}
	}

	return address_t(-1);
}

/**
 * @brief Returns the size of the selected bytes in the stack view, or 0 if no selection exists.
 *
 * @return The size of the selected bytes in the stack view, or 0 if no selection exists.
 */
size_t selected_stack_size() {
	if (auto hexview = qobject_cast<QHexView *>(ui()->stackDock_->widget())) {
		if (hexview->hasSelectedText()) {
			return hexview->selectedBytesSize();
		}
	}

	return 0;
}

/**
 * @brief Returns the address of the selected data in the current widget, or (address_t)-1 if no selection exists.
 *
 * @return The address of the selected data in the current widget, or (address_t)-1 if no selection exists.
 */
address_t selected_data_address() {
	if (auto hexview = qobject_cast<QHexView *>(ui()->tabWidget_->currentWidget())) {
		if (hexview->hasSelectedText()) {
			return hexview->selectedBytesAddress();
		}
	}

	return address_t(-1);
}

/**
 * @brief Returns the size of the selected data in the current widget.
 *
 * @return The size of the selected data, or 0 if nothing is selected.
 */
size_t selected_data_size() {
	if (auto hexview = qobject_cast<QHexView *>(ui()->tabWidget_->currentWidget())) {
		if (hexview->hasSelectedText()) {
			return hexview->selectedBytesSize();
		}
	}

	return 0;
}

}

namespace v2 {

/**
 * @brief Evaluates a given expression and returns the resulting address if successful.
 *
 * @param expression The expression to evaluate.
 * @return An optional containing the evaluated address if successful, or an empty optional if the evaluation failed.
 * If the evaluation fails, an error message is displayed to the user.
 */
std::optional<edb::address_t> eval_expression(const QString &expression) {

	Expression<address_t> expr(expression, v1::get_variable, v1::get_value);

	const Result<edb::address_t, ExpressionError> address = expr.evaluate();
	if (address) {
		return *address;
	}

	QMessageBox::critical(v1::debugger_ui, tr("Error In Expression!"), QString::fromLatin1(address.error().what()));
	return {};
}

/**
 * @brief Gets an expression from the user and evaluates it.
 *
 * @param title The title of the input dialog.
 * @param prompt The prompt for the input dialog.
 * @return An optional containing the evaluated address if successful, or an empty optional
 * if the user canceled the input dialog or if the evaluation failed.
 */
std::optional<edb::address_t> get_expression_from_user(const QString &title, const QString &prompt) {

	auto inputDialog = std::make_unique<ExpressionDialog>(title, prompt, edb::v1::debugger_ui);

	if (inputDialog->exec()) {
		return inputDialog->getAddress();
	}

	return {};
}

/**
 * @brief Formats a sequence of bytes into a human-readable string.
 *
 * @param buffer The buffer containing the bytes to format.
 * @param count The number of bytes to format.
 * @return A QString containing the formatted bytes.
 */
QString format_bytes(const void *buffer, size_t count) {
	QString bytes;

	if (count != 0) {
		bytes.reserve(static_cast<int>(count) * 4);

		auto it  = static_cast<const uint8_t *>(buffer);
		auto end = it + count;

		char buf[4];
		qsnprintf(buf, sizeof(buf), "%02x", *it++ & 0xff);
		bytes += QString::fromLatin1(buf);

		while (it != end) {
			qsnprintf(buf, sizeof(buf), " %02x", *it++ & 0xff);
			bytes += QString::fromLatin1(buf);
		}
	}

	return bytes;
}

/**
 * @brief Finds the module whose base address is closest to the given address, if any.
 *
 * @param address The address to find the module for.
 * @return An optional Module object if a module is found, or std::nullopt if not.
 */
std::optional<Module> module_for_address(edb::address_t address) {

	IProcess *process = edb::v1::debugger_core->process();
	Q_ASSERT(process);

	QSet<Module> modules = process->loadedModules();

	std::optional<Module> best_module;

	// Find which module whose base address is closest to the bookmark address
	for (const auto &module : modules) {
		if (module.baseAddress <= address) {
			if (!best_module || module.baseAddress > best_module->baseAddress) {
				best_module = module;
			}
		}
	}

	return best_module;
}

/**
 * @brief Compares two module names for equality, taking into account symbolic links and canonical paths.
 *
 * @param name1 The first module name to compare.
 * @param name2 The second module name to compare.
 * @return true if the module names refer to the same file, false otherwise.
 */
bool compare_module_names(const QString &name1, const QString &name2) {

	// TODO(eteran): this works great except for the case where one of the names is the empty string
	// and the other is not, this happens because ld.so gives the "primary" module the name "" (empty string)
	// and the other modules have their full path names. But... our fallback mechanism for finding modules is
	// to look in /proc/<pid>/maps which gives the full path name. We need to figure out a good plan for this.

	// Convert both names into canonical paths to ensure consistent comparison
	// and then resolve any symbolic links or shortcuts to their target paths.

	QString canonicalName1 = QFileInfo(name1).canonicalFilePath();
	QString canonicalName2 = QFileInfo(name2).canonicalFilePath();

	if (QFileInfo(canonicalName1).isSymLink()) {
		canonicalName1 = QFileInfo(canonicalName1).symLinkTarget();
	}

	if (QFileInfo(canonicalName2).isSymLink()) {
		canonicalName2 = QFileInfo(canonicalName2).symLinkTarget();
	}

	QT_STATBUF statbuf1;
	if (QT_STAT(canonicalName1.toUtf8().data(), &statbuf1) != 0) {
		return false;
	}

	QT_STATBUF statbuf2;
	if (QT_STAT(canonicalName2.toUtf8().data(), &statbuf2) != 0) {
		return false;
	}

	return statbuf1.st_dev == statbuf2.st_dev && statbuf1.st_ino == statbuf2.st_ino;
}

}

}

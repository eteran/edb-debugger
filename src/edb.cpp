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
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>

#include <QDebug>
#include <cctype>

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

	if (const std::shared_ptr<Symbol> s = edb::v1::symbol_manager().findNearSymbol(address)) {
		*value  = s->name;
		*offset = address - s->address;
		return true;
	}

	*offset = 0;
	return false;
}
}

const char version[] = "1.3.0";

namespace internal {

//------------------------------------------------------------------------------
// Name: register_plugin
// Desc:
//------------------------------------------------------------------------------
bool register_plugin(const QString &filename, QObject *plugin) {
	if (!g_GeneralPlugins.contains(filename)) {
		g_GeneralPlugins[filename] = plugin;
		return true;
	}

	return false;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
void load_function_db() {
	QFile file(":/debugger/xml/functions.xml");
	QDomDocument doc;

	if (file.open(QIODevice::ReadOnly)) {
		if (doc.setContent(&file)) {
			QDomElement root     = doc.firstChildElement("functions");
			QDomElement function = root.firstChildElement("function");
			for (; !function.isNull(); function = function.nextSiblingElement("function")) {

				Prototype func;
				func.name     = function.attribute("name");
				func.type     = function.attribute("type");
				func.noreturn = function.attribute("noreturn", "false") == "true";

				QDomElement argument = function.firstChildElement("argument");
				for (; !argument.isNull(); argument = argument.nextSiblingElement("argument")) {

					Argument arg;
					arg.name = argument.attribute("name");
					arg.type = argument.attribute("type");
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

//------------------------------------------------------------------------------
// Name: set_cpu_selected_address
// Desc:
//------------------------------------------------------------------------------
void set_cpu_selected_address(address_t address) {
	ui()->ui.cpuView->setSelectedAddress(address);
	ui()->ui.cpuView->update();
}

//------------------------------------------------------------------------------
// Name: cpu_selected_address
// Desc:
//------------------------------------------------------------------------------
address_t cpu_selected_address() {
	return ui()->ui.cpuView->selectedAddress();
}

//------------------------------------------------------------------------------
// Name: current_cpu_view_region
// Desc:
//------------------------------------------------------------------------------
std::shared_ptr<IRegion> current_cpu_view_region() {
	return ui()->ui.cpuView->region();
}

//------------------------------------------------------------------------------
// Name: repaint_cpu_view
// Desc:
//------------------------------------------------------------------------------
void repaint_cpu_view() {
	Debugger *const gui = ui();
	Q_ASSERT(gui);
	gui->ui.cpuView->update();
}

//------------------------------------------------------------------------------
// Name: symbol_manager
// Desc:
//------------------------------------------------------------------------------
ISymbolManager &symbol_manager() {
	static SymbolManager g_SymbolManager;
	return g_SymbolManager;
}

//------------------------------------------------------------------------------
// Name: memory_regions
// Desc:
//------------------------------------------------------------------------------
MemoryRegions &memory_regions() {
	static MemoryRegions g_MemoryRegions;
	return g_MemoryRegions;
}

//------------------------------------------------------------------------------
// Name: arch_processor
// Desc:
//------------------------------------------------------------------------------
ArchProcessor &arch_processor() {
	static ArchProcessor g_ArchProcessor;
	return g_ArchProcessor;
}

//------------------------------------------------------------------------------
// Name: set_analyzer
// Desc:
//------------------------------------------------------------------------------
IAnalyzer *set_analyzer(IAnalyzer *p) {
	Q_ASSERT(p);
	return g_Analyzer.fetchAndStoreAcquire(p);
}

//------------------------------------------------------------------------------
// Name: analyzer
// Desc:
//------------------------------------------------------------------------------
IAnalyzer *analyzer() {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	return g_Analyzer.loadRelaxed();
#else
	return g_Analyzer.load();
#endif
}

//------------------------------------------------------------------------------
// Name: execute_debug_event_handlers
// Desc:
//------------------------------------------------------------------------------
EventStatus execute_debug_event_handlers(const std::shared_ptr<IDebugEvent> &e) {
	return g_DebugEventHandlers.execute(e);
}

//------------------------------------------------------------------------------
// Name: add_debug_event_handler
// Desc:
//------------------------------------------------------------------------------
void add_debug_event_handler(IDebugEventHandler *p) {
	g_DebugEventHandlers.add(p);
}

//------------------------------------------------------------------------------
// Name: remove_debug_event_handler
// Desc:
//------------------------------------------------------------------------------
void remove_debug_event_handler(IDebugEventHandler *p) {
	g_DebugEventHandlers.remove(p);
}

//------------------------------------------------------------------------------
// Name: jump_to_address
// Desc: sets the disassembly display to a given address, returning success
//       status
//------------------------------------------------------------------------------
bool jump_to_address(address_t address) {
	Debugger *const gui = ui();
	Q_ASSERT(gui);
	return gui->jumpToAddress(address);
}

//------------------------------------------------------------------------------
// Name: dump_data_range
// Desc: shows a given address through a given end address in the data view,
//       optionally in a new tab
//------------------------------------------------------------------------------
bool dump_data_range(address_t address, address_t end_address, bool new_tab) {
	Debugger *const gui = ui();
	Q_ASSERT(gui);
	return gui->dumpDataRange(address, end_address, new_tab);
}

//------------------------------------------------------------------------------
// Name: dump_data_range
// Desc: shows a given address through a given end address in the data view
//------------------------------------------------------------------------------
bool dump_data_range(address_t address, address_t end_address) {
	return dump_data_range(address, end_address, false);
}

//------------------------------------------------------------------------------
// Name: dump_stack
// Desc:
//------------------------------------------------------------------------------
bool dump_stack(address_t address) {
	return dump_stack(address, true);
}

//------------------------------------------------------------------------------
// Name: dump_stack
// Desc: shows a given address in the stack view
//------------------------------------------------------------------------------
bool dump_stack(address_t address, bool scroll_to) {
	Debugger *const gui = ui();
	Q_ASSERT(gui);
	return gui->dumpStack(address, scroll_to);
}

//------------------------------------------------------------------------------
// Name: dump_data
// Desc: shows a given address in the data view, optionally in a new tab
//------------------------------------------------------------------------------
bool dump_data(address_t address, bool new_tab) {
	Debugger *const gui = ui();
	Q_ASSERT(gui);
	return gui->dumpData(address, new_tab);
}

//------------------------------------------------------------------------------
// Name: dump_data
// Desc: shows a given address in the data view
//------------------------------------------------------------------------------
bool dump_data(address_t address) {
	return dump_data(address, false);
}

//------------------------------------------------------------------------------
// Name: set_breakpoint_condition
// Desc:
//------------------------------------------------------------------------------
void set_breakpoint_condition(address_t address, const QString &condition) {

	if (std::shared_ptr<IBreakpoint> bp = find_breakpoint(address)) {
		bp->condition = condition;
	}
}

//------------------------------------------------------------------------------
// Name: get_breakpoint_condition
// Desc:
//------------------------------------------------------------------------------
QString get_breakpoint_condition(address_t address) {
	QString ret;

	if (std::shared_ptr<IBreakpoint> bp = find_breakpoint(address)) {
		ret = bp->condition;
	}

	return ret;
}

//------------------------------------------------------------------------------
// Name: create_breakpoint
// Desc: adds a breakpoint at a given address
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Name: enable_breakpoint
// Desc:
//------------------------------------------------------------------------------
address_t enable_breakpoint(address_t address) {
	if (address != 0) {
		std::shared_ptr<IBreakpoint> bp = find_breakpoint(address);
		if (bp && bp->enable()) {
			return address;
		}
	}
	return 0;
}

//------------------------------------------------------------------------------
// Name: disable_breakpoint
// Desc:
//------------------------------------------------------------------------------
address_t disable_breakpoint(address_t address) {
	if (address != 0) {
		std::shared_ptr<IBreakpoint> bp = find_breakpoint(address);
		if (bp && bp->disable()) {
			return address;
		}
	}
	return 0;
}

//------------------------------------------------------------------------------
// Name: toggle_breakpoint
// Desc: toggles the existence of a breakpoint at a given address
//------------------------------------------------------------------------------
void toggle_breakpoint(address_t address) {
	if (find_breakpoint(address)) {
		remove_breakpoint(address);
	} else {
		create_breakpoint(address);
	}
}

//------------------------------------------------------------------------------
// Name: remove_breakpoint
// Desc: removes a breakpoint
//------------------------------------------------------------------------------
void remove_breakpoint(address_t address) {
	debugger_core->removeBreakpoint(address);
	repaint_cpu_view();
}

//------------------------------------------------------------------------------
// Name: eval_expression
// Desc:
//------------------------------------------------------------------------------
bool eval_expression(const QString &expression, address_t *value) {

	Q_ASSERT(value);

	Expression<address_t> expr(expression, get_variable, get_value);

	const Result<edb::address_t, ExpressionError> address = expr.evaluate();
	if (address) {
		*value = *address;
		return true;
	} else {
		QMessageBox::critical(debugger_ui, tr("Error In Expression!"), address.error().what());
		return false;
	}
}

//------------------------------------------------------------------------------
// Name: get_expression_from_user
// Desc:
//------------------------------------------------------------------------------
bool get_expression_from_user(const QString &title, const QString &prompt, address_t *value) {

	bool retval      = false;
	auto inputDialog = std::make_unique<ExpressionDialog>(title, prompt, edb::v1::debugger_ui);

	if (inputDialog->exec()) {
		*value = inputDialog->getAddress();
		retval = true;
	}

	return retval;
}

//------------------------------------------------------------------------------
// Name: get_value_from_user
// Desc:
//------------------------------------------------------------------------------
bool get_value_from_user(Register &value) {
	return get_value_from_user(value, tr("Input Value"));
}

//------------------------------------------------------------------------------
// Name: get_value_from_user
// Desc:
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Name: get_binary_string_from_user
// Desc:
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Name: dialog_options
// Desc: returns a pointer to the options dialog
//------------------------------------------------------------------------------
QPointer<QDialog> dialog_options() {
	static QPointer<QDialog> dialog = new DialogOptions(debugger_ui);
	return dialog;
}

//------------------------------------------------------------------------------
// Name: config
// Desc:
//------------------------------------------------------------------------------
Configuration &config() {
	static Configuration g_Configuration;
	return g_Configuration;
}

//------------------------------------------------------------------------------
// Name: get_human_string_at_address
// Desc: attempts to create a summary of the content at address appropriate for
// display in a user interface.
// Note: strings are comprised of printable characters and whitespace.
// Note: found_length is needed because we replace characters which need an
//       escape char with the escape sequence (thus the resultant string may be
//       longer than the original). found_length is the original length.
//------------------------------------------------------------------------------

bool get_human_string_at_address(address_t address, QString &s) {
	bool ret = false;
	if (address > 0x10000ULL) { // FIXME use page size
		QString string_param;
		int string_length;

		if (get_ascii_string_at_address(address, string_param, edb::v1::config().min_string_length, 256, string_length)) {
			ret = true;
			s.append(
				QString("ASCII \"%1\" ").arg(string_param));
		} else if (get_utf16_string_at_address(address, string_param, edb::v1::config().min_string_length, 256, string_length)) {
			ret = true;
			s.append(
				QString("UTF16 \"%1\" ").arg(string_param));
		}
	}
	return ret;
}

//------------------------------------------------------------------------------
// Name: get_ascii_string_at_address
// Desc: attempts to get a string at a given address whose length is >= min_length
//       and < max_length
// Note: strings are comprised of printable characters and whitespace.
// Note: found_length is needed because we replace characters which need an
//       escape char with the escape sequence (thus the resultant string may be
//       longer than the original). found_length is the original length.
//------------------------------------------------------------------------------
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
						s += ch;
					} else {
						break;
					}
				}
			}

			is_string = s.length() >= min_length;

			if (is_string) {
				found_length = s.length();
				s.replace("\r", "\\r");
				s.replace("\n", "\\n");
				s.replace("\t", "\\t");
				s.replace("\v", "\\v");
				s.replace("\"", "\\\"");
			}
		}
	}

	return is_string;
}

//------------------------------------------------------------------------------
// Name: get_utf16_string_at_address
// Desc: attempts to get a string at a given address whose length os >= min_length
//       and < max_length
// Note: strings are comprised of printable characters and whitespace.
// Note: found_length is needed because we replace characters which need an
//       escape char with the escape sequence (thus the resultant string may be
//       longer than the original). found_length is the original length.
//------------------------------------------------------------------------------
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
				found_length = s.length();
				s.replace("\r", "\\r");
				s.replace("\n", "\\n");
				s.replace("\t", "\\t");
				s.replace("\v", "\\v");
				s.replace("\"", "\\\"");
			}
		}
	}
	return is_string;
}

//------------------------------------------------------------------------------
// Name: find_function_symbol
// Desc:
//------------------------------------------------------------------------------
QString find_function_symbol(address_t address, const QString &default_value, int *offset) {

	QString symname(default_value);
	int off;

	if (function_symbol_base(address, &symname, &off)) {

		if (config().function_offsets_in_hex) {
			symname = QString("%1+0x%2").arg(symname).arg(off, 0, 16);
		} else {
			symname = QString("%1+%2").arg(symname).arg(off, 0, 10);
		}

		if (offset) {
			*offset = off;
		}
	}

	return symname;
}

//------------------------------------------------------------------------------
// Name: find_function_symbol
// Desc:
//------------------------------------------------------------------------------
QString find_function_symbol(address_t address, const QString &default_value) {
	return find_function_symbol(address, default_value, nullptr);
}

//------------------------------------------------------------------------------
// Name: find_function_symbol
// Desc:
//------------------------------------------------------------------------------
QString find_function_symbol(address_t address) {
	return find_function_symbol(address, QString(), nullptr);
}

//------------------------------------------------------------------------------
// Name: get_variable
// Desc:
//------------------------------------------------------------------------------
address_t get_variable(const QString &s, bool *ok, ExpressionError *err) {

	Q_ASSERT(debugger_core);
	Q_ASSERT(ok);
	Q_ASSERT(err);

	if (IProcess *process = debugger_core->process()) {

		State state;
		process->currentThread()->getState(&state);
		const Register reg = state.value(s);
		*ok                = reg.valid();
		if (!*ok) {
			if (const std::shared_ptr<Symbol> sym = edb::v1::symbol_manager().find(s)) {
				*ok = true;
				return sym->address;
			}

			*err = ExpressionError(ExpressionError::UnknownVariable);
			return 0;
		}

		// FIXME: should this really return segment base, not selector?
		// FIXME: if it's really meant to return base, then need to check whether
		//        State::operator[]() returned valid Register
		if (reg.name() == "fs") {
			return state["fs_base"].valueAsAddress();
		} else if (reg.name() == "gs") {
			return state["gs_base"].valueAsAddress();
		}

		if (reg.bitSize() > 8 * sizeof(edb::address_t)) {
			*err = ExpressionError(ExpressionError::UnknownVariable);
			return 0;
		}

		return reg.valueAsAddress();
	}

	*err = ExpressionError(ExpressionError::UnknownVariable);
	return 0;
}

//------------------------------------------------------------------------------
// Name: get_value
// Desc:
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Name: get_instruction_bytes
// Desc: attempts to read at most size bytes.
//------------------------------------------------------------------------------
bool get_instruction_bytes(address_t address, uint8_t *buf, int *size) {

	Q_ASSERT(debugger_core);
	Q_ASSERT(size);
	Q_ASSERT(*size >= 0);

	if (IProcess *process = edb::v1::debugger_core->process()) {
		*size = process->readBytes(address, buf, *size);
		if (*size) {
			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: get_instruction_bytes
// Desc: attempts to read at most size bytes.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Name: get_binary_info
// Desc: gets an object which knows how to analyze the binary file provided
//       or NULL if none-found.
// Note: the caller is responsible for deleting the object!
//------------------------------------------------------------------------------
std::unique_ptr<IBinary> get_binary_info(const std::shared_ptr<IRegion> &region) {
	Q_FOREACH (IBinary::create_func_ptr_t f, g_BinaryInfoList) {
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

#if 0
	qDebug() << "Failed to find any binary parser for region"
		<< QString::number(region->start(), 16);
#endif
	return nullptr;
}

//------------------------------------------------------------------------------
// Name: locate_main_function
// Desc:
// Note: this currently only works for glibc linked elf files
//------------------------------------------------------------------------------
address_t locate_main_function() {

	if (debugger_core) {
		if (IProcess *process = debugger_core->process()) {
			const address_t main_func = process->calculateMain();
			if (main_func != 0) {
				return main_func;
			} else {
				return process->entryPoint();
			}
		}
	}

	return 0;
}

//------------------------------------------------------------------------------
// Name: plugin_list
// Desc:
//------------------------------------------------------------------------------
const QMap<QString, QObject *> &plugin_list() {
	return g_GeneralPlugins;
}

//------------------------------------------------------------------------------
// Name: find_plugin_by_name
// Desc: gets a pointer to a plugin based on it's classname
//------------------------------------------------------------------------------
IPlugin *find_plugin_by_name(const QString &name) {
	Q_FOREACH (QObject *p, g_GeneralPlugins) {
		if (name == p->metaObject()->className()) {
			return qobject_cast<IPlugin *>(p);
		}
	}
	return nullptr;
}

//------------------------------------------------------------------------------
// Name: reload_symbols
// Desc:
//------------------------------------------------------------------------------
void reload_symbols() {
	symbol_manager().clear();
}

//------------------------------------------------------------------------------
// Name: get_function_info
// Desc:
//------------------------------------------------------------------------------
const Prototype *get_function_info(const QString &function) {

	auto it = g_FunctionDB.find(function);
	if (it != g_FunctionDB.end()) {
		return &(it.value());
	}

	return nullptr;
}

//------------------------------------------------------------------------------
// Name: primary_data_region
// Desc: returns the main .data section of the main executable module
// Note: make sure that memory regions has been sync'd first or you will likely
//       get a null-region result
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Name: primary_code_region
// Desc: returns the main .text section of the main executable module
// Note: make sure that memory regions has been sync'd first or you will likely
//       get a null-region result
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Name: pop_value
// Desc:
//------------------------------------------------------------------------------
void pop_value(State *state) {
	Q_ASSERT(state);
	state->adjustStack(pointer_size());
}

//------------------------------------------------------------------------------
// Name: push_value
// Desc:
//------------------------------------------------------------------------------
void push_value(State *state, reg_t value) {
	Q_ASSERT(state);

	if (IProcess *process = edb::v1::debugger_core->process()) {
		state->adjustStack(-static_cast<int>(pointer_size()));
		process->writeBytes(state->stackPointer(), &value, pointer_size());
	}
}

//------------------------------------------------------------------------------
// Name: register_binary_info
// Desc:
//------------------------------------------------------------------------------
void register_binary_info(IBinary::create_func_ptr_t fptr) {
	if (!g_BinaryInfoList.contains(fptr)) {
		g_BinaryInfoList.push_back(fptr);
	}
}

//------------------------------------------------------------------------------
// Name: edb_version
// Desc: returns an integer comparable version of our current version string
//------------------------------------------------------------------------------
quint32 edb_version() {
	return int_version(version);
}

//------------------------------------------------------------------------------
// Name: overwrite_check
// Desc:
//------------------------------------------------------------------------------
bool overwrite_check(address_t address, size_t size) {
	bool firstConflict = true;
	for (address_t addr = address; addr != (address + size); ++addr) {
		std::shared_ptr<IBreakpoint> bp = find_breakpoint(addr);

		if (bp && bp->enabled()) {
			if (firstConflict) {
				const int ret = QMessageBox::question(
					nullptr,
					tr("Overwritting breakpoint"),
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

//------------------------------------------------------------------------------
// Name: update_ui
// Desc:
//------------------------------------------------------------------------------
void update_ui() {
	// force a full update
	Debugger *const gui = ui();
	Q_ASSERT(gui);
	gui->updateUi();
}

//------------------------------------------------------------------------------
// Name: modify_bytes
// Desc:
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Name: get_md5
// Desc:
//------------------------------------------------------------------------------
QByteArray get_md5(const QVector<uint8_t> &bytes) {
	return get_md5(&bytes[0], bytes.size());
}

//------------------------------------------------------------------------------
// Name: get_md5
// Desc:
//------------------------------------------------------------------------------
QByteArray get_md5(const void *p, size_t n) {
	auto b = QByteArray::fromRawData(reinterpret_cast<const char *>(p), n);
	return QCryptographicHash::hash(b, QCryptographicHash::Md5);
}

//------------------------------------------------------------------------------
// Name: get_file_md5
// Desc: returns a byte array representing the MD5 of a file
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Name: symlink_target
// Desc:
//------------------------------------------------------------------------------
QString symlink_target(const QString &s) {
	return QFileInfo(s).symLinkTarget();
}

//------------------------------------------------------------------------------
// Name: int_version
// Desc: returns an integer comparable version of a version string in x.y.z
//       format, or 0 if error
//------------------------------------------------------------------------------
quint32 int_version(const QString &s) {

	ulong ret              = 0;
	const QStringList list = s.split(".");
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

//------------------------------------------------------------------------------
// Name: parse_command_line
// Desc:
//------------------------------------------------------------------------------
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
		} else if (*s == '\\') {

			// '\\'
			arg += *s++;
			++bcount;

		} else if (*s == '"') {

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
				arg += '"';
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

//------------------------------------------------------------------------------
// Name: string_to_address
// Desc:
//------------------------------------------------------------------------------
Result<address_t, QString> string_to_address(const QString &s) {
	QString hex(s);
	hex.replace("0x", "");

	bool ok;
	address_t r = edb::address_t::fromHexString(hex.left(2 * sizeof(edb::address_t)), &ok);
	if (ok) {
		return r;
	}

	return make_unexpected(tr("Error converting string to address"));
}

//------------------------------------------------------------------------------
// Name: format_bytes
// Desc:
//------------------------------------------------------------------------------
QString format_bytes(const uint8_t *buffer, size_t count) {
	return v2::format_bytes(buffer, count);
}

//------------------------------------------------------------------------------
// Name: format_bytes
// Desc:
//------------------------------------------------------------------------------
QString format_bytes(const QByteArray &x) {
	return v2::format_bytes(x.data(), x.size());
}

//------------------------------------------------------------------------------
// Name: format_bytes
// Desc:
//------------------------------------------------------------------------------
QString format_bytes(uint8_t byte) {
	char buf[4];
	qsnprintf(buf, sizeof(buf), "%02x", byte & 0xff);
	return QLatin1String(buf);
}

//------------------------------------------------------------------------------
// Name: format_pointer
// Desc:
//------------------------------------------------------------------------------
QString format_pointer(address_t p) {
	return p.toPointerString();
}

//------------------------------------------------------------------------------
// Name: current_data_view_address
// Desc:
//------------------------------------------------------------------------------
address_t current_data_view_address() {
	return qobject_cast<QHexView *>(ui()->ui.tabWidget->currentWidget())->firstVisibleAddress();
}

//------------------------------------------------------------------------------
// Name: set_status
// Desc:
//------------------------------------------------------------------------------
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

void clear_status() {
	ui()->ui.statusbar->clearMessage();
	// FIXME: same comment applies as in set_status()
	// TODO(eteran): still true in Qt5.x?
	ui()->ui.statusbar->repaint();
}

//------------------------------------------------------------------------------
// Name: find_breakpoint
// Desc:
//------------------------------------------------------------------------------
std::shared_ptr<IBreakpoint> find_breakpoint(address_t address) {
	if (debugger_core) {
		return debugger_core->findBreakpoint(address);
	}
	return nullptr;
}

//------------------------------------------------------------------------------
// Name: find_triggered_breakpoint
// Desc:
//------------------------------------------------------------------------------
std::shared_ptr<IBreakpoint> find_triggered_breakpoint(address_t address) {
	if (debugger_core) {
		return debugger_core->findTriggeredBreakpoint(address);
	}
	return nullptr;
}

//------------------------------------------------------------------------------
// Name: pointer_size
// Desc:
//------------------------------------------------------------------------------
size_t pointer_size() {

	if (debugger_core) {
		return debugger_core->pointerSize();
	}

	// default to sizeof the native pointer for sanity!
	return sizeof(void *);
}

//------------------------------------------------------------------------------
// Name: disassembly_widget
// Desc:
//------------------------------------------------------------------------------
QAbstractScrollArea *disassembly_widget() {
	return ui()->ui.cpuView;
}

//------------------------------------------------------------------------------
// Name: read_pages
// Desc:
//------------------------------------------------------------------------------
QVector<uint8_t> read_pages(address_t address, size_t page_count) {

	if (debugger_core) {
		if (IProcess *process = edb::v1::debugger_core->process()) {
			try {
				const size_t page_size = debugger_core->pageSize();
				QVector<uint8_t> pages(page_count * page_size);

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

//------------------------------------------------------------------------------
// Name: disassemble_address
// Desc: will return a QString where isNull is true on failure
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Name: formatter
// Desc: returns a reference to the global instruction formatter
//------------------------------------------------------------------------------
CapstoneEDB::Formatter &formatter() {
	return g_Formatter;
}

//------------------------------------------------------------------------------
// Name: selected_stack_address
// Desc: returns the address of the selection or (address_t)-1
//------------------------------------------------------------------------------
address_t selected_stack_address() {

	if (auto hexview = qobject_cast<QHexView *>(ui()->ui.stackDock->widget())) {
		if (hexview->hasSelectedText()) {
			return hexview->selectedBytesAddress();
		}
	}

	return address_t(-1);
}

//------------------------------------------------------------------------------
// Name: selected_stack_size
// Desc: returns the size of the selection or 0
//------------------------------------------------------------------------------
size_t selected_stack_size() {
	if (auto hexview = qobject_cast<QHexView *>(ui()->ui.stackDock->widget())) {
		if (hexview->hasSelectedText()) {
			return hexview->selectedBytesSize();
		}
	}

	return 0;
}

//------------------------------------------------------------------------------
// Name: selected_stack_address
// Desc: returns the address of the selection or (address_t)-1
//------------------------------------------------------------------------------
address_t selected_data_address() {
	if (auto hexview = qobject_cast<QHexView *>(ui()->ui.tabWidget->currentWidget())) {
		if (hexview->hasSelectedText()) {
			return hexview->selectedBytesAddress();
		}
	}

	return address_t(-1);
}

//------------------------------------------------------------------------------
// Name: selected_data_size
// Desc: returns the size of the selection or 0
//------------------------------------------------------------------------------
size_t selected_data_size() {
	if (auto hexview = qobject_cast<QHexView *>(ui()->ui.tabWidget->currentWidget())) {
		if (hexview->hasSelectedText()) {
			return hexview->selectedBytesSize();
		}
	}

	return 0;
}

}

namespace v2 {

//------------------------------------------------------------------------------
// Name: eval_expression
// Desc:
//------------------------------------------------------------------------------
boost::optional<edb::address_t> eval_expression(const QString &expression) {

	Expression<address_t> expr(expression, v1::get_variable, v1::get_value);

	const Result<edb::address_t, ExpressionError> address = expr.evaluate();
	if (address) {
		return *address;
	} else {
		QMessageBox::critical(v1::debugger_ui, tr("Error In Expression!"), address.error().what());
		return boost::none;
	}
}

//------------------------------------------------------------------------------
// Name: get_expression_from_user
// Desc:
//------------------------------------------------------------------------------
boost::optional<edb::address_t> get_expression_from_user(const QString &title, const QString &prompt) {

	auto inputDialog = std::make_unique<ExpressionDialog>(title, prompt, edb::v1::debugger_ui);

	if (inputDialog->exec()) {
		return inputDialog->getAddress();
	}

	return boost::none;
}

//------------------------------------------------------------------------------
// Name: format_bytes
// Desc:
//------------------------------------------------------------------------------
QString format_bytes(const void *buffer, size_t count) {
	QString bytes;

	if (count != 0) {
		bytes.reserve(count * 4);

		auto it = static_cast<const uint8_t *>(buffer);
		auto end = it + count;

		char buf[4];
		qsnprintf(buf, sizeof(buf), "%02x", *it++ & 0xff);
		bytes += buf;

		while (it != end) {
			qsnprintf(buf, sizeof(buf), " %02x", *it++ & 0xff);
			bytes += buf;
		}
	}

	return bytes;
}

}
}

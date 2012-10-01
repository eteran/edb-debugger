/*
Copyright (C) 2006 - 2011 Evan Teran
                          eteran@alum.rit.edu

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

#ifndef ANALYZER_20080630_H_
#define ANALYZER_20080630_H_

#include "IAnalyzer.h"
#include "IPlugin.h"
#include "MemoryRegion.h"
#include "Symbol.h"
#include "Types.h"
#include <QSet>
#include <QMap>
#include <QHash>

class QMenu;
class AnalyzerWidget;

class Analyzer : public QObject, public IAnalyzer, public IPlugin {
	Q_OBJECT
	Q_INTERFACES(IPlugin)
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")

public:
	Analyzer();

public:
	virtual QMenu *menu(QWidget *parent = 0);
	virtual QList<QAction *> cpu_context_menu();

private:
	virtual void private_init();
	virtual QWidget *options_page();

public:
	virtual void analyze(const MemoryRegion &region);
	virtual FunctionMap functions(const MemoryRegion &region) const;
	virtual AddressCategory category(edb::address_t address) const;
	virtual void invalidate_analysis(const MemoryRegion &region);
	virtual void invalidate_analysis();
	virtual QSet<edb::address_t> specified_functions() const { return specified_functions_; }

private:
	void indent_header();
	void bonus_stack_frames_helper(Function &info) const;
	void bonus_stack_frames(FunctionMap &results);
	void bonus_marked_functions(const MemoryRegion &region, FunctionMap &results);
	void bonus_symbols(const MemoryRegion &region, FunctionMap &results);
	void bonus_symbols_helper(const MemoryRegion &region, FunctionMap &results, const Symbol::pointer &sym);
	void bonus_entry_point(const MemoryRegion &region, FunctionMap &results) const;
	void bonus_main(const MemoryRegion &region, FunctionMap &results) const;

private:
	QByteArray md5_region(const MemoryRegion &region) const;
	bool find_containing_function(edb::address_t address, Function &function) const;
	bool is_inside_known(const MemoryRegion &region, edb::address_t address);
	bool is_stack_frame(edb::address_t address) const;
	bool is_thunk(edb::address_t address) const;
	edb::address_t module_entry_point(const MemoryRegion &region) const;
	int walk_all_functions(FunctionMap &results, const MemoryRegion &region, QSet<edb::address_t> &walked_functions);
	void find_calls_from_known(const MemoryRegion &region, FunctionMap &results, QSet<edb::address_t> &walked_functions);
	void find_function_calls(const MemoryRegion &region, FunctionMap &results);
	void find_function_end(Function &function, edb::address_t end_address, QSet<edb::address_t> &found_functions, const FunctionMap &results);
	void fix_overlaps(FunctionMap &function_map);
	void invalidate_dynamic_analysis(const MemoryRegion &region);
	void set_function_types(FunctionMap &results);
	void set_function_types_helper(Function &info) const;
	void update_results_entry(FunctionMap &results, edb::address_t address) const;
	void collect_high_ref_results(FunctionMap &function_map, FunctionMap &found_functions) const;
	void collect_low_ref_results(const MemoryRegion &region, FunctionMap &function_map, FunctionMap &found_functions);

Q_SIGNALS:
	void update_progress(int);

public Q_SLOTS:
	void do_ip_analysis();
	void do_view_analysis();
	void goto_function_start();
	void goto_function_end();
	void mark_function_start();
	void show_specified();

private:
	void do_analysis(const MemoryRegion &region);

private:
	struct RegionInfo {
		FunctionMap analysis;
		QByteArray  md5;
		bool        fuzzy;
	};

	QMenu *                      menu_;
	QHash<MemoryRegion, RegionInfo> analysis_info_;
	QSet<edb::address_t>         specified_functions_;
	AnalyzerWidget *             analyzer_widget_;
};

#endif

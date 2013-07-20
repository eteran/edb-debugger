/*
Copyright (C) 2006 - 2013 Evan Teran
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
#include "IRegion.h"
#include "Symbol.h"
#include "Types.h"
#include <QSet>
#include <QMap>
#include <QHash>
#include <QVector>
#include <QList>

class QMenu;
class AnalyzerWidget;

class Analyzer : public QObject, public IAnalyzer, public IPlugin {
	Q_OBJECT
	
#if QT_VERSION >= 0x050000
	Q_PLUGIN_METADATA(IID "edb.IPlugin/1.0")
#endif
	Q_INTERFACES(IPlugin)
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")

private:
	struct RegionData;
	struct BasicBlock;
	
public:
	Analyzer();

public:
	virtual QMenu *menu(QWidget *parent = 0);
	virtual QList<QAction *> cpu_context_menu();

private:
	virtual void private_init();
	virtual QWidget *options_page();

public:
	virtual AddressCategory category(edb::address_t address) const;
	virtual FunctionMap functions(const IRegion::pointer &region) const;
	virtual QSet<edb::address_t> specified_functions() const { return specified_functions_; }
	virtual edb::address_t find_containing_function(edb::address_t address, bool *ok) const;
	virtual void analyze(const IRegion::pointer &region);
	virtual void invalidate_analysis();
	virtual void invalidate_analysis(const IRegion::pointer &region);

private:
	void ident_header();
	void bonus_stack_frames_helper(Function &info) const;
	void bonus_stack_frames(RegionData *data);
	void bonus_marked_functions(RegionData *data);
	void bonus_symbols(RegionData *data);
	void bonus_symbols_helper(RegionData *data, const Symbol::pointer &sym);
	void bonus_entry_point(RegionData *data) const;
	void bonus_main(RegionData *data) const;

private:
	QByteArray md5_region(const IRegion::pointer &region) const;
	bool find_containing_function(edb::address_t address, Function *function) const;
	bool is_inside_known(const IRegion::pointer &region, edb::address_t address);
	bool is_stack_frame(edb::address_t address) const;
	bool is_thunk(edb::address_t address) const;
	edb::address_t module_entry_point(const IRegion::pointer &region) const;
	int walk_all_functions(FunctionMap *results, const IRegion::pointer &region, QSet<edb::address_t> *walked_functions);
	void collect_high_ref_results(FunctionMap *function_map, FunctionMap *found_functions) const;
	void collect_low_ref_results(const IRegion::pointer &region, FunctionMap *function_map, FunctionMap *found_functions);
	void find_calls_from_known(const IRegion::pointer &region, FunctionMap *results, QSet<edb::address_t> *walked_functions);
	void find_function_calls(const IRegion::pointer &region, FunctionMap *results);
	void find_function_end(Function *function, edb::address_t end_address, QSet<edb::address_t> *found_functions, const FunctionMap &results);
	void fix_overlaps(FunctionMap *function_map);
	void invalidate_dynamic_analysis(const IRegion::pointer &region);
	void set_function_types(FunctionMap *results);
	void set_function_types_helper(Function &info) const;
	void update_results_entry(FunctionMap *results, edb::address_t address) const;
	void collect_basic_blocks(RegionData *data);
	void collect_known_functions(RegionData *data);

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
	void do_analysis(const IRegion::pointer &region);

private:
	static int block_size(const BasicBlock &basic_block);

private:
	struct BasicBlock {
		QList<QSharedPointer<edb::Instruction> > instructions;
	};

	struct RegionData {
		QHash<edb::address_t, BasicBlock> basic_blocks;
		QSet<edb::address_t>              known_functions;
		FunctionMap                       analysis;
		QByteArray                        md5;
		bool                              fuzzy;
		IRegion::pointer                  region;
	};

	QMenu                              *menu_;
	QHash<edb::address_t, RegionData>   analysis_info_;
	QSet<edb::address_t>                specified_functions_;
	AnalyzerWidget                     *analyzer_widget_;
};

#endif

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

#ifndef ANALYZER_20080630_H_
#define ANALYZER_20080630_H_

#include "IAnalyzer.h"
#include "IPlugin.h"
#include "IRegion.h"
#include "Symbol.h"
#include "Types.h"
#include "BasicBlock.h"
#include <QSet>
#include <QMap>
#include <QHash>
#include <QVector>
#include <QList>

class QMenu;

namespace AnalyzerPlugin {

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
	virtual FunctionMap functions(const std::shared_ptr<IRegion> &region) const;
	virtual FunctionMap functions() const;
	virtual QSet<edb::address_t> specified_functions() const { return specified_functions_; }
	virtual Result<edb::address_t> find_containing_function(edb::address_t address) const;
	virtual void analyze(const std::shared_ptr<IRegion> &region);
	virtual void invalidate_analysis();
	virtual void invalidate_analysis(const std::shared_ptr<IRegion> &region);
	virtual bool for_funcs_in_range(const edb::address_t start, const edb::address_t end, std::function<bool(const Function*)> functor) const;

private:
	bool find_containing_function(edb::address_t address, Function *function) const;
	bool is_thunk(edb::address_t address) const;
	bool will_return(edb::address_t address) const;
	void bonus_entry_point(RegionData *data) const;
	void bonus_main(RegionData *data) const;
	void bonus_marked_functions(RegionData *data);
	void bonus_symbols(RegionData *data);
	void collect_functions(RegionData *data);
	void collect_fuzzy_functions(RegionData *data);
	void do_analysis(const std::shared_ptr<IRegion> &region);
	void ident_header(Analyzer::RegionData *data);
	void invalidate_dynamic_analysis(const std::shared_ptr<IRegion> &region);
	void set_function_types(FunctionMap *results);
	void set_function_types_helper(Function &function) const;
	QString get_analysis_path(const std::shared_ptr<IRegion> &region) const;

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
	struct RegionData {
		QSet<edb::address_t>              known_functions;
		QSet<edb::address_t>              fuzzy_functions;

		FunctionMap                       functions;
		QHash<edb::address_t, BasicBlock> basic_blocks;

		QByteArray                        md5;
		bool                              fuzzy;
		std::shared_ptr<IRegion>                  region;

		// a copy of the whole region
		QVector<quint8>                   memory;
	};

	QMenu                             *menu_;
	QHash<edb::address_t, RegionData>  analysis_info_;
	QSet<edb::address_t>               specified_functions_;
	AnalyzerWidget                    *analyzer_widget_;
};

}

#endif

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

class Analyzer final : public QObject, public IAnalyzer, public IPlugin {
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "edb.IPlugin/1.0")
	Q_INTERFACES(IPlugin)
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")

private:
	struct RegionData;

public:
	explicit Analyzer(QObject *parent = nullptr);

public:
    QMenu *menu(QWidget *parent = nullptr) override;
	QList<QAction *> cpu_context_menu() override;

private:
	void private_init() override;
	QWidget *options_page() override;

public:
	AddressCategory category(edb::address_t address) const override;
	FunctionMap functions(const std::shared_ptr<IRegion> &region) const override;
	FunctionMap functions() const override;
	QSet<edb::address_t> specified_functions() const override { return specified_functions_; }
	Result<edb::address_t, QString> find_containing_function(edb::address_t address) const override;
	void analyze(const std::shared_ptr<IRegion> &region) override;
	void invalidate_analysis() override;
	void invalidate_analysis(const std::shared_ptr<IRegion> &region) override;
	bool for_funcs_in_range(const edb::address_t start, const edb::address_t end, std::function<bool(const Function*)> functor) const override;

private:
	bool find_containing_function(edb::address_t address, Function *function) const;
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

Q_SIGNALS:
	void update_progress(int);

public Q_SLOTS:
	void do_ip_analysis();
	void do_view_analysis();
	void goto_function_start();
	void goto_function_end();
	void mark_function_start();
	void show_xrefs();
	void show_specified();

private:
	struct RegionData {
		QSet<edb::address_t>              known_functions;
		QSet<edb::address_t>              fuzzy_functions;

		FunctionMap                       functions;
		QHash<edb::address_t, BasicBlock> basic_blocks;

		QByteArray                        md5;
		bool                              fuzzy;
		std::shared_ptr<IRegion>          region;

		// a copy of the whole region
		QVector<quint8>                   memory;
	};

	QMenu                             *menu_ = nullptr;
	AnalyzerWidget                    *analyzer_widget_ = nullptr;
	QHash<edb::address_t, RegionData>  analysis_info_;
	QSet<edb::address_t>               specified_functions_;
};

}

#endif

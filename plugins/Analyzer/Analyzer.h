/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ANALYZER_H_20080630_
#define ANALYZER_H_20080630_

#include "BasicBlock.h"
#include "IAnalyzer.h"
#include "IPlugin.h"
#include "IRegion.h"
#include "Symbol.h"
#include "Types.h"

#include <QHash>
#include <QList>
#include <QMap>
#include <QSet>
#include <QVector>

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
	[[nodiscard]] QMenu *menu(QWidget *parent = nullptr) override;
	[[nodiscard]] QList<QAction *> cpuContextMenu() override;

private:
	void privateInit() override;
	[[nodiscard]] QWidget *optionsPage() override;

public:
	[[nodiscard]] AddressCategory category(edb::address_t address) const override;
	[[nodiscard]] FunctionMap functions() const override;
	[[nodiscard]] FunctionMap functions(const std::shared_ptr<IRegion> &region) const override;
	[[nodiscard]] QSet<edb::address_t> specifiedFunctions() const override { return specifiedFunctions_; }
	[[nodiscard]] Result<edb::address_t, QString> findContainingFunction(edb::address_t address) const override;
	bool forFuncsInRange(edb::address_t start, edb::address_t end, std::function<bool(const Function *)> functor) const override;
	void analyze(const std::shared_ptr<IRegion> &region) override;
	void invalidateAnalysis() override;
	void invalidateAnalysis(const std::shared_ptr<IRegion> &region) override;

private:
	[[nodiscard]] bool findContainingFunction(edb::address_t address, Function *function) const;
	void bonusEntryPoint(RegionData *data) const;
	void bonusMain(RegionData *data) const;
	void bonusMarkedFunctions(RegionData *data);
	void bonusSymbols(RegionData *data);
	void collectFunctions(RegionData *data);
	void computeNonReturning(RegionData *data);
	void splitBlocks(RegionData *data);
	void collectFuzzyFunctions(RegionData *data);
	void doAnalysis(const std::shared_ptr<IRegion> &region);
	void identHeader(Analyzer::RegionData *data);
	void invalidateDynamicAnalysis(const std::shared_ptr<IRegion> &region);

Q_SIGNALS:
	void updateProgress(int);

public Q_SLOTS:
	void doIpAnalysis();
	void doViewAnalysis();
	void gotoFunctionStart();
	void gotoFunctionEnd();
	void markFunctionStart();
	void showXrefs();
	void showSpecified();

private:
	struct RegionData {
		QSet<edb::address_t> knownFunctions;
		QSet<edb::address_t> fuzzyFunctions;

		FunctionMap functions;
		QHash<edb::address_t, BasicBlock> basicBlocks;

		QByteArray md5;
		bool fuzzy;
		std::shared_ptr<IRegion> region;

		// a copy of the whole region
		QVector<uint8_t> memory;
	};

	QMenu *menu_                    = nullptr;
	AnalyzerWidget *analyzerWidget_ = nullptr;
	QHash<edb::address_t, RegionData> analysisInfo_;
	QSet<edb::address_t> specifiedFunctions_;
};

}

#endif

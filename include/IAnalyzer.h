/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef IANALYZER_H_20080630_
#define IANALYZER_H_20080630_

#include "Function.h"
#include "Types.h"
#include <QSet>
#include <functional>
#include <memory>

class IRegion;

template <class T, class E>
class Result;

class IAnalyzer {
public:
	virtual ~IAnalyzer() = default;

public:
	using FunctionMap = QMap<edb::address_t, Function>;

public:
	enum AddressCategory {
		ADDRESS_FUNC_UNKNOWN = 0x00,
		ADDRESS_FUNC_START   = 0x01,
		ADDRESS_FUNC_BODY    = 0x02,
		ADDRESS_FUNC_END     = 0x04
	};

public:
	[[nodiscard]] virtual AddressCategory category(edb::address_t address) const              = 0;
	[[nodiscard]] virtual FunctionMap functions(const std::shared_ptr<IRegion> &region) const = 0;
	[[nodiscard]] virtual FunctionMap functions() const                                       = 0;
	[[nodiscard]] virtual QSet<edb::address_t> specifiedFunctions() const { return {}; }
	[[nodiscard]] virtual Result<edb::address_t, QString> findContainingFunction(edb::address_t address) const                  = 0;
	virtual void analyze(const std::shared_ptr<IRegion> &region)                                                                = 0;
	virtual void invalidateAnalysis()                                                                                           = 0;
	virtual void invalidateAnalysis(const std::shared_ptr<IRegion> &region)                                                     = 0;
	virtual bool forFuncsInRange(edb::address_t start, edb::address_t end, std::function<bool(const Function *)> functor) const = 0;
};

#endif

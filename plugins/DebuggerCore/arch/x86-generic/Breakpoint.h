/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef X86_BREAKPOINT_H_20060720_
#define X86_BREAKPOINT_H_20060720_

#include "IBreakpoint.h"
#include "Util.h"
#include <QCoreApplication>
#include <array>
#include <vector>

namespace DebuggerCorePlugin {

class Breakpoint final : public IBreakpoint {
	Q_DECLARE_TR_FUNCTIONS(Breakpoint)
public:
	enum class TypeId : int {
		Automatic = static_cast<int>(IBreakpoint::TypeId::Automatic),
		INT3,
		INT1,
		HLT,
		CLI,
		STI,
		INSB,
		INSD,
		OUTSB,
		OUTSD,
		UD2,
		UD0,

		TYPE_COUNT
	};

	using Type = util::AbstractEnumData<IBreakpoint::TypeId, TypeId>;

public:
	explicit Breakpoint(edb::address_t address);
	~Breakpoint() override;

public:
	[[nodiscard]] edb::address_t address() const override { return address_; }
	[[nodiscard]] uint64_t hitCount() const override { return hitCount_; }
	[[nodiscard]] bool enabled() const override { return enabled_; }
	[[nodiscard]] bool oneTime() const override { return oneTime_; }
	[[nodiscard]] bool internal() const override { return internal_; }
	[[nodiscard]] size_t size() const override { return originalBytes_.size(); }
	[[nodiscard]] const uint8_t *originalBytes() const override { return originalBytes_.data(); }
	[[nodiscard]] IBreakpoint::TypeId type() const override { return type_; }

	[[nodiscard]] static std::vector<BreakpointType> supportedTypes();
	[[nodiscard]] static std::vector<size_t> possibleRewindSizes();

public:
	bool enable() override;
	bool disable() override;
	void hit() override;
	void setOneTime(bool value) override;
	void setInternal(bool value) override;
	void setType(IBreakpoint::TypeId type) override;

private:
	std::vector<uint8_t> originalBytes_;
	edb::address_t address_;
	uint64_t hitCount_ = 0;
	bool enabled_      = false;
	bool oneTime_      = false;
	bool internal_     = false;
	Type type_;
};

}

#endif

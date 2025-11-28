/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef STATE_H_20060715_
#define STATE_H_20060715_

#include "API.h"
#include "Types.h"
#include <memory>

class IState;
class Register;
class QString;

namespace DebuggerCorePlugin {
class DebuggerCore;
class PlatformThread;
}

// NOTE(eteran): the purpose of this class is simply to give IState objects
// value semantics, so it tends to nearly replicate that interface.
class EDB_EXPORT State {

	// TODO(eteran): I don't like needing to do this
	// need to revisit the IState/State/PlatformState stuff...
	friend class DebuggerCorePlugin::DebuggerCore;
	friend class DebuggerCorePlugin::PlatformThread;

public:
	State();
	State(const State &other);
	State &operator=(const State &rhs);
	State(State &&other) noexcept;
	State &operator=(State &&rhs) noexcept;
	~State();

public:
	void swap(State &other);

public:
	[[nodiscard]] bool empty() const;
	[[nodiscard]] edb::address_t framePointer() const;
	[[nodiscard]] edb::address_t instructionPointer() const;
	[[nodiscard]] edb::address_t stackPointer() const;
	[[nodiscard]] edb::reg_t debugRegister(size_t n) const;
	[[nodiscard]] edb::reg_t flags() const;
	[[nodiscard]] QString flagsToString() const;
	[[nodiscard]] QString flagsToString(edb::reg_t flags) const;
	[[nodiscard]] Register archRegister(uint64_t type, size_t n) const;
	[[nodiscard]] Register flagsRegister() const;
	[[nodiscard]] Register gpRegister(size_t n) const;
	[[nodiscard]] Register instructionPointerRegister() const;
	[[nodiscard]] Register value(const QString &reg) const;
	void adjustStack(int bytes);
	void clear();

public:
#if defined(EDB_X86) || defined(EDB_X86_64)
	[[nodiscard]] int fpuStackPointer() const;
	[[nodiscard]] edb::value80 fpuRegister(size_t n) const;
	[[nodiscard]] bool fpuRegisterIsEmpty(std::size_t n) const;
	[[nodiscard]] QString fpuRegisterTagString(std::size_t n) const;
	[[nodiscard]] edb::value16 fpuControlWord() const;
	[[nodiscard]] edb::value16 fpuStatusWord() const;
	[[nodiscard]] edb::value16 fpuTagWord() const;
#endif

public:
	void setDebugRegister(size_t n, edb::reg_t value);
	void setFlags(edb::reg_t flags);
	void setInstructionPointer(edb::address_t value);
	void setRegister(const QString &name, edb::reg_t value);
	void setRegister(const Register &reg);

public:
	[[nodiscard]] Register operator[](const QString &reg) const;

private:
	std::unique_ptr<IState> impl_;
};

Q_DECLARE_METATYPE(State)

#endif

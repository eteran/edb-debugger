/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FORMATTER_H_20191119_
#define FORMATTER_H_20191119_

#include "API.h"
#include <string>
class QString;

namespace CapstoneEDB {

class Operand;
class Instruction;

class EDB_EXPORT Formatter {
public:
	enum Syntax {
		SyntaxIntel,
		SyntaxAtt
	};

	enum Capitalization {
		UpperCase,
		LowerCase
	};

	struct FormatOptions {
		Syntax syntax;
		Capitalization capitalization;
		bool tabBetweenMnemonicAndOperands;
		bool simplifyRIPRelativeTargets;
	};

public:
	[[nodiscard]] std::string toString(const Instruction &insn) const;
	[[nodiscard]] std::string toString(const Operand &operand) const;
	[[nodiscard]] std::string registerName(unsigned int reg) const;

	[[nodiscard]] FormatOptions options() const {
		return options_;
	}

	void setOptions(const FormatOptions &options);

private:
	void checkCapitalize(std::string &str, bool canContainHex = true) const;
	[[nodiscard]] QString adjustInstructionText(const Instruction &insn) const;

private:
	FormatOptions options_ = {SyntaxIntel, LowerCase, false, true};
};

}

#endif


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
	std::string toString(const Instruction &insn) const;
	std::string toString(const Operand &operand) const;
	std::string registerName(unsigned int reg) const;

	FormatOptions options() const {
		return options_;
	}

	void setOptions(const FormatOptions &options);

private:
	void checkCapitalize(std::string &str, bool canContainHex = true) const;
	QString adjustInstructionText(const Instruction &instruction) const;

private:
	FormatOptions options_ = {SyntaxIntel, LowerCase, false, true};
};

}

#endif


#ifndef OPERAND_H_
#define OPERAND_H_

#include <capstone/capstone.h>

namespace CapstoneEDB {

class Formatter;
class Instruction;

// NOTE(eteran): Operand is a non-owning class that exists for API purposes
//               it doesn't make sense to store instances of it long term as
//               they are only valid as long as the associated Instruction
//               object is
class Operand {
	friend class Formatter;
	friend class Instruction;

private:
	Operand(const Instruction *instruction, cs_x86_op *operand, size_t index) : owner_(instruction), operand_(operand), index_(index) {
	}

public:
	Operand() : owner_(nullptr), operand_(nullptr), index_(0) {
	}

	Operand(const Operand &)            = default;
	Operand &operator=(const Operand &) = default;
	~Operand()                          = default;

public:
	bool valid() const                  { return operand_; }
	explicit operator bool() const      { return valid();  }
	const cs_x86_op *operator->() const { return operand_; }
	const cs_x86_op *native() const     { return operand_; }
	int index() const                   { return index_;   }

public:
	const Instruction *owner() const {
		return owner_;
	}

private:
	const Instruction *owner_;
	cs_x86_op *        operand_;
	size_t             index_;
};

}

#endif

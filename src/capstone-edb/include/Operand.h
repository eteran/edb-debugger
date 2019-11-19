
#ifndef OPERAND_H_20191119_
#define OPERAND_H_20191119_

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

#if defined(EDB_X86) || defined(EDB_X86_64)
	using op_type = cs_x86_op;
#elif defined(EDB_ARM32) || defined(EDB_ARM64)
	using op_type = cs_arm_op;
#endif

private:
	Operand(const Instruction *instruction, op_type *operand, size_t index)
		: owner_(instruction), operand_(operand), index_(index) {
	}

public:
	Operand()                = default;
	Operand(const Operand &) = default;
	Operand &operator=(const Operand &) = default;
	~Operand()                          = default;

public:
	bool valid() const { return operand_; }
	explicit operator bool() const { return valid(); }
	const op_type *operator->() const { return operand_; }
	const op_type *native() const { return operand_; }
	size_t index() const { return index_; }

public:
	const Instruction *owner() const {
		return owner_;
	}

private:
	const Instruction *owner_ = nullptr;
	op_type *operand_         = nullptr;
	size_t index_             = 0;
};

}

#endif

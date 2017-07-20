
// based on code from mozilla: https://github.com/mozilla/rr/blob/master/src/Registers.cc

#ifndef REGISTER_REF_H_
#define REGISTER_REF_H_

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <climits>

namespace DebuggerCorePlugin {

class RegisterRef {
public:
	constexpr RegisterRef() : name(nullptr), offset(0), nbytes(0), comparison_mask(0) {
	}

	RegisterRef(const char *name_, std::size_t offset_, std::size_t nbytes_) : name(name_), offset(offset_), nbytes(nbytes_) {
		comparison_mask = mask_for_nbytes(nbytes_);
	}

	RegisterRef(const char *name_, std::size_t offset_, std::size_t nbytes_, std::uint64_t comparison_mask_) : name(name_), offset(offset_), nbytes(nbytes_), comparison_mask(comparison_mask_) {
		// Ensure no bits are set outside of the register's bitwidth.
		assert((comparison_mask_ & ~mask_for_nbytes(nbytes_)) == 0);
	}
	
	RegisterRef(const RegisterRef &) = default;
	
	RegisterRef &operator=(const RegisterRef &) = default;

	// Returns a pointer to the register in |regs| represented by |offset|.
	// |regs| is assumed to be a pointer to the user_struct_regs for the
	// appropriate architecture.
	void *pointer_into(void *regs) const {
		return static_cast<char *>(regs) + offset;
	}

	const void *pointer_into(const void *regs) const {
		return static_cast<const char *>(regs) + offset;
	}

public:
	static std::uint64_t mask_for_nbytes(std::size_t nbytes) {
		assert(nbytes <= sizeof(comparison_mask));
		return ((nbytes == sizeof(comparison_mask)) ? std::uint64_t(0) : (std::uint64_t(1) << nbytes * CHAR_BIT)) - 1;
	}

public:
	// The name of this register.
	const char *name;

	// The offsetof the register in state structure.
	std::size_t offset;

	// The size of the register.  0 means we cannot read it.
	std::size_t nbytes;

	// Mask to be applied to register values prior to comparing them.  Will
	// typically be ((1 << nbytes) - 1), but some registers may have special
	// comparison semantics.
	//
	// NOTE(eteran): there is a max of 64-bits on the mask, for larger registers
	// we should set this to zero and let the consumer wory about which bits 
	// are valid
	std::uint64_t comparison_mask;
};

}

#endif

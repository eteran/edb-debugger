
// inspired from code from mozilla: https://github.com/mozilla/rr/blob/master/src/Registers.cc

#ifndef REGISTER_REF_H_20191119_
#define REGISTER_REF_H_20191119_

#include <cassert>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>

class RegisterRef {
public:
	constexpr RegisterRef(const char *name, const void *ptr, std::size_t size)
		: name_(name), ptr_(ptr), size_(size) {
	}

	constexpr RegisterRef()          = default;
	RegisterRef(const RegisterRef &) = default;
	RegisterRef &operator=(const RegisterRef &) = default;

public:
	const void *data() const { return ptr_; }

public:
	bool operator==(const RegisterRef &rhs) const { return size_ == rhs.size_ && std::memcmp(ptr_, rhs.ptr_, size_) == 0; }
	bool operator!=(const RegisterRef &rhs) const { return size_ != rhs.size_ || std::memcmp(ptr_, rhs.ptr_, size_) != 0; }

public:
	bool valid() const { return ptr_ != nullptr; }

public:
	// The name of this register.
	const char *name_ = nullptr;

	// The ptr to register in state structure.
	const void *ptr_ = nullptr;

	// The size of the register. 0 means we cannot read it.
	std::size_t size_ = 0;
};

#endif

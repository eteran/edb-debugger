
#ifndef VALUE_H_
#define VALUE_H_

#include "API.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>

#include <QString>
#include <QVariant>

class Register;

#ifdef _MSC_VER
extern "C" EDB_EXPORT void __fastcall long_double_to_double(const void* src, double* dest);
#endif

namespace edb {

namespace v1 {
extern bool debuggeeIs32Bit();
}

namespace detail {

template <size_t N>
class large_value_type {
public:
	using T = uint64_t[N / 8];
public:
	// all defaulted to help ensure that this is a trivially-copyable type
	large_value_type()                                   = default;
	large_value_type(const large_value_type &)            = default;
	large_value_type& operator=(const large_value_type &) = default;
	large_value_type(large_value_type &&)                 = default;
	large_value_type& operator=(large_value_type &&)      = default;
	~large_value_type()                                  = default;

public:
	template <class Data, class = typename std::enable_if<!std::is_floating_point<Data>::value && !std::is_integral<Data>::value>::type>
	explicit large_value_type(const Data &data, size_t offset = 0) {

		static_assert(sizeof(Data) >= sizeof(T), "value_type can only be constructed from large enough variable");
		static_assert(std::is_trivially_copyable<Data>::value, "value_type can only be constructed from trivially copiable data");

		Q_ASSERT(sizeof(Data) - offset >= sizeof(T)); // check bounds, this can't be done at compile time

		auto dataStart = reinterpret_cast<const char *>(&data);
		std::memcpy(&value_, dataStart + offset, sizeof(value_));
	}

public:
	bool operator==(const large_value_type &rhs) const { return memcmp(value_, rhs.value_, sizeof(T)) == 0; }
	bool operator!=(const large_value_type &rhs) const { return memcmp(value_, rhs.value_, sizeof(T)) != 0; }

public:
	QString toHexString() const {
		char buf[1024];
		char *p = buf;
		auto ptr = reinterpret_cast<const char *>(value_);
		auto end = ptr + sizeof(T);

		for(auto it = end; it != (ptr - 1); --it) {
			p += sprintf(p, "%02x", *it & 0xff);
		}

		return QString::fromLatin1(buf);
	}

public:
	template <class U>
	static large_value_type fromZeroExtended(const U &data) {
		large_value_type created;
		created.copyZeroExtended(data);
		return created;
	}

private:
	template <class U>
	void copyZeroExtended(const U& data) {
		// TODO(eteran): this seems to be more complex than I think we need
		// since this is an unsigned type, I suspect that just assigning would be enough so long as it would fit...
		static_assert(sizeof(U) <= sizeof(T), "It doesn't make sense to expand a larger type into a smaller type");

		auto dataStart = reinterpret_cast<const char*>(&data);
		auto target    = reinterpret_cast<char*>(&value_);

		std::memcpy(target, dataStart, sizeof(data));
		std::memset(target + sizeof(data), 0, sizeof(value_) - sizeof(data));
	}

public:
	T value_ = {};
};

template <class Integer>
using IsInteger = typename std::enable_if<std::is_integral<Integer>::value>::type;

template <class T1, class T2>
using PromoteType = typename std::conditional<
	sizeof(T1) >= sizeof(T2),
	typename std::make_unsigned<T1>::type,
	typename std::make_unsigned<T2>::type
>::type;

template <class T>
class value_type {
	template <class U>
	friend class value_type;

public:
	using InnerValueType = T;

public:
	// all defaulted to help ensure that this is a trivially-copyable type
	value_type()                              = default;
	value_type(const value_type &)            = default;
	value_type& operator=(const value_type &) = default;
	value_type(value_type &&)                 = default;
	value_type& operator=(value_type &&)      = default;
	~value_type()                             = default;

public:
	template <class Integer, class = IsInteger<Integer>>
	value_type(Integer n) : value_(n) {
		// NOTE(eteran): this is allowed to truncate like assigning a uint64_t to a uint32_t
	}

	template <class Integer, class = IsInteger<Integer>>
	value_type& operator=(const Integer &rhs) {
		value_ = rhs;
		// NOTE(eteran): this is allowed to truncate like assigning a uint64_t to a uint32_t
		return *this;
	}

public:
	template <class U>
	explicit value_type(const value_type<U> &other) : value_(other.value_) {
		// NOTE(eteran): this is allowed to truncate like assigning a uint64_t to a uint32_t
	}

	template <class U>
	value_type& operator=(const value_type<U> &rhs) {
		value_ = rhs.value_;
		// NOTE(eteran): this is allowed to truncate like assigning a uint64_t to a uint32_t
		return *this;
	}

public:
	template <class Data, class = typename std::enable_if<!std::is_floating_point<Data>::value && !std::is_integral<Data>::value>::type>
	explicit value_type(const Data &data, size_t offset = 0) {

		static_assert(sizeof(Data) >= sizeof(T), "value_type can only be constructed from large enough variable");
		static_assert(std::is_trivially_copyable<Data>::value, "value_type can only be constructed from trivially copiable data");

		Q_ASSERT(sizeof(Data) - offset >= sizeof(T)); // check bounds, this can't be done at compile time

		auto dataStart = reinterpret_cast<const char *>(&data);
		std::memcpy(&value_, dataStart + offset, sizeof(value_));
	}

public:
	static value_type fromString(const QString &str, bool *ok = nullptr, int base = 10, bool isSigned = false) {

		const qulonglong v = isSigned ? str.toLongLong(ok, base) : str.toULongLong(ok, base);

		if(ok && !*ok) {
			return value_type(0);
		}

		// Check that the result fits into the underlying type
		value_type result(v);
		if(result == v) {
			return result;
		}

		if(ok) {
			*ok = false;
		}

		return value_type(0);
	}

	static value_type fromHexString(const QString &str, bool *ok = nullptr) {
		return fromString(str, ok, 16);
	}

	static value_type fromSignedString(const QString &str, bool *ok = nullptr) {
		return fromString(str, ok, 10, true);
	}

	static value_type fromCString(const QString &str, bool* ok = nullptr) {
		return fromString(str, ok, 0);
	}

	template <class U>
	static value_type fromZeroExtended(const U &data) {
		value_type created;
		created.copyZeroExtended(data);
		return created;
	}

public:
	void swap(value_type &other) {
		std::swap(value_, other.value_);
	}

public:
	bool negative() const {
		return typename std::make_signed<T>::type(value_) < 0;
	}

public:
	explicit operator bool() const { return value_ != 0; }
	bool operator!() const         { return !value_; }
	operator T() const             { return value_; }
	T toUint() const               { return value_; }
	T& asUint()                    { return value_; }

public:
	value_type operator++(int) {
		T v(value_);
		++value_;
		return v;
	}

	value_type& operator++() {
		++value_;
		return *this;
	}

	value_type operator--(int) {
		T v(value_);
		--value_;
		return v;
	}

	value_type& operator--() {
		--value_;
		return *this;
	}

public:
	template <class U>
	value_type& operator+=(const value_type<U> &rhs) {
		value_ += rhs.value_;
		return *this;
	}

	template <class U>
	value_type& operator-=(const value_type<U> &rhs) {
		value_ -= rhs.value_;
		return *this;
	}

	template <class U>
	value_type& operator*=(const value_type<U> &rhs) {
		value_ *= rhs.value_;
		return *this;
	}

	template <class U>
	value_type& operator/=(const value_type<U> &rhs) {
		value_ /= rhs.value_;
		return *this;
	}

	template <class U>
	value_type& operator%=(const value_type<U> &rhs) {
		value_ %= rhs.value_;
		return *this;
	}

public:
	template <class U>
	value_type& operator&=(const value_type<U> &rhs) {
		value_ &= rhs.value_;
		return *this;
	}

	template <class U>
	value_type& operator|=(const value_type<U> &rhs) {
		value_ |= rhs.value_;
		return *this;
	}

	template <class U>
	value_type& operator^=(const value_type<U> &rhs) {
		value_ ^= rhs.value_;
		return *this;
	}

	template <class U>
	value_type& operator>>=(const value_type<U> &rhs) {
		value_ >>= rhs.value_;
		return *this;
	}

	template <class U>
	value_type& operator<<=(const value_type<U> &rhs) {
		value_ <<= rhs.value_;
		return *this;
	}

public:
	template <class Integer, class = IsInteger<Integer>>
	value_type& operator+=(Integer n) {
		value_ += n;
		return *this;
	}

	template <class Integer, class = IsInteger<Integer>>
	value_type& operator-=(Integer n) {
		value_ -= n;
		return *this;
	}

	template <class Integer, class = IsInteger<Integer>>
	value_type& operator*=(Integer n) {
		value_ *= n;
		return *this;
	}

	template <class Integer, class = IsInteger<Integer>>
	value_type& operator/=(Integer n) {
		value_ /= n;
		return *this;
	}

	template <class Integer, class = IsInteger<Integer>>
	value_type& operator%=(Integer n) {
		value_ %= n;
		return *this;
	}

public:
	template <class Integer, class = IsInteger<Integer>>
	value_type& operator&=(Integer n) {
		value_ &= n;
		return *this;
	}

	template <class Integer, class = IsInteger<Integer>>
	value_type& operator|=(Integer n) {
		value_ |= n;
		return *this;
	}

	template <class Integer, class = IsInteger<Integer>>
	value_type& operator^=(Integer n) {
		value_ ^= n;
		return *this;
	}

	template <class Integer, class = IsInteger<Integer>>
	value_type& operator>>=(Integer n) {
		value_ >>= n;
		return *this;
	}

	template <class Integer, class = IsInteger<Integer>>
	value_type& operator<<=(Integer n) {
		value_ <<= n;
		return *this;
	}

public:
	QString toPointerString(bool createdFromNativePointer = true) const {
		if (edb::v1::debuggeeIs32Bit()) {
			return "0x" + toHexString();
		} else {
			if (!createdFromNativePointer) { // then we don't know value of upper dword
				return "0x????????" + value_type<uint32_t>(value_).toHexString();
			} else {
				return "0x" + toHexString();
			}
		}
	}

	QString toHexString() const {
		std::ostringstream ss;
		ss << std::setw(sizeof(value_) * 2) << std::setfill('0') << std::hex << +value_; // + to prevent printing uint8_t as a character
		return QString::fromStdString(ss.str());
	}

	QString unsignedToString() const {
		return toString();
	}

	QString signedToString() const {
		return QString("%1").arg(typename std::make_signed<T>::type(value_));
	}

	QString toString() const {
		return QString("%1").arg(value_);
	}

	QVariant toQVariant() const {
		return QVariant::fromValue(value_);
	}

public:
	value_type signExtended(size_t valueLength) const {
		value_type result(value_);

		if(valueLength == sizeof(value_)) {
			return result;
		}

		// if the sign bit is set
		if(value_ & (1ull << (valueLength * 8 - 1))) {
			// start with all bits set
			result.value_ = -1ll;

			// overwrite the lower <valueLength> bytes with the original value
			std::memcpy(&result.value_, &value_, valueLength);
		}

		return result;
	}

public:
	void normalize() {
		if (edb::v1::debuggeeIs32Bit()) {
			value_ &= 0xffffffffull;
		}
	}

private:
	template <class U>
	void copyZeroExtended(const U& data) {
		// TODO(eteran): this seems to be more complex than I think we need
		// since this is an unsigned type, I suspect that just assigning would be enough so long as it would fit...
		static_assert(sizeof(U) <= sizeof(T), "It doesn't make sense to expand a larger type into a smaller type");

		auto dataStart = reinterpret_cast<const char*>(&data);
		auto target    = reinterpret_cast<char*>(&value_);

		std::memcpy(target, dataStart, sizeof(data));
		std::memset(target + sizeof(data), 0, sizeof(value_) - sizeof(data));
	}

public:
	T value_ = {};
};


// operators for value_type, Integer
template <class T, class Integer, class = IsInteger<Integer>>
bool operator==(const value_type<T> &lhs, Integer rhs) {
	using U = typename std::make_unsigned<Integer>::type;
	return lhs.value_ == static_cast<U>(rhs);
}

template <class T, class Integer, class = IsInteger<Integer>>
bool operator!=(const value_type<T> &lhs, Integer rhs) {
	using U = typename std::make_unsigned<Integer>::type;
	return lhs.value_ != static_cast<U>(rhs);
}

template <class T, class Integer, class = IsInteger<Integer>>
auto operator+(const value_type<T> &lhs, Integer rhs) -> value_type<T> {
	value_type<T> r(lhs);
	r += rhs;
	return r;
}

template <class T, class Integer, class = IsInteger<Integer>>
auto operator-(const value_type<T> &lhs, Integer rhs) -> value_type<T> {
	value_type<T> r(lhs);
	r -= rhs;
	return r;
}

template <class T, class Integer, class = IsInteger<Integer>>
auto operator*(const value_type<T> &lhs, Integer rhs) -> value_type<T> {
	value_type<T> r(lhs);
	r *= rhs;
	return r;
}

template <class T, class Integer, class = IsInteger<Integer>>
auto operator/(const value_type<T> &lhs, Integer rhs) -> value_type<T> {
	value_type<T> r(lhs);
	r /= rhs;
	return r;
}

template <class T, class Integer, class = IsInteger<Integer>>
auto operator%(const value_type<T> &lhs, Integer rhs) -> value_type<T> {
	value_type<T> r(lhs);
	r %= rhs;
	return r;
}

template <class T, class Integer, class = IsInteger<Integer>>
auto operator&(const value_type<T> &lhs, Integer rhs) -> value_type<T> {
	value_type<T> r(lhs);
	r &= rhs;
	return r;
}

template <class T, class Integer, class = IsInteger<Integer>>
auto operator|(const value_type<T> &lhs, Integer rhs) -> value_type<T> {
	value_type<T> r(lhs);
	r |= rhs;
	return r;
}

template <class T, class Integer, class = IsInteger<Integer>>
auto operator^(const value_type<T> &lhs, Integer rhs) -> value_type<T> {
	value_type<T> r(lhs);
	r ^= rhs;
	return r;
}

template <class T, class Integer, class = IsInteger<Integer>>
auto operator>>(const value_type<T> &lhs, Integer rhs) -> value_type<T> {
	value_type<T> r(lhs);
	r >>= rhs;
	return r;
}

template <class T, class Integer, class = IsInteger<Integer>>
auto operator<<(const value_type<T> &lhs, Integer rhs) -> value_type<T> {
	value_type<T> r(lhs);
	r <<= rhs;
	return r;
}

// operators for Integer, value_type
template <class T, class Integer, class = IsInteger<Integer>>
bool operator==(Integer lhs, const value_type<T> &rhs) {
	return rhs == lhs;
}

template <class T, class Integer, class = IsInteger<Integer>>
bool operator!=(Integer lhs, const value_type<T> &rhs) {
	return rhs != lhs;
}

template <class T, class Integer, class = IsInteger<Integer>>
auto operator+(Integer lhs, const value_type<T> &rhs) -> value_type<PromoteType<T, Integer>> {

	using U = value_type<PromoteType<T, Integer>>;

	value_type<U> r(lhs);
	r += rhs;
	return r;
}

template <class T, class Integer, class = IsInteger<Integer>>
auto operator-(Integer lhs, const value_type<T> &rhs) -> value_type<PromoteType<T, Integer>> {

	using U = value_type<PromoteType<T, Integer>>;

	value_type<U> r(lhs);
	r -= rhs;
	return r;
}

template <class T, class Integer, class = IsInteger<Integer>>
auto operator*(Integer lhs, const value_type<T> &rhs) -> value_type<PromoteType<T, Integer>> {

	using U = value_type<PromoteType<T, Integer>>;

	value_type<U> r(lhs);
	r *= rhs;
	return r;
}

template <class T, class Integer, class = IsInteger<Integer>>
auto operator/(Integer lhs, const value_type<T> &rhs) -> value_type<PromoteType<T, Integer>> {

	using U = value_type<PromoteType<T, Integer>>;

	value_type<U> r(lhs);
	r /= rhs;
	return r;
}

template <class T, class Integer, class = IsInteger<Integer>>
auto operator%(Integer lhs, const value_type<T> &rhs) -> value_type<PromoteType<T, Integer>> {

	using U = value_type<PromoteType<T, Integer>>;

	value_type<U> r(lhs);
	r %= rhs;
	return r;
}

template <class T, class Integer, class = IsInteger<Integer>>
auto operator&(Integer lhs, const value_type<T> &rhs) -> value_type<PromoteType<T, Integer>> {

	using U = value_type<PromoteType<T, Integer>>;

	value_type<U> r(lhs);
	r &= rhs;
	return r;
}

template <class T, class Integer, class = IsInteger<Integer>>
auto operator|(Integer lhs, const value_type<T> &rhs) -> value_type<PromoteType<T, Integer>> {

	using U = value_type<PromoteType<T, Integer>>;

	value_type<U> r(lhs);
	r |= rhs;
	return r;
}

template <class T, class Integer, class = IsInteger<Integer>>
auto operator^(Integer lhs, const value_type<T> &rhs) -> value_type<PromoteType<T, Integer>> {

	using U = value_type<PromoteType<T, Integer>>;

	value_type<U> r(lhs);
	r ^= rhs;
	return r;
}

template <class T, class Integer, class = IsInteger<Integer>>
auto operator>>(Integer lhs, const value_type<T> &rhs) -> value_type<PromoteType<T, Integer>> {

	using U = value_type<PromoteType<T, Integer>>;

	value_type<U> r(lhs);
	r >>= rhs;
	return r;
}

template <class T, class Integer, class = IsInteger<Integer>>
auto operator<<(Integer lhs, const value_type<T> &rhs) -> value_type<PromoteType<T, Integer>> {

	using U = value_type<PromoteType<T, Integer>>;

	value_type<U> r(lhs);
	r <<= rhs;
	return r;
}

// operators for value_type, value_type
template <class T1, class T2>
bool operator==(const value_type<T1> &lhs, const value_type<T2> &rhs) {
	return lhs.value_ == rhs.value_;
}

template <class T1, class T2>
bool operator!=(const value_type<T1> &lhs, const value_type<T2> &rhs) {
	return lhs.value_ == rhs.value_;
}

template <class T1, class T2>
auto operator+(const value_type<T1> &lhs, const value_type<T2> &rhs) -> value_type<PromoteType<T1, T2>> {

	using T = value_type<PromoteType<T1, T2>>;

	value_type<T> r(lhs);
	r += rhs;
	return r;
}

template <class T1, class T2>
auto operator-(const value_type<T1> &lhs, const value_type<T2> &rhs) -> value_type<PromoteType<T1, T2>> {

	using T = value_type<PromoteType<T1, T2>>;

	value_type<T> r(lhs);
	r -= rhs;
	return r;
}

template <class T1, class T2>
auto operator*(const value_type<T1> &lhs, const value_type<T2> &rhs) -> value_type<PromoteType<T1, T2>> {

	using T = value_type<PromoteType<T1, T2>>;

	value_type<T> r(lhs);
	r *= rhs;
	return r;
}

template <class T1, class T2>
auto operator/(const value_type<T1> &lhs, const value_type<T2> &rhs) -> value_type<PromoteType<T1, T2>> {

	using T = value_type<PromoteType<T1, T2>>;

	value_type<T> r(lhs);
	r /= rhs;
	return r;
}

template <class T1, class T2>
auto operator%(const value_type<T1> &lhs, const value_type<T2> &rhs) -> value_type<PromoteType<T1, T2>> {

	using T = value_type<PromoteType<T1, T2>>;

	value_type<T> r(lhs);
	r %= rhs;
	return r;
}

template <class T1, class T2>
auto operator&(const value_type<T1> &lhs, const value_type<T2> &rhs) -> value_type<PromoteType<T1, T2>> {

	using T = value_type<PromoteType<T1, T2>>;

	value_type<T> r(lhs);
	r &= rhs;
	return r;
}

template <class T1, class T2>
auto operator|(const value_type<T1> &lhs, const value_type<T2> &rhs) -> value_type<PromoteType<T1, T2>> {

	using T = value_type<PromoteType<T1, T2>>;

	value_type<T> r(lhs);
	r |= rhs;
	return r;
}

template <class T1, class T2>
auto operator^(const value_type<T1> &lhs, const value_type<T2> &rhs) -> value_type<PromoteType<T1, T2>> {

	using T = value_type<PromoteType<T1, T2>>;

	value_type<T> r(lhs);
	r ^= rhs;
	return r;
}



struct value_type80 {
public:
	using T = uint8_t[10];

public:
	// all defaulted to help ensure that this is a trivially-copyable type
	value_type80()                                = default;
	value_type80(const value_type80 &)            = default;
	value_type80& operator=(const value_type80 &) = default;
	value_type80(value_type80 &&)                 = default;
	value_type80& operator=(value_type80 &&)      = default;
	~value_type80()                               = default;

public:
	template <class Data>
	explicit value_type80(const Data &data, size_t offset = 0) {
		static_assert(sizeof(Data) >= sizeof(value_type80), "ValueBase can only be constructed from large enough variable");
#if defined __GNUG__ && __GNUC__ >= 5 || !defined __GNUG__ || defined __clang__ && __clang_major__*100+__clang_minor__>=306
		static_assert(std::is_trivially_copyable<Data>::value, "ValueBase can only be constructed from trivially copiable data");
#endif

		assert(sizeof(Data) - offset >= sizeof(value_type80)); // check bounds, this can't be done at compile time

		auto dataStart = reinterpret_cast<const char*>(&data);
		std::memcpy(&value_, dataStart + offset, sizeof(value_));
	}

public:
	bool negative() const {
		return value_[9] & 0x80;
	}

	value_type<uint16_t> exponent() const {
		value_type<uint16_t> e(value_, 8);
		e &= 0x7fff;
		return e;
	}

	value_type<uint64_t> mantissa() const {
		value_type<uint64_t> m(value_, 0);
		return m;
	}

	bool normalized() const {
		return value_[7] & 0x80;
	}

public:
	long double toFloatValue() const {
#ifdef _MSC_VER
		double d;
		long_double_to_double(&value_, &d);
		return d;
#else
		long double float80val;
		std::memcpy(&float80val, &value_, sizeof(value_));
		return float80val;
#endif
	}

public:
	QString toHexString() const {
		char buf[32];
		snprintf(buf, sizeof(buf), "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
			value_[9],
			value_[8],
			value_[7],
			value_[6],
			value_[5],
			value_[4],
			value_[3],
			value_[2],
			value_[1],
			value_[0]
		);

		return QString::fromLatin1(buf);
	}

public:
	bool operator==(const value_type80 &rhs) const { return memcmp(value_, rhs.value_, 10) == 0; }
	bool operator!=(const value_type80 &rhs) const { return memcmp(value_, rhs.value_, 10) != 0; }

public:
	T value_ = {};
};

static_assert(sizeof(value_type80) * 8 == 80, "value_type80 size is broken!");

}

// GPR on x86
using value8  = detail::value_type<uint8_t>;
using value16 = detail::value_type<uint16_t>;
using value32 = detail::value_type<uint32_t>;

// MMX/GPR(x86_64)
using value64  = detail::value_type<uint64_t>;

// FPU
using value80  = detail::value_type80;

// SSE
using value128 = detail::large_value_type<128>;

// AVX
using value256 = detail::large_value_type<256>;

// AVX512
using value512 = detail::large_value_type<512>;

static_assert(std::is_standard_layout<value8>::value &&
              std::is_standard_layout<value16>::value &&
              std::is_standard_layout<value32>::value &&
              std::is_standard_layout<value64>::value &&
              std::is_standard_layout<value80>::value &&
              std::is_standard_layout<value128>::value &&
              std::is_standard_layout<value256>::value &&
              std::is_standard_layout<value512>::value, "Fixed-sized values are intended to have standard layout");


static_assert(std::is_trivially_copyable<value8>::value &&
			  std::is_trivially_copyable<value16>::value &&
			  std::is_trivially_copyable<value32>::value &&
			  std::is_trivially_copyable<value64>::value &&
			  std::is_trivially_copyable<value80>::value &&
			  std::is_trivially_copyable<value128>::value &&
			  std::is_trivially_copyable<value256>::value &&
			  std::is_trivially_copyable<value512>::value, "Fixed-sized values are intended to be trivially copyable");


}

#endif


#ifndef VALUE_H_20191119_
#define VALUE_H_20191119_

#include "API.h"
#include <array>
#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>

#include <QString>
#include <QVariant>

#ifdef _MSC_VER
extern "C" EDB_EXPORT void __fastcall long_double_to_double(const void *src, double *dest);
EDB_EXPORT void convert_real64_to_real80(const void *src, void *dst);
#endif

namespace edb {

namespace v1 {
EDB_EXPORT bool debuggeeIs32Bit();
}

namespace detail {

template <class Integer>
using IsInteger = typename std::enable_if<std::is_integral<Integer>::value>::type;

template <class T1, class T2>
using PromoteType = typename std::conditional<
	sizeof(T1) >= sizeof(T2),
	typename std::make_unsigned<T1>::type,
	typename std::make_unsigned<T2>::type>::type;

template <size_t N>
class value_type_large {
public:
	using T = uint64_t[N / 64];

public:
	// all defaulted to help ensure that this is a trivially-copyable type
	value_type_large()                         = default;
	value_type_large(const value_type_large &) = default;
	value_type_large &operator=(const value_type_large &) = default;
	value_type_large(value_type_large &&)                 = default;
	value_type_large &operator=(value_type_large &&) = default;
	~value_type_large()                              = default;

public:
	template <class U, class = typename std::enable_if<!std::is_arithmetic<U>::value>::type>
	explicit value_type_large(const U &data, size_t offset = 0) {

		static_assert(sizeof(data) >= sizeof(T), "value_type can only be constructed from large enough variable");
		static_assert(std::is_trivially_copyable<U>::value, "value_type can only be constructed from trivially copiable data");

		Q_ASSERT(sizeof(data) - offset >= sizeof(T)); // check bounds, this can't be done at compile time

		auto dataStart = reinterpret_cast<const char *>(&data);
		std::memcpy(&value_, dataStart + offset, sizeof(value_));
	}

public:
	template <class U>
	void load(const U &n) {
		static_assert(sizeof(T) >= sizeof(n), "Value to load is too large.");
		std::memcpy(&value_, &n, sizeof(n));
	}

public:
	bool operator==(const value_type_large &rhs) const { return std::memcmp(value_, rhs.value_, sizeof(T)) == 0; }
	bool operator!=(const value_type_large &rhs) const { return std::memcmp(value_, rhs.value_, sizeof(T)) != 0; }

public:
	QString toHexString() const {
		char buf[sizeof(T) * 2 + 1];
		char *p = buf;

		for (auto it = std::rbegin(value_); it != std::rend(value_); ++it) {
			p += sprintf(p, "%016" PRIx64, *it);
		}

		return QString::fromLatin1(buf);
	}

public:
	template <class U>
	static value_type_large fromZeroExtended(const U &data) {

		static_assert(sizeof(data) <= sizeof(T), "It doesn't make sense to expand a larger type into a smaller type");

		value_type_large created;

		auto dataStart = reinterpret_cast<const char *>(&data);
		auto target    = reinterpret_cast<char *>(&created.value_);

		std::memcpy(target, dataStart, sizeof(data));
		std::memset(target + sizeof(data), 0, sizeof(T) - sizeof(data));

		return created;
	}

private:
	T value_ = {};
};

template <class T>
class value_type {
	template <class U>
	friend class value_type;

public:
	using InnerValueType = T;

public:
	// all defaulted to help ensure that this is a trivially-copyable type
	value_type()                   = default;
	value_type(const value_type &) = default;
	value_type &operator=(const value_type &) = default;
	value_type(value_type &&)                 = default;
	value_type &operator=(value_type &&) = default;
	~value_type()                        = default;

public:
	template <class Integer, class = IsInteger<Integer>>
	value_type(Integer n)
		: value_(n) {
		// NOTE(eteran): this is allowed to truncate like assigning a uint64_t to a uint32_t
	}

	template <class Integer, class = IsInteger<Integer>>
	value_type &operator=(const Integer &rhs) {
		value_ = rhs;
		// NOTE(eteran): this is allowed to truncate like assigning a uint64_t to a uint32_t
		return *this;
	}

public:
	template <class U>
	explicit value_type(const value_type<U> &other)
		: value_(other.value_) {
		// NOTE(eteran): this is allowed to truncate like assigning a uint64_t to a uint32_t
	}

	template <class U>
	value_type &operator=(const value_type<U> &rhs) {
		value_ = rhs.value_;
		// NOTE(eteran): this is allowed to truncate like assigning a uint64_t to a uint32_t
		return *this;
	}

public:
	template <class U, class = typename std::enable_if<!std::is_arithmetic<U>::value>::type>
	explicit value_type(const U &data, size_t offset = 0) {

		static_assert(sizeof(data) >= sizeof(T), "value_type can only be constructed from large enough variable");
		static_assert(std::is_trivially_copyable<U>::value, "value_type can only be constructed from trivially copiable data");

		Q_ASSERT(sizeof(data) - offset >= sizeof(T)); // check bounds, this can't be done at compile time

		auto dataStart = reinterpret_cast<const char *>(&data);
		std::memcpy(&value_, dataStart + offset, sizeof(value_));
	}

public:
	template <class U>
	void load(const U &n) {
		static_assert(sizeof(T) >= sizeof(n), "Value to load is too large.");
		std::memcpy(&value_, &n, sizeof(n));
	}

public:
	static value_type fromString(const QString &str, bool *ok = nullptr, int base = 10, bool isSigned = false) {

		const qulonglong v = isSigned ? static_cast<qulonglong>(str.toLongLong(ok, base)) : str.toULongLong(ok, base);

		if (ok && !*ok) {
			return 0;
		}

		// Check that the result fits into the underlying type
		value_type result(v);
		if (result == v) {
			return result;
		}

		if (ok) {
			*ok = false;
		}

		return 0;
	}

	static value_type fromHexString(const QString &str, bool *ok = nullptr) {
		return fromString(str, ok, 16);
	}

	static value_type fromSignedString(const QString &str, bool *ok = nullptr) {
		return fromString(str, ok, 10, true);
	}

	static value_type fromCString(const QString &str, bool *ok = nullptr) {
		return fromString(str, ok, 0);
	}

	template <class U>
	static value_type fromZeroExtended(const U &data) {
		value_type created;

		static_assert(sizeof(data) <= sizeof(T), "It doesn't make sense to expand a larger type into a smaller type");

		auto dataStart = reinterpret_cast<const char *>(&data);
		auto target    = reinterpret_cast<char *>(&created.value_);

		std::memcpy(target, dataStart, sizeof(data));
		std::memset(target + sizeof(data), 0, sizeof(T) - sizeof(data));

		return created;
	}

public:
	void swap(value_type &other) {
		using std::swap;
		swap(value_, other.value_);
	}

public:
	bool negative() const {
		return typename std::make_signed<T>::type(value_) < 0;
	}

public:
	explicit operator bool() const { return value_ != 0; }
	bool operator!() const { return !value_; }
	operator T() const { return value_; }
	T toUint() const { return value_; }
	T &asUint() { return value_; }

public:
	value_type operator++(int) {
		T v(value_);
		++value_;
		return v;
	}

	value_type &operator++() {
		++value_;
		return *this;
	}

	value_type operator--(int) {
		T v(value_);
		--value_;
		return v;
	}

	value_type &operator--() {
		--value_;
		return *this;
	}

public:
	template <class U>
	value_type &operator+=(const value_type<U> &rhs) {
		value_ += rhs.value_;
		return *this;
	}

	template <class U>
	value_type &operator-=(const value_type<U> &rhs) {
		value_ -= rhs.value_;
		return *this;
	}

	template <class U>
	value_type &operator*=(const value_type<U> &rhs) {
		value_ *= rhs.value_;
		return *this;
	}

	template <class U>
	value_type &operator/=(const value_type<U> &rhs) {
		value_ /= rhs.value_;
		return *this;
	}

	template <class U>
	value_type &operator%=(const value_type<U> &rhs) {
		value_ %= rhs.value_;
		return *this;
	}

public:
	template <class U>
	value_type &operator&=(const value_type<U> &rhs) {
		value_ &= rhs.value_;
		return *this;
	}

	template <class U>
	value_type &operator|=(const value_type<U> &rhs) {
		value_ |= rhs.value_;
		return *this;
	}

	template <class U>
	value_type &operator^=(const value_type<U> &rhs) {
		value_ ^= rhs.value_;
		return *this;
	}

	template <class U>
	value_type &operator>>=(const value_type<U> &rhs) {
		value_ >>= rhs.value_;
		return *this;
	}

	template <class U>
	value_type &operator<<=(const value_type<U> &rhs) {
		value_ <<= rhs.value_;
		return *this;
	}

public:
	template <class Integer, class = IsInteger<Integer>>
	value_type &operator+=(Integer n) {
		value_ += n;
		return *this;
	}

	template <class Integer, class = IsInteger<Integer>>
	value_type &operator-=(Integer n) {
		value_ -= n;
		return *this;
	}

	template <class Integer, class = IsInteger<Integer>>
	value_type &operator*=(Integer n) {
		value_ *= n;
		return *this;
	}

	template <class Integer, class = IsInteger<Integer>>
	value_type &operator/=(Integer n) {
		value_ /= n;
		return *this;
	}

	template <class Integer, class = IsInteger<Integer>>
	value_type &operator%=(Integer n) {
		value_ %= n;
		return *this;
	}

public:
	template <class Integer, class = IsInteger<Integer>>
	value_type &operator&=(Integer n) {
		value_ &= n;
		return *this;
	}

	template <class Integer, class = IsInteger<Integer>>
	value_type &operator|=(Integer n) {
		value_ |= n;
		return *this;
	}

	template <class Integer, class = IsInteger<Integer>>
	value_type &operator^=(Integer n) {
		value_ ^= n;
		return *this;
	}

	template <class Integer, class = IsInteger<Integer>>
	value_type &operator>>=(Integer n) {
		value_ >>= n;
		return *this;
	}

	template <class Integer, class = IsInteger<Integer>>
	value_type &operator<<=(Integer n) {
		value_ <<= n;
		return *this;
	}

public:
	QString toPointerString(bool createdFromNativePointer = true) const {
		if (edb::v1::debuggeeIs32Bit()) {
			return "0x" + value_type<uint32_t>(value_).toHexString();
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

		if (valueLength == sizeof(value_)) {
			return result;
		}

		// if the sign bit is set
		if (value_ & (1ull << (valueLength * 8 - 1))) {
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

public:
	T value_ = {};
};

// iostream operators
template <class T>
std::istream &operator>>(std::istream &os, value_type<T> &val) {
	os >> val.asUint();
	return os;
}

template <class T>
std::ostream &operator<<(std::ostream &os, value_type<T> &val) {
	os << val.toUint();
	return os;
}

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
	return lhs.value_ != rhs.value_;
}

template <class T1, class T2>
auto operator+(const value_type<T1> &lhs, const value_type<T2> &rhs) -> value_type<PromoteType<T1, T2>> {

	using U = value_type<PromoteType<T1, T2>>;

	value_type<U> r(lhs);
	r += rhs;
	return r;
}

template <class T1, class T2>
auto operator-(const value_type<T1> &lhs, const value_type<T2> &rhs) -> value_type<PromoteType<T1, T2>> {

	using U = value_type<PromoteType<T1, T2>>;

	value_type<U> r(lhs);
	r -= rhs;
	return r;
}

template <class T1, class T2>
auto operator*(const value_type<T1> &lhs, const value_type<T2> &rhs) -> value_type<PromoteType<T1, T2>> {

	using U = value_type<PromoteType<T1, T2>>;

	value_type<U> r(lhs);
	r *= rhs;
	return r;
}

template <class T1, class T2>
auto operator/(const value_type<T1> &lhs, const value_type<T2> &rhs) -> value_type<PromoteType<T1, T2>> {

	using U = value_type<PromoteType<T1, T2>>;

	value_type<U> r(lhs);
	r /= rhs;
	return r;
}

template <class T1, class T2>
auto operator%(const value_type<T1> &lhs, const value_type<T2> &rhs) -> value_type<PromoteType<T1, T2>> {

	using U = value_type<PromoteType<T1, T2>>;

	value_type<U> r(lhs);
	r %= rhs;
	return r;
}

template <class T1, class T2>
auto operator&(const value_type<T1> &lhs, const value_type<T2> &rhs) -> value_type<PromoteType<T1, T2>> {

	using U = value_type<PromoteType<T1, T2>>;

	value_type<U> r(lhs);
	r &= rhs;
	return r;
}

template <class T1, class T2>
auto operator|(const value_type<T1> &lhs, const value_type<T2> &rhs) -> value_type<PromoteType<T1, T2>> {

	using U = value_type<PromoteType<T1, T2>>;

	value_type<U> r(lhs);
	r |= rhs;
	return r;
}

template <class T1, class T2>
auto operator^(const value_type<T1> &lhs, const value_type<T2> &rhs) -> value_type<PromoteType<T1, T2>> {

	using U = value_type<PromoteType<T1, T2>>;

	value_type<U> r(lhs);
	r ^= rhs;
	return r;
}

struct value_type80 {
public:
	using T = uint8_t[10];

public:
	// all defaulted to help ensure that this is a trivially-copyable type
	value_type80()                     = default;
	value_type80(const value_type80 &) = default;
	value_type80 &operator=(const value_type80 &) = default;
	value_type80(value_type80 &&)                 = default;
	value_type80 &operator=(value_type80 &&) = default;
	~value_type80()                          = default;

public:
	template <class U>
	explicit value_type80(const U &data, size_t offset = 0) {
#ifdef _MSC_VER
		if (std::is_same<U, long double>::value && sizeof(U) < sizeof(T)) {
			T temp;
			convert_real64_to_real80(&data, &temp);

			Q_ASSERT(sizeof(temp) - offset >= sizeof(T)); // check bounds, this can't be done at compile time

			auto dataStart = reinterpret_cast<const char *>(&temp);
			std::memcpy(&value_, dataStart + offset, sizeof(value_));
			return;
		}
#else
		static_assert(sizeof(data) >= sizeof(T), "ValueBase can only be constructed from large enough variable");
#endif
		static_assert(std::is_trivially_copyable<U>::value, "ValueBase can only be constructed from trivially copiable data");

		Q_ASSERT(sizeof(data) - offset >= sizeof(T)); // check bounds, this can't be done at compile time

		auto dataStart = reinterpret_cast<const char *>(&data);
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
				 value_[0]);

		return QString::fromLatin1(buf);
	}

public:
	bool operator==(const value_type80 &rhs) const { return std::memcmp(value_, rhs.value_, 10) == 0; }
	bool operator!=(const value_type80 &rhs) const { return std::memcmp(value_, rhs.value_, 10) != 0; }

private:
	T value_ = {};
};

static_assert(sizeof(value_type80) * 8 == 80, "value_type80 size is broken!");

}

// GPR on x86
using value8  = detail::value_type<uint8_t>;
using value16 = detail::value_type<uint16_t>;
using value32 = detail::value_type<uint32_t>;

// MMX/GPR(x86_64)
using value64 = detail::value_type<uint64_t>;

// We support registers and addresses of 64-bits
using address_t = value64;
using reg_t     = value64;

// FPU
using value80 = detail::value_type80;

// SSE
using value128 = detail::value_type_large<128>;

// AVX
using value256 = detail::value_type_large<256>;

// AVX512
using value512 = detail::value_type_large<512>;

static_assert(std::is_standard_layout<value8>::value &&
				  std::is_standard_layout<value16>::value &&
				  std::is_standard_layout<value32>::value &&
				  std::is_standard_layout<value64>::value &&
				  std::is_standard_layout<value80>::value &&
				  std::is_standard_layout<value128>::value &&
				  std::is_standard_layout<value256>::value &&
				  std::is_standard_layout<value512>::value,
			  "Fixed-sized values are intended to have standard layout");

static_assert(std::is_trivially_copyable<value8>::value &&
				  std::is_trivially_copyable<value16>::value &&
				  std::is_trivially_copyable<value32>::value &&
				  std::is_trivially_copyable<value64>::value &&
				  std::is_trivially_copyable<value80>::value &&
				  std::is_trivially_copyable<value128>::value &&
				  std::is_trivially_copyable<value256>::value &&
				  std::is_trivially_copyable<value512>::value,
			  "Fixed-sized values are intended to be trivially copyable");

}

#endif

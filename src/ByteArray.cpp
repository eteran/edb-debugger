
#include "ByteArray.h"
#include <QtAlgorithms>
#include <QString>
#include <QtDebug>
#include <bitset>

bool fullAdder(bool b1, bool b2, bool &carry) {
	bool sum = (b1 ^ b2) ^ carry;
	carry = (b1 && b2) || (b1 && carry) || (b2 && carry);
	return sum;
}

bool fullSubtractor(bool b1, bool b2, bool &borrow) {
	bool diff;
	if (borrow) {
		diff = !(b1 ^ b2);
		borrow = !b1 || (b1 && b2);
	} else {
		diff = b1 ^ b2;
		borrow = !b1 && b2;
	}
	return diff;
}

template <unsigned int N>
bool bitsetLtEq(const std::bitset<N> &x, const std::bitset<N> &y) {
	for (int i = N - 1; i >= 0; i--) {
		if (x[i] && !y[i])
			return false;
		if (!x[i] && y[i])
			return true;
	}
	return true;
}

template <unsigned int N>
bool bitsetLt(const std::bitset<N> &x, const std::bitset<N> &y) {
	for (int i = N - 1; i >= 0; i--) {
		if (x[i] && !y[i])
			return false;
		if (!x[i] && y[i])
			return true;
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: fromU8
//------------------------------------------------------------------------------
ByteArray ByteArray::fromU8(quint8 value) {
	ByteArray ba(sizeof(quint8));
	ba.value_.u8 = value;
	return ba;
}

//------------------------------------------------------------------------------
// Name: fromU16
//------------------------------------------------------------------------------
ByteArray ByteArray::fromU16(quint16 value) {
	ByteArray ba(sizeof(quint16));
	ba.value_.u16 = value;
	return ba;
}

//------------------------------------------------------------------------------
// Name: fromU32
//------------------------------------------------------------------------------
ByteArray ByteArray::fromU32(quint32 value) {
	ByteArray ba(sizeof(quint32));
	ba.value_.u32 = value;
	return ba;
}

//------------------------------------------------------------------------------
// Name: fromU64
//------------------------------------------------------------------------------
ByteArray ByteArray::fromU64(quint64 value) {
	ByteArray ba(sizeof(quint64));
	ba.value_.u64 = value;
	return ba;
}

//------------------------------------------------------------------------------
// Name: ByteArray
//------------------------------------------------------------------------------
ByteArray::ByteArray(size_t size) : size_(size) {
	switch (size_) {
	case sizeof(quint8):
		value_.u8 = 0;
		break;
	case sizeof(quint16):
		value_.u16 = 0;
		break;
	case sizeof(quint32):
		value_.u32 = 0;
		break;
	case sizeof(quint64):
		value_.u64 = 0;
		break;
	default:
		value_.ptr = new quint8[size_]();
		break;
	}
}

//------------------------------------------------------------------------------
// Name: ~ByteArray
//------------------------------------------------------------------------------
ByteArray::~ByteArray() {
	switch (size_) {
	case sizeof(quint8):
	case sizeof(quint16):
	case sizeof(quint32):
	case sizeof(quint64):
		break;
	default:
		delete[] value_.ptr;
	}
}

//------------------------------------------------------------------------------
// Name: ByteArray
//------------------------------------------------------------------------------
ByteArray::ByteArray(const ByteArray &other) : size_(other.size_) {

	switch (size_) {
	case sizeof(quint8):
		value_.u8 = other.value_.u8;
		break;
	case sizeof(quint16):
		value_.u16 = other.value_.u16;
		break;
	case sizeof(quint32):
		value_.u32 = other.value_.u32;
		break;
	case sizeof(quint64):
		value_.u64 = other.value_.u64;
		break;
	default:
		value_.ptr = new quint8[size_];
		qCopy(other.value_.ptr, other.value_.ptr + other.size_, value_.ptr);
		break;
	}
}

//------------------------------------------------------------------------------
// Name: operator=
//------------------------------------------------------------------------------
ByteArray &ByteArray::operator=(const ByteArray &rhs) {
	if (this != &rhs) {
		ByteArray(rhs).swap(*this);
	}
	return *this;
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
size_t ByteArray::size() const {
	return size_;
}

//------------------------------------------------------------------------------
// Name: swap
//------------------------------------------------------------------------------
void ByteArray::swap(ByteArray &other) {
	qSwap(size_, other.size_);
	qSwap(value_, other.value_);
}

//------------------------------------------------------------------------------
// Name: operator[]
//------------------------------------------------------------------------------
quint8 ByteArray::operator[](size_t index) const {
	Q_ASSERT(index < size_);

	switch (size_) {
	case sizeof(quint8):
	case sizeof(quint16):
	case sizeof(quint32):
	case sizeof(quint64):
		return value_.bytes[index];
	default:
		return value_.ptr[index];
	}
}

//------------------------------------------------------------------------------
// Name: operator[]
//------------------------------------------------------------------------------
quint8 &ByteArray::operator[](size_t index) {
	Q_ASSERT(index < size_);

	switch (size_) {
	case sizeof(quint8):
	case sizeof(quint16):
	case sizeof(quint32):
	case sizeof(quint64):
		return value_.bytes[index];
	default:
		return value_.ptr[index];
	}
}

//------------------------------------------------------------------------------
// Name: operator==
//------------------------------------------------------------------------------
bool ByteArray::operator==(const ByteArray &rhs) const {
	if (size_ != rhs.size_) {
		return false;
	}

	switch (size_) {
	case sizeof(quint8):
		return value_.u8 == rhs.value_.u8;
	case sizeof(quint16):
		return value_.u16 == rhs.value_.u16;
	case sizeof(quint32):
		return value_.u32 == rhs.value_.u32;
	case sizeof(quint64):
		return value_.u64 == rhs.value_.u64;
	default:
		return qEqual(rhs.value_.ptr, rhs.value_.ptr + rhs.size_, value_.ptr);
	}
}

//------------------------------------------------------------------------------
// Name: operator!=
//------------------------------------------------------------------------------
bool ByteArray::operator!=(const ByteArray &rhs) const {
	return !(*this == rhs);
}

//------------------------------------------------------------------------------
// Name: format
//------------------------------------------------------------------------------
QString ByteArray::format(Format f) const {

	char buf[1024] = {};

	switch (size_) {
	case sizeof(quint8):
		switch (f) {
		case SignedDecimal:
			qsnprintf(buf, sizeof(buf), "%d", value_.u8);
			break;
		case UnsignedDecimal:
			qsnprintf(buf, sizeof(buf), "%u", value_.u8);
			break;
		case HexDecimal:
			qsnprintf(buf, sizeof(buf), "%02x", value_.u8);
			break;
		case Binary:
			for(int j = 0, i = (size_ * 8) - 1; i >= 0; --i) {
				buf[j++] = bit(i) ? '1' : '0';
			}
			break;
		}
		break;
	case sizeof(quint16):
		switch (f) {
		case SignedDecimal:
			qsnprintf(buf, sizeof(buf), "%d", value_.u16);
			break;
		case UnsignedDecimal:
			qsnprintf(buf, sizeof(buf), "%u", value_.u16);
			break;
		case HexDecimal:
			qsnprintf(buf, sizeof(buf), "%04x", value_.u16);
			break;
		case Binary:
			for(int j = 0, i = (size_ * 8) - 1; i >= 0; --i) {
				buf[j++] = bit(i) ? '1' : '0';
			}
			break;
		}
		break;
	case sizeof(quint32):
		switch (f) {
		case SignedDecimal:
			qsnprintf(buf, sizeof(buf), "%d", value_.u32);
			break;
		case UnsignedDecimal:
			qsnprintf(buf, sizeof(buf), "%u", value_.u32);
			break;
		case HexDecimal:
			qsnprintf(buf, sizeof(buf), "%08x", value_.u32);
			break;
		case Binary:
			for(int j = 0, i = (size_ * 8) - 1; i >= 0; --i) {
				buf[j++] = bit(i) ? '1' : '0';
			}
			break;
		}
		break;
	case sizeof(quint64):

		switch (f) {
		case SignedDecimal:
			qsnprintf(buf, sizeof(buf), "%lld", value_.u64);
			break;
		case UnsignedDecimal:
			qsnprintf(buf, sizeof(buf), "%llu", value_.u64);
			break;
		case HexDecimal:
			qsnprintf(buf, sizeof(buf), "%016llx", value_.u64);
			break;
		case Binary:
			for(int j = 0, i = (size_ * 8) - 1; i >= 0; --i) {
				buf[j++] = bit(i) ? '1' : '0';
			}
			break;
		}
		break;
	default:
		switch (f) {
		case SignedDecimal:
			// TODO(eteran): implement this
			break;
		case UnsignedDecimal:
			// TODO(eteran): implement this
			break;
		case HexDecimal:
			for (size_t i = 0; i < size_; ++i) {
				qsnprintf(&buf[i * 2], 3, "%02x", value_.ptr[size_ - i - 1]);
			}
			break;
		case Binary:
			for(int j = 0, i = (size_ * 8) - 1; i >= 0; --i) {
				buf[j++] = bit(i) ? '1' : '0';
			}
			break;
		}
	}

	return buf;
}

//------------------------------------------------------------------------------
// Name: bit
//------------------------------------------------------------------------------
bool ByteArray::bit(size_t index) const {

	if(index > size_ * 8) {
		return false;
	}

	switch (size_) {
	case sizeof(quint8):
		return value_.u8 & (1 << index);
	case sizeof(quint16):
		return value_.u16 & (1 << index);
	case sizeof(quint32):
		return value_.u32 & (1 << index);
	case sizeof(quint64):
		return value_.u64 & (1 << index);
	default:
		return value_.ptr[index / 8] & (1 << (index % 8));
	}
}

//------------------------------------------------------------------------------
// Name: setBit
//------------------------------------------------------------------------------
void ByteArray::setBit(size_t index, bool value) {

	if(index > size_ * 8) {
		return ;
	}

	if(value) {
		switch (size_) {
		case sizeof(quint8):
			value_.u8 |= (1 << index);
			break;
		case sizeof(quint16):
			value_.u16 |= (1 << index);
			break;
		case sizeof(quint32):
			value_.u32 |= (1 << index);
			break;
		case sizeof(quint64):
			value_.u64 |= (1 << index);
			break;
		default:
			value_.ptr[index / 8] |= (1 << (index % 8));
			break;
		}
	} else {


		switch (size_) {
		case sizeof(quint8):
			value_.u8 &= ~(1 << index);
			break;
		case sizeof(quint16):
			value_.u16 &= ~(1 << index);
			break;
		case sizeof(quint32):
			value_.u32 &= ~(1 << index);
			break;
		case sizeof(quint64):
			value_.u64 &= ~(1 << index);
			break;
		default:
			value_.ptr[index / 8] &= ~(1 << (index % 8));
			break;
		}
	}
}

//------------------------------------------------------------------------------
// Name: clear
//------------------------------------------------------------------------------
void ByteArray::clear() {
	switch (size_) {
	case sizeof(quint8):
		value_.u8 = 0;
		break;
	case sizeof(quint16):
		value_.u16 = 0;
		break;
	case sizeof(quint32):
		value_.u32 = 0;
		break;
	case sizeof(quint64):
		value_.u64 = 0;
		break;
	default:
		memset(value_.ptr, 0, size_);
		break;
	}
}

//------------------------------------------------------------------------------
// Name: shiftLeft
//------------------------------------------------------------------------------
void ByteArray::shiftLeft(int n) {
	Q_ASSERT(n >= 0);

	switch (size_) {
	case sizeof(quint8):
		value_.u8 <<= n;
		break;
	case sizeof(quint16):
		value_.u16 <<= n;
		break;
	case sizeof(quint32):
		value_.u32 <<= n;
		break;
	case sizeof(quint64):
		value_.u64 <<= n;
		break;
	default:
		{
			// TODO(eteran): test if we are shifting by whole bytes, and
			// if so, do this faster!

			ByteArray temp(size_);
			for(size_t i = 0; i < static_cast<size_t>(n); ++i) {
				for(size_t j = 0; j < (size_ * 8) - 1; ++j) {
					temp.setBit(j + 1, bit(j));
				}

				temp.swap(*this);
				temp.clear();
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: shiftRight
//------------------------------------------------------------------------------
void ByteArray::shiftRight(int n) {
	Q_ASSERT(n >= 0);

	switch (size_) {
	case sizeof(quint8):
		value_.u8 >>= n;
		break;
	case sizeof(quint16):
		value_.u16 >>= n;
		break;
	case sizeof(quint32):
		value_.u32 >>= n;
		break;
	case sizeof(quint64):
		value_.u64 >>= n;
		break;
	default:
		{
			// TODO(eteran): test if we are shifting by whole bytes, and
			// if so, do this faster!

			ByteArray temp(size_);
			for(size_t i = 0; i < static_cast<size_t>(n); ++i) {
				for(size_t j = 1; j < (size_ * 8); ++j) {
					temp.setBit(j - 1, bit(j));
				}

				temp.swap(*this);
				temp.clear();
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: operator<<=
//------------------------------------------------------------------------------
ByteArray &ByteArray::operator<<=(int n) {
	if(n >= 0) {
		shiftLeft(n);
	} else {
		shiftRight(-n);
	}
	return *this;
}

//------------------------------------------------------------------------------
// Name: operator>>=
//------------------------------------------------------------------------------
ByteArray &ByteArray::operator>>=(int n) {
	if(n >= 0) {
		shiftRight(n);
	} else {
		shiftLeft(-n);
	}

	return *this;
}


#ifndef BYTE_ARRAY_20150715_H_
#define BYTE_ARRAY_20150715_H_

#include <QtGlobal>
class QString;

class ByteArray {
public:
	ByteArray(size_t n);
	~ByteArray();
	ByteArray(const ByteArray &other);
	ByteArray &operator=(const ByteArray &rhs);
	
public:
	static ByteArray fromU8(quint8 value);
	static ByteArray fromU16(quint16 value);
	static ByteArray fromU32(quint32 value);
	static ByteArray fromU64(quint64 value);
	
public:
	size_t size() const;
	void swap(ByteArray &other);
	
	quint8 operator[](size_t index) const;
	quint8& operator[](size_t index);
	
	bool bit(size_t index) const;
	void setBit(size_t index, bool value);
	void clear();
	
public:
	enum Format {
		HexDecimal,
		SignedDecimal,
		UnsignedDecimal,
		Binary
	};
	QString format(Format f) const;
	
public:
	bool operator==(const ByteArray &rhs) const;
	bool operator!=(const ByteArray &rhs) const;
	ByteArray &operator<<=(int n);
	ByteArray &operator>>=(int n);


private:
	void shiftLeft(int n);
	void shiftRight(int n);
	
private:
	size_t size_;
	
	// the idea here is that the size of this union will be the same as 
	// the size of a pointer.. but for small sizes, we just inline the data
	// instead of allocating on the heap. This is similar to the 
	// "small-string-optimization" found in many string implementations
	union {
		quint8 *ptr;
		quint8  bytes[sizeof(quint64)]; // to allow direct access to the bytes of the inlined data
		quint64 u64;
		quint32 u32;
		quint16 u16;
		quint8  u8;
	} value_;
};

#endif

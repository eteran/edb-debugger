
#ifndef ADDRESS_H_
#define ADDRESS_H_

#include "Value.h"

namespace edb {

class EDB_EXPORT address_t : public value64 {
public:
	QString toPointerString(bool createdFromNativePointer = true) const;
	QString toHexString() const;

	template <class SmallData>
	static address_t fromZeroExtended(const SmallData& data) {
		return value64::fromZeroExtended(data);
	}

	template<class T>
	address_t(const T& val) : value64(val) {
	}

	address_t() = default;
	void normalize();
};

using reg_t = address_t;

}

#endif


#include "Address.h"
#include "edb.h"

/**
 * @brief edb::address_t::toPointerString
 * @param createdFromNativePointer
 * @return
 */
QString edb::address_t::toPointerString(bool createdFromNativePointer) const {
	if (edb::v1::debuggeeIs32Bit()) {
		return "0x" + toHexString();
	} else {
		if (!createdFromNativePointer) { // then we don't know value of upper dword
			return "0x????????" + value32(value_[0]).toHexString();
		} else {
			return "0x" + toHexString();
		}
	}
}

/**
 * @brief edb::address_t::toHexString
 * @return
 */
QString edb::address_t::toHexString() const {
	if (edb::v1::debuggeeIs32Bit()) {
		if (value_[0] > 0xffffffffull) {
			// Make erroneous bits visible
			QString string = value64::toHexString();
			string.insert(8, "]");
			return "[" + string;
		}
		return value32(value_[0]).toHexString();
	} else {
		return value64::toHexString();
	}
}

void edb::address_t::normalize() {
	if (edb::v1::debuggeeIs32Bit()) {
		value_[0] &= 0xffffffffull;
	}
}

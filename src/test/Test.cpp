
#include "Value.h"
#include <cstdlib>

#define TEST(expr)                                                  \
	do {                                                            \
		if (!(expr)) {                                              \
			fprintf(stderr, "FAILED: [@%d] %s\n", __LINE__, #expr); \
			abort();                                                \
		}                                                           \
	} while (0)

namespace {

void testSignExtension() {

	edb::value64 v64;

	// should sign extend
	v64 = 0xff;
	TEST(v64.signExtended(1) == 0xffffffffffffffff);

	v64 = 0xffff;
	TEST(v64.signExtended(2) == 0xffffffffffffffff);

	v64 = 0xffffff;
	TEST(v64.signExtended(3) == 0xffffffffffffffff);

	v64 = 0xffffffff;
	TEST(v64.signExtended(4) == 0xffffffffffffffff);

	v64 = 0xffffffffff;
	TEST(v64.signExtended(5) == 0xffffffffffffffff);

	v64 = 0xffffffffffff;
	TEST(v64.signExtended(6) == 0xffffffffffffffff);

	v64 = 0xffffffffffffff;
	TEST(v64.signExtended(7) == 0xffffffffffffffff);

	v64 = 0xffffffffffffffff;
	TEST(v64.signExtended(8) == 0xffffffffffffffff);

	//shouldn't!
	v64 = 0x7f;
	TEST(v64.signExtended(1) == 0x000000000000007f);

	v64 = 0x7fff;
	TEST(v64.signExtended(2) == 0x0000000000007fff);

	v64 = 0x7fffff;
	TEST(v64.signExtended(3) == 0x00000000007fffff);

	v64 = 0x7fffffff;
	TEST(v64.signExtended(4) == 0x000000007fffffff);

	v64 = 0x7fffffffff;
	TEST(v64.signExtended(5) == 0x0000007fffffffff);

	v64 = 0x7fffffffffff;
	TEST(v64.signExtended(6) == 0x00007fffffffffff);

	v64 = 0x7fffffffffffff;
	TEST(v64.signExtended(7) == 0x007fffffffffffff);

	v64 = 0x7fffffffffffffff;
	TEST(v64.signExtended(8) == 0x7fffffffffffffff);
}

void testAssignment() {
	edb::value64 v64;
	TEST(v64 == 0);

	v64 = 10;
	TEST(v64 == 10);

	v64 += 50;
	TEST(v64 == 60);

	++v64;
	TEST(v64 == 61);

	TEST(v64++ == 61);
	TEST(v64 == 62);

	--v64;
	TEST(v64 == 61);
}

void testFromString() {
	edb::value64 v64;
	v64 = edb::value64::fromHexString("0123456789abcdef");
	TEST(v64 == 0x0123456789abcdef);

	v64 = edb::value64::fromSignedString("-10");
	TEST(v64 == 0xfffffffffffffff6);

	v64 = edb::value64::fromCString("07777777777");
	TEST(v64 == 07777777777);
}

void testConstruction() {
	edb::value8 v8;
	edb::value16 v16;
	edb::value32 v32;
	edb::value64 v64;
	edb::value128 v128;
	edb::value256 v256;
	edb::value512 v512;

	(void)v8;
	(void)v16;
	(void)v32;
	(void)v64;
	(void)v128;
	(void)v256;
	(void)v512;
}

void testToString() {
	edb::value64 v64;
	v64 = edb::value64::fromHexString("0123456789abcdef");
	TEST(v64 == 0x0123456789abcdef);

	QString str = v64.toHexString();
	TEST(str == "0123456789abcdef");

	QString str2 = v64.toString();
	TEST(str2 == "81985529216486895");
}

}

int main() {
	testConstruction();
	testAssignment();
	testFromString();
	testSignExtension();
	testToString();
}

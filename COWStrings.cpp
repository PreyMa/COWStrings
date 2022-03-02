// COWStrings.cpp : Defines the entry point for the application.
//

#include<iostream>
#include<vector>

#include "string.h"
#include "test.h"


void printStringStats(const String& s) {
	std::cout << "Buffer size: " << s.bufferSize() << " Buffer capacity: " << s.bufferCapacity() << " Length: " << s.length() << std::endl;
	std::cout << "Content: '" << s.cString() << "'\n";
}


void stringTests() {
	Test test;

	test.test("Default construction", [&] {
		String s;
		test.expect(s.isEmpty())->toBeTrue();
		test.expect(s.bufferCapacity())->toBe(32);
		test.expect(s.bufferSize())->toBe(1);
		test.expect(s.length())->toBeZero();
		test.expect(StringIntrospection(s).isSmall())->toBeTrue();
	});


	test.test("Small string construction from pointer", [&] {
		char cstr[] = "abcdefgh";
		const char* ptr = cstr;
		auto asciiLength = sizeof(cstr);
		String s( ptr );
		test.expect(s.isEmpty())->toBeFalse();
		test.expect(s.bufferCapacity())->toBe(32);
		test.expect(s.bufferSize())->toBe(asciiLength);
		test.expect(s.length())->toBe(asciiLength-1);
		test.expect(StringIntrospection(s).isSmall())->toBeTrue();
	});


	test.test("Long string construction from pointer", [&] {
		char cstr[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
		const char* ptr = cstr;
		auto asciiLength = sizeof(cstr);
		String s(ptr);
		test.expect(s.isEmpty())->toBeFalse();
		test.expect(s.bufferCapacity())->toBe(64);
		test.expect(s.bufferSize())->toBe(asciiLength);
		test.expect(s.length())->toBe(asciiLength - 1);
		test.expect(StringIntrospection(s).isSmall())->toBeFalse();
		test.expect(StringIntrospection(s).isLiteral())->toBeFalse();
		test.expect(StringIntrospection(s).isDynamic())->toBeTrue();
		test.expect(StringIntrospection(s).isShared())->toBeFalse();
		test.expect(StringIntrospection(s).mode())->toBe(StringIntrospection::Mode::Owned);
	});


	test.test("Small string construction from array (compile time length)", [&] {
		char cstr[] = "abcdefgh";
		const char* ptr = cstr;
		auto asciiLength = sizeof(cstr);
		String s= cstr;
		test.expect(s.isEmpty())->toBeFalse();
		test.expect(s.bufferCapacity())->toBe(32);
		test.expect(s.bufferSize())->toBe(asciiLength);
		test.expect(s.length())->toBe(asciiLength - 1);
		test.expect(StringIntrospection(s).isSmall())->toBeTrue();
	});


	test.test("Long string construction from array (compile time length)", [&] {
		char cstr[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
		auto asciiLength = sizeof(cstr);
		String s = cstr;
		test.expect(s.isEmpty())->toBeFalse();
		test.expect(s.bufferCapacity())->toBe(0);
		test.expect(s.bufferSize())->toBe(asciiLength);
		test.expect(s.length())->toBe(asciiLength - 1);
		test.expect(StringIntrospection(s).isSmall())->toBeFalse();
		test.expect(StringIntrospection(s).isLiteral())->toBeTrue();
		test.expect(StringIntrospection(s).isDynamic())->toBeFalse();
		test.expect(StringIntrospection(s).isShared())->toBeFalse();
		test.expect(StringIntrospection(s).mode())->toBe(StringIntrospection::Mode::Literal);
	});


	test.test("Small string copy construction", [&] {
		char cstr[] = "abcdefgh";
		const char* ptr = cstr;
		auto asciiLength = sizeof(cstr);
		String s = cstr;
		String s2 = s;

		test.expect(s.isEmpty())->toBeFalse();
		test.expect(s.bufferCapacity())->toBe(32);
		test.expect(s.bufferSize())->toBe(asciiLength);
		test.expect(s.length())->toBe(asciiLength - 1);
		test.expect(StringIntrospection(s).isSmall())->toBeTrue();

		test.expect(s2.isEmpty())->toBeFalse();
		test.expect(s2.bufferCapacity())->toBe(32);
		test.expect(s2.bufferSize())->toBe(asciiLength);
		test.expect(s2.length())->toBe(asciiLength - 1);
		test.expect(StringIntrospection(s2).isSmall())->toBeTrue();
	});


	test.test("Literal string copy construction", [&] {
		char cstr[] = "abcdefghihklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
		auto asciiLength = sizeof(cstr);
		String s = cstr;
		String s2= s;

		test.expect(s.isEmpty())->toBeFalse();
		test.expect(s.bufferCapacity())->toBeZero();
		test.expect(s.bufferSize())->toBe(asciiLength);
		test.expect(s.length())->toBe(asciiLength - 1);
		test.expect(StringIntrospection(s).mode())->toBe(StringIntrospection::Mode::Literal);

		test.expect(s2.isEmpty())->toBeFalse();
		test.expect(s2.bufferCapacity())->toBeZero();
		test.expect(s2.bufferSize())->toBe(asciiLength);
		test.expect(s2.length())->toBe(asciiLength - 1);
		test.expect(StringIntrospection(s2).mode())->toBe(StringIntrospection::Mode::Literal);
	});


	test.test("Long string shared-copy construction", [&] {
		char cstr[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
		const char* ptr = cstr;
		auto asciiLength = sizeof(cstr);
		String s(ptr);

		test.expect(StringIntrospection(s).mode())->toBe(StringIntrospection::Mode::Owned);

		String s2 = s;

		test.expect(s.isEmpty())->toBeFalse();
		test.expect(s.bufferCapacity())->toBe(64);
		test.expect(s.bufferSize())->toBe(asciiLength);
		test.expect(s.length())->toBe(asciiLength - 1);
		test.expect(StringIntrospection(s).isShared())->toBeTrue();

		test.expect(s2.isEmpty())->toBeFalse();
		test.expect(s2.bufferCapacity())->toBe(64);
		test.expect(s2.bufferSize())->toBe(asciiLength);
		test.expect(s2.length())->toBe(asciiLength - 1);
		test.expect(StringIntrospection(s2).isShared())->toBeTrue();
	});


	test.test("Long string from owned to shared and back to owned", [&] {
		String s = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
		s.reserve();

		test.expect(StringIntrospection(s).mode())->toBe(StringIntrospection::Mode::Owned);

		{
			String s2 = s;
			test.expect(StringIntrospection(s).mode())->toBe(StringIntrospection::Mode::Shared);
			test.expect(StringIntrospection(s2).mode())->toBe(StringIntrospection::Mode::Shared);

			test.expect(s.bufferSize())->toBe(s2.bufferSize());
			test.expect(s.bufferCapacity())->toBe(s2.bufferCapacity());
		}
		
		test.expect(StringIntrospection(s).mode())->toBe(StringIntrospection::Mode::Owned);
	});


	test.test("Count utf-8 codepoints short", [&] {
		char cstr[] = "\xf0\x9f\xa5\x9d!\xc3\xa4(obzzt)";
		auto asciiLength = sizeof(cstr);
		String s(cstr);
		test.expect(s.isEmpty())->toBeFalse();
		test.expect(s.bufferCapacity())->toBe(32);
		test.expect(s.bufferSize())->toBe(asciiLength);
		test.expect(s.length())->toBe(10);
		test.expect(StringIntrospection(s).mode())->toBe(StringIntrospection::Mode::Small);
	});


	test.test("Count the utf-8 codepoints long", [&] {
		char cstr[] = "\xf0\x9f\x8e\x80\x68\xf0\x9f\x8e\x81\x65\xf0\x9f\x8e\x97\x6c\xf0\x9f\x8e\x9e\x6c\xf0\x9f\x8e\x9f\x6f\xf0\x9f\x8e\xab\x77\xf0\x9f\x8e\xa0\x6f\xf0\x9f\x8e\xa1\x72\xf0\x9f\x8e\xa2\x6c\xf0\x9f\x8e\xaa\x64\xf0\x9f\x8e\xad\x21\xf0\x9f\x96\xbc\xc3\xa4\xf0\x9f\x8e\xa8\xc3\xbc\xf0\x9f\xa7\xb5\xc3\xb6\xf0\x9f\xa7\xb6\xf0\x9f\x9b\x92";
		auto asciiLength = sizeof(cstr);
		String s(cstr);
		test.expect(s.isEmpty())->toBeFalse();
		test.expect(s.bufferCapacity())->toBe(82);
		test.expect(s.bufferSize())->toBe(asciiLength);
		test.expect(s.length())->toBe(30);
		test.expect(StringIntrospection(s).mode())->toBe(StringIntrospection::Mode::Owned);
	});

	std::cout << test;
}


int main() {
	stringTests();
	return 0;
}

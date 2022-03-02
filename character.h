#pragma once

#include <ostream>
#include <cassert>

#include "util.h"

class Character {
public:
	static u64 byteLengthFromLeadingByte(u8 byte) {
		// 0xxxxxxx -> 00 - 7F (0   - 127)
		// 110xxxxx -> C0 - DF (192 - 223)
		// 1110xxxx -> E0 - EF (224 - 239)
		// 11110xxx -> F0 - F7 (240 - 247)

		if (byte < 128) {
			return 1;
		}

		if (byte < 224) {
			return 2;
		}

		if (byte < 240) {
			return 3;
		}

		return 4;
	}

	static u32 utf8ToUnicodeCodePoint(u32 utf8) {
		if (utf8 < 128) {
			// 0xxxxxxx
			return utf8;
		}

		if (utf8 < 224) {
			// 110xxxxx 10xxxxxx
			return ((utf8 & 0x1F00) >> 2) | (utf8 & 0x3F);
		}

		if (utf8 < 240) {
			// 1110xxxx 10xxxxxx 10xxxxxx
			return ((utf8 & 0x0F0000) >> 4) | ((utf8 & 0x3F00) >> 2) | (utf8 & 0x3F);
		}

		// 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
		return ((utf8 & 0x07000000) >> 6) | ((utf8 & 0x3F0000) >> 4) | ((utf8 & 0x3F00) >> 2) | (utf8 & 0x3F);
	}

	static u64 countCodePointsInBuffer(const u8* ptr, u64 length) {
		u64 count = 0;
		auto endPtr = ptr + length;
		while (ptr < endPtr) {
			ptr += Character::byteLengthFromLeadingByte(*ptr);
			count++;
		}
		return count;
	}

	static u64 countCodePointsInCString(const char* startPtr, u64* length) {
		u64 count = 0;
		u64 bytesToSkip = 0;
		auto ptr = startPtr;

		while (*ptr) {
			if (!bytesToSkip) {
				bytesToSkip = Character::byteLengthFromLeadingByte(*ptr);
				count++;
			}
			ptr++;
			bytesToSkip--;
		}

		*length = ptr - startPtr;
		return count;
	}

	static const u8* getCodePointInBufferAt(const u8* ptr, u64 length, u64 idx) {
		auto endPtr = ptr + length;
		while (ptr < endPtr) {
			if (!idx--) {
				return ptr;
			}
			ptr += Character::byteLengthFromLeadingByte(*ptr);
		}
		return nullptr;
	}

	const u8* bytes() const {
		return (u8*)(&value);
	}

	u64 byteCount() const {
		return byteLengthFromLeadingByte(*bytes());
	}

	u32 toUnicodeCodepoint() const {
		return utf8ToUnicodeCodePoint(value);
	}

	Character(const u8* ptr) {
		assert(ptr);
		auto len = byteLengthFromLeadingByte(*ptr);
		switch (len) {
		case 1:
			value = *ptr;
			break;
		case 2:
			value = *((u16*)ptr);
			break;
		case 3:
			value = ptr[0] | ((u64)ptr[1] << 8) | ((u64)ptr[2] << 16);
			break;
		default:
			value = *((u64*)ptr);
		}
	}

private:
	u32 value{ 0 };
};

std::ostream& operator << (std::ostream& o, Character c);

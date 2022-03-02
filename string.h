#pragma once

#include <ostream>
#include <algorithm>
#include <cassert>

#include "mem.h"
#include "character.h"
#include <optional>

class String {
private:
	struct StringDataInterface {
		void setCodePoints(u64 v) const {
			u64 mask = 3ull << 62;
			u64 topbits = codePoints & mask;
			codePoints = (v & ~mask) | topbits; // Ensure top bits are unchanged to mark 'data' union as dynamic string
		}

		u64 getCodePoints() const {
			return codePoints & ~(3ull << 62); // Hide top bits
		}

		bool hasCachedCodePoints() const {
			return getCodePoints() || used <= 1;
		}

		std::aligned_storage<8, 8> bufferPlaceHolder;

		u64 capacity;
		u64 used;
	protected:
		mutable u64 codePoints; // Marked as dirty if codePoints == 0 && used > 1 (more than '\0' is stored)
	};

	template<typename TBuffer>
	struct StringDataBase : StringDataInterface {

		StringDataBase() { new(&bufferPlaceHolder) TBuffer(); }

		StringDataBase(const StringDataBase& o)
			: capacity(o.capacity), used(o.used), codePoints(o.codePoints) {
			new(&bufferPlaceHolder) TBuffer(o.buffer());
		}

		StringDataBase(StringDataBase&& o)
			: capacity(o.capacity), used(o.used), codePoints(o.codePoints) {
			new(&bufferPlaceHolder) TBuffer(std::move(o.buffer()));
		}

		~StringDataBase() {
			buffer().~TBuffer();
		}

		StringDataBase& operator=(const StringDataBase& o) {
			capacity = o.capacity;
			used = o.used;
			codePoints = o.codePoints;
			buffer() = o.buffer();
			return *this;
		}

		TBuffer& buffer() { return *(TBuffer*)(&bufferPlaceHolder); }
		const TBuffer& buffer() const { return *(TBuffer*)(&bufferPlaceHolder); }
	};

	using TDynamicString = StringDataBase< SharedPtr<Shared<u8[]>> >;
	using TLiteralString = StringDataBase< const char* >;

	static constexpr u64 TSmallCapacity = sizeof(TDynamicString);

	union TData {
		u8 bytes[TSmallCapacity];

		TypedAlignedStorage<TDynamicString> dyn;
		TypedAlignedStorage<TLiteralString> lit;

		TData() : bytes() {}
		~TData() {};
	} data;

	enum class Mode {
		Small,
		Shared,
		Owned,
		Literal
	};

	bool isSmall() const {
		return (data.bytes[TSmallCapacity - 1] & 0xC0) == 0x00; // Dynmic bit cleared, literal bit cleared
	}

	bool isDynamic() const {
		return (data.bytes[TSmallCapacity - 1] & 0xC0) == 0x80; // Dynamic bit set, literal bit cleared
	}

	bool isLiteral() const {
		return (data.bytes[TSmallCapacity - 1] & 0xC0) == 0x40; // Dynamic bit cleared, literal bit set
	}

	bool isShared() const {
		return isDynamic() && dyn().buffer().refCount() > 1;
	}


	Mode mode() const;
	void setMode(Mode m);

	TDynamicString& dyn() {
		assert(isDynamic());
		return data.dyn.value();
	}

	const TDynamicString& dyn() const {
		assert(isDynamic());
		return data.dyn.value();
	}

	TLiteralString& lit() {
		assert(isLiteral());
		return data.lit.value();
	}

	const TLiteralString& lit() const {
		assert(isLiteral());
		return data.lit.value();
	}

	void copySmallString(const String& s) {
		assert(s.isSmall());
		initAsSmallString(s.data.bytes, s.bufferSize());
	}

	void initAsSmallString(const u8* ptr, u64 len);
	void initAsSmallString();
	void initAsLiteralString(const char* s, u64 len);

	const u8* safeBufferPointer() const;

	bool hasCachedCodePointsLitOrDyn() const;
	void resetCodePointsLitOrDyn();

	u64 countCodePoints() const {
		return Character::countCodePointsInBuffer(safeBufferPointer(), bufferSize() - 1);
	}

	void ensureOwnedCapacity(u64 numBytes);
	void growIntoDynamicString(u64 numBytes);
	void appendBytes(const u8* bytes, u64 numBytes);

	friend class StringIntrospection;

public:
	class CharRef {
	private:
		String& str;
		u64 idx;

	public:
		CharRef(String& s, u64 i) : str(s), idx(i) {}

		operator Character() { return str.charAt(idx); }

		CharRef& operator= (Character c) {
			str.setCharAt(idx, c);
			return *this;
		}
	};

	String() {
		initAsSmallString();
	}

	String(const String& s);
	String(String&& s);
	explicit String(const char* s, std::optional<u64> knownLen = {});

	template<unsigned int N>
	String(const char(&s)[N]) /* : String(s, N) {}*/ {
		if constexpr (N <= TSmallCapacity) {
			initAsSmallString((const u8*)s, N);
			return;
		}

		initAsLiteralString(s, N);
	}

	~String();

	u64 bufferCapacity() const;
	u64 bufferSize() const;
	u64 length() const;

	String& append(Character c) {
		appendBytes(c.bytes(), c.byteCount());
		return *this;
	}

	String& append(const String& s);
	String& append(String&& s);
	String& append(const char* s);

	Character charAt(u64 idx) const {
		auto ptr = Character::getCodePointInBufferAt(safeBufferPointer(), bufferSize() - 1, idx);
		assert(ptr);
		return { ptr };
	}

	void setCharAt(u64 idx, Character c);

	CharRef operator[](u64 idx) { return { *this, idx }; }
	Character operator[](u64 idx) const { return charAt(idx); }

	const char* cString() const {
		return (const char*)safeBufferPointer();
	}

	bool isEmpty() const {
		return bufferSize() <= 1;
	}

	void reserve(u64 numBytes= 0) {
		if ((bufferCapacity() < numBytes) || (mode() == Mode::Shared) || (mode() == Mode::Literal)) {
			ensureOwnedCapacity(numBytes);
		}
	}
};


class StringIntrospection {
public:
	StringIntrospection(String& s) : str(s) {}

	using Mode = String::Mode;
	Mode mode() const { return str.mode(); }

	static const char* modeToString(Mode m) {
		switch (m) {
		case Mode::Owned:	return "Owned";
		case Mode::Shared:	return "Shared";
		case Mode::Small:	return "Small";
		case Mode::Literal:	return "Literal";
		}
	}

	const char* modeAsStr() const { return modeToString(mode()); }

	bool isSmall() const { return str.isSmall(); }
	bool isShared() const { return str.isShared(); }
	bool isDynamic() const { return str.isDynamic(); }
	bool isLiteral() const { return str.isLiteral(); }

	using DynString = String::TDynamicString;
	const DynString& dynamicData() const { return str.dyn(); }

private:
	String& str;
};

std::ostream& operator << (std::ostream& o, StringIntrospection::Mode m);
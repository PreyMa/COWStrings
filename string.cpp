
#include "string.h"

std::ostream& operator << (std::ostream& o, StringIntrospection::Mode m) {
	o << StringIntrospection::modeToString(m);
	return o;
}

String::Mode String::mode() const {
	if (isSmall()) {
		return Mode::Small;
	}

	if (isLiteral()) {
		return Mode::Literal;
	}

	if (isShared()) {
		return Mode::Shared;
	}

	return Mode::Owned;
}

void String::setMode(Mode m) {
	data.bytes[TSmallCapacity - 1] &= 0x3F;

	switch (m) {
	case Mode::Small:
		return;
	case Mode::Literal:
		data.bytes[TSmallCapacity - 1] |= 0x40;
		return;
	case Mode::Owned:
	case Mode::Shared:
		data.bytes[TSmallCapacity - 1] |= 0x80;
		return;
	}
}

void String::initAsSmallString(const u8* ptr, u64 len) {
	// Set small mode string to null-terminated c-string
	assert(len <= TSmallCapacity);
	memcpy(data.bytes, ptr, len);
	data.bytes[TSmallCapacity - 1] = (TSmallCapacity - len);
	data.bytes[len] = '\0';
	setMode(Mode::Small);
}

void String::initAsSmallString() {
	data.bytes[TSmallCapacity - 1] = TSmallCapacity - 1;
	data.bytes[0] = '\0';
}

void String::initAsLiteralString(const char* s, u64 len) {
	assert(s);

	data.lit.construct();
	setMode(Mode::Literal);
	lit().buffer() = s;
	lit().capacity = 0;
	lit().used = len;
	lit().setCodePoints(0);
}

const u8* String::safeBufferPointer() const {
	if (isSmall()) {
		return data.bytes;
	}

	if (isLiteral() && lit().buffer()) {
		return (u8*)lit().buffer();
	}

	if (isDynamic() && dyn().buffer()) {
		return dyn().buffer().dataPtr();
	}

	static char defaultEmptyString[] = "";
	return (const u8*)defaultEmptyString;
}

bool String::hasCachedCodePointsLitOrDyn() const {
	if (isLiteral()) {
		return lit().hasCachedCodePoints();
	}
	if (isDynamic()) {
		return dyn().hasCachedCodePoints();
	}

	return false;
}

void String::resetCodePointsLitOrDyn() {
	if (isLiteral()) {
		lit().setCodePoints(0);
	}
	if (isDynamic()) {
		dyn().setCodePoints(0);
	}
}

void String::ensureOwnedCapacity(u64 numBytes) {
	if (!isDynamic()) {
		growIntoDynamicString(numBytes);
		return;
	}

	// There is still enough space in the owned buffer
	auto curCapacity = bufferCapacity();
	auto hasSpace = curCapacity >= numBytes;
	if (!isShared() && hasSpace) {
		return;
	}

	// Make an owned buffer, that will be larger if needed
	// At least as large as the number of bytes requested and double the small version size
	// The buffer size doubles by default.
	auto newCapacity = hasSpace ? curCapacity : std::max({ curCapacity * 2, numBytes, TSmallCapacity * 2 });
	auto newBuffer = Shared<u8[]>::make(newCapacity);

	// If there already exists a (possibly shared) buffer, the contents are copied
	if (dyn().buffer()) {
		memcpy(newBuffer.ptr()->value, dyn().buffer().dataPtr(), dyn().used);
	}

	dyn().buffer() = std::move(newBuffer);
	dyn().capacity = newCapacity;
}

void String::growIntoDynamicString(u64 numBytes) {
	assert(isSmall() || isLiteral());
	// Allocate dyn memory and store the characters there
	auto newCapacity = std::max(TSmallCapacity * 2, numBytes);
	auto newBuffer = Shared<u8[]>::make(newCapacity);
	auto ptr = newBuffer.ptr()->value;
	u64 used = bufferSize();
	memcpy(ptr, safeBufferPointer(), used);
	ptr[used] = '\0';

	data.dyn.construct();
	setMode(Mode::Owned); // Construct zero initializes all PODs
	dyn().buffer() = std::move(newBuffer);
	dyn().capacity = newCapacity;
	dyn().used = used;
	resetCodePointsLitOrDyn();
}

void String::appendBytes(const u8* bytes, u64 numBytes) {
	// Append non-null terminated bytes
	auto cap = bufferCapacity();
	auto used = bufferSize();

	// There is still enough space in the small string (the string obj itself)
	if (isSmall() && (used + numBytes <= cap)) {
		memcpy(data.bytes + used - 1, bytes, numBytes);
		data.bytes[TSmallCapacity - 1] = (TSmallCapacity - used - numBytes);
		data.bytes[used + numBytes - 1] = '\0';
		return;
	}

	auto oldDataPtr = isDynamic() ? dyn().buffer().dataPtr() : nullptr;

	ensureOwnedCapacity(used + numBytes);

	// If the string is append to itself, reallocation on buffer expansion would make the following a use after free
	if (bytes == oldDataPtr) {
		bytes = dyn().buffer().dataPtr();
	}

	memcpy(dyn().buffer().dataPtr() + used - 1, bytes, numBytes);
	dyn().buffer()[used + numBytes - 1] = '\0';
	dyn().used = used + numBytes;

	// If the number of appended bytes is not too long, just count them
	if (dyn().hasCachedCodePoints() && numBytes <= 64) {
		auto numPoints = Character::countCodePointsInBuffer(bytes, numBytes);
		dyn().setCodePoints(dyn().getCodePoints() + numPoints);
	}
	else {
		resetCodePointsLitOrDyn();
	}
}

String::String(const String& s) {

	// Copy the content of the small string
	if (s.isSmall()) {
		copySmallString(s);
		return;
	}

	if (s.isLiteral()) {
		// Copy pointer to the literal
		data.lit.value() = s.data.lit.value();
		return;
	}

	// Copy everything, incrementing the ref counter of the buffer
	// making them a shared string
	data.dyn.value() = s.data.dyn.value();
}

String::String(String&& s) {
	if (s.isSmall()) {
		copySmallString(s);
		return;
	}

	if (s.isLiteral()) {
		data.lit.value() = data.lit.value();
		return;
	}

	// Gut the buffer from the moved string
	data.dyn.construct();
	setMode(Mode::Owned); // Could also be shared (construct zero initialized all PODs)
	dyn().capacity = s.dyn().capacity;
	dyn().used = s.dyn().used;
	dyn().setCodePoints(s.dyn().getCodePoints());
	dyn().buffer() = std::move(s.dyn().buffer());
	s.initAsSmallString();
}

String::String(const char* s, std::optional<u64> knownLen) {
	auto l = knownLen ? *knownLen : strlen(s) + 1;
	if (l <= TSmallCapacity) {
		initAsSmallString((const u8*)s, l);
		return;
	}

	data.dyn.construct();
	setMode(Mode::Owned); // After zero init all PODs
	ensureOwnedCapacity(l);
	memcpy(dyn().buffer().dataPtr(), s, l);
	dyn().used = l;
	resetCodePointsLitOrDyn();
}

String::~String() {
	if (isDynamic()) {
		// data.dyn.destroy();
		dyn().buffer().reset();
	}
}

u64 String::bufferCapacity() const {
	if (isSmall()) {
		return TSmallCapacity;
	}

	if (isLiteral()) {
		return 0;
	}

	return dyn().buffer() ? dyn().capacity : 0;
}

u64 String::bufferSize() const {
	if (isSmall()) {
		return TSmallCapacity - data.bytes[TSmallCapacity - 1];
	}

	if (isLiteral()) {
		return lit().used;
	}

	return dyn().buffer() ? dyn().used : 0;
}

u64 String::length() const {
	if (isSmall()) {
		return countCodePoints();
	}

	if (isLiteral()) {
		if (!lit().hasCachedCodePoints()) {
			lit().setCodePoints(countCodePoints());
		}

		return lit().getCodePoints();
	}

	if (!dyn().buffer()) {
		return 0;
	}

	// If more than a '\0' is stored in the buffer and codePoints == 0
	// -> count the number of code points and store them
	if (!dyn().hasCachedCodePoints()) {
		dyn().setCodePoints(countCodePoints());
	}

	return dyn().getCodePoints();
}

String& String::append(const String& s) {
	u64 newCodePoints = 0;
	// Either one is small and the other one is large and has a cached codePoint count
	// or both are large and have a cached code point count
	if ((s.isSmall() && hasCachedCodePointsLitOrDyn()) ||
		(s.hasCachedCodePointsLitOrDyn() && isSmall()) ||
		(s.hasCachedCodePointsLitOrDyn() && hasCachedCodePointsLitOrDyn())) {
		newCodePoints = length() + s.length();

		// Reset code points, so that 'appendBytes' does not try to calculate anything
		resetCodePointsLitOrDyn();
	}

	appendBytes((u8*)s.cString(), s.bufferSize() - 1);

	if (!isSmall()) {
		dyn().setCodePoints(newCodePoints);
	}

	return *this;
}

String& String::append(String&& s) {
	// The string s owns its buffer and has space for this strings bytes
	if ((s.mode() == Mode::Owned) && (s.bufferCapacity() >= s.bufferSize() + bufferSize() - 1)) {
		// This string does not own its buffer and the buffer of s is large enough for the concated data
		// Or this string owns its buffer, but it is too small
		// Or both buffers could hold the data but s has at least more additional capacity as this string contains
		// (-> a very adhoc heuristic, this could be more sophisticated)
		if ((mode() != Mode::Owned) ||
			((mode() == Mode::Owned) && (bufferCapacity() < s.bufferSize() + bufferSize() - 1)) ||
			((mode() == Mode::Owned) && (s.bufferCapacity() - bufferCapacity() > bufferSize()))) {
			// Move the data inside s back and insert this strings data
			if (!isEmpty()) {
				memmove(s.dyn().buffer().dataPtr() + bufferSize() - 1, s.dyn().buffer().dataPtr(), s.bufferSize());
				memcpy(s.dyn().buffer().dataPtr(), safeBufferPointer(), bufferSize() - 1);
			}

			u64 newCodePoints = 0;
			if ((isSmall() || hasCachedCodePointsLitOrDyn()) && s.dyn().hasCachedCodePoints()) {
				newCodePoints = length() + s.length();
			}

			// Safe old buffer usage before a possible switch to dynamic mode
			u64 oldUsage = bufferSize();

			if (!isDynamic()) {
				data.dyn.construct();
				setMode(Mode::Owned);
			}

			// Gut the buffer and own it (<- the buffer that is)
			dyn().capacity = s.dyn().capacity;
			dyn().used = oldUsage + s.dyn().used - 1;
			dyn().setCodePoints(newCodePoints);
			dyn().buffer() = std::move(s.dyn().buffer());

			s.initAsSmallString();
			return *this;
		}
	}
	// Or eles do regular append
	return append(s);
}

String& String::append(const char* s) {
	u64 numBytes;
	u64 numCodePoints = Character::countCodePointsInCString(s, &numBytes);

	// String is has cached codepoints or it is small but it will become large
	u64 newCodePoints = 0;
	if (hasCachedCodePointsLitOrDyn() ||
		(isSmall() && (bufferSize() + numBytes) > TSmallCapacity)) {
		newCodePoints = length() + numCodePoints;
		resetCodePointsLitOrDyn();
	}

	appendBytes((const u8*)s, numBytes);

	if (!isSmall()) {
		dyn().setCodePoints(newCodePoints);
	}

	return *this;
}

void String::setCharAt(u64 idx, Character c) {
	auto used = bufferSize();
	auto bufferPtr = safeBufferPointer();
	auto posPtr = (u8*)Character::getCodePointInBufferAt(bufferPtr, used - 1, idx);
	assert(posPtr);

	auto oldCharSize = Character::byteLengthFromLeadingByte(*posPtr);
	auto newCharSize = c.byteCount();

	// Ensure the buffer is owned and there is enough space in it
	auto requiredCap = used - oldCharSize + newCharSize;
	auto offset = posPtr - bufferPtr;
	ensureOwnedCapacity(requiredCap);

	// Reinit the buffer pointer and posPointer after the buffer was reallocated
	bufferPtr = safeBufferPointer();
	posPtr = (u8*)bufferPtr + offset;

	// If the new code point uses more space than the old one
	if (newCharSize > oldCharSize) {

		// Make room for the new char by moving the tail back
		memmove(posPtr + newCharSize, posPtr + oldCharSize, used - (posPtr - bufferPtr) - oldCharSize);
	}

	// Copy the bytes over the old code point
	for (int i = 0; i != newCharSize; i++) {
		posPtr[i] = c.bytes()[i];
	}

	// If the new code point needs the same or less space
	if (newCharSize < oldCharSize) {
		// Move the tail of the string forward to close the gap
		memmove(posPtr + newCharSize, posPtr + oldCharSize, used - (posPtr - bufferPtr) - oldCharSize);
	}
}

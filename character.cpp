
#include "character.h"

std::ostream& operator << (std::ostream& o, Character c) {
	o.write((const char*)c.bytes(), c.byteCount());
	return o;
}

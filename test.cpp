#include "test.h"

std::ostream& operator<<(std::ostream& o, const Test& t) {
	t.printSummary(o);
	return o;
}

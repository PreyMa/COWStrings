#include "test.h"

std::ostream& operator<<(std::ostream& o, const Test& t) {
	t.printSummary(o);
	return o;
}

void Test::testFailed(const char* name, Testing::ExpectationException* e) {
	failedTestCount++;
	if (e && printFailedTests) {
		std::cout << "Failed test '" << name << "': ";
		e->print(std::cout);
	}
}

void Test::testSucceeded() { /*NOP*/ }

void Test::printSummary(std::ostream& o) const {
	if (failedTestCount) {
		o << "[FAILURES] ";
	}
	else {
		o << "[SUCCESS ] ";
	}
	o << (testCount - failedTestCount) << '/' << testCount << " tests passed. (" << failedTestCount << " tests failed)\n";
}

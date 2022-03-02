#pragma once

#include <exception>
#include <optional>
#include <iostream>

#include "mem.h"


namespace Testing {

	// Check if an object can be written to a std::ostream with operator <<
	template<typename T, typename = void>
	struct isStreamWritable : std::false_type {};

	template<typename T>
	struct isStreamWritable<T, std::void_t< decltype(std::declval<std::ostream&>() << std::declval<T>()) >>
		: std::true_type {};


	// Check if an object can be compared via operator <=
	template<typename T, typename = void>
	struct isLessEqualComparable : std::false_type {};

	template<typename T>
	struct isLessEqualComparable<T, std::void_t< decltype(std::declval<T>() <= std::declval<T>()) >>
		: std::true_type {};


	// Check if an object is iterable
	// from: https://stackoverflow.com/questions/13830158/check-if-a-variable-type-is-iterable
	template<typename T, typename = void>
	struct isIterable : std::false_type {};

	template<typename T>
	struct isIterable<T, std::void_t< decltype(
			begin(std::declval<T&>()) != end(std::declval<T&>()),	// begin/end and operator !=
			void(),													// Handle evil operator ,
			++std::declval<decltype(begin(std::declval<T&>()))&>(), // operator ++
			void(*begin(std::declval<T&>()))
		) >> : std::true_type {};


	
	class ExpectedValueInterface : public RefCounted<ExpectedValueInterface> {
	public:
		// Virtual abstract methods

		virtual ~ExpectedValueInterface() {}
	};


	// Does equality check via operator ==
	template<typename T>
	class ExpectedValueBase : public ExpectedValueInterface {
	public:

		ExpectedValueBase(const T& v) : value(v) {}

		void toBe(const T& exp) {
			if ((value == exp) == isInverted) {
				throw ExpectedValueException<T>(exp, value);
			}
		}

	protected:
		const T& value;
		bool isInverted{ false };
	};

	// Any generic type that supports operator ==
	template<typename T, typename = void>
	class ExpectedValue : public ExpectedValueBase<T> {
	public:
		using ExpectedValueBase<T>::ExpectedValueBase;

		ExpectedValue & not() { isInverted = !isInverted; return *this; }
	};

	// Booleans
	template<>
	class ExpectedValue<bool, void> : public ExpectedValueBase<bool> {
	public:
		using ExpectedValueBase<bool>::ExpectedValueBase;

		ExpectedValue & not() { isInverted = !isInverted; return *this; }

		void toBeTrue() {
			toBe(true);
		}

		void toBeFalse() {
			toBe(false);
		}
	};

	// Any type that supports operator == and has begin/end methods returning iterators
	template<typename T>
	class ExpectedValue<T, std::enable_if_t<isIterable<T>::value>> : public ExpectedValueBase<T> {
	public:
		using ExpectedValueBase<T>::ExpectedValueBase;

		ExpectedValue & not() { isInverted = !isInverted; return *this; }

		void toBeEmpty() {
			if ((std::begin(value) != std::end(value)) != isInverted) {
				throw ExpectedStateException<bool>( isInverted ? "be not empty" : "be empty");
			}
		}

		void toHaveLenght(u64 len) {
			auto dist = std::distance(std::begin(value), std::end(value));
			if ((dist == len) == isInverted) {
				throw ExpectedStateException<u64>("have length of", len, dist);
			}
		}

		template<typename U>
		void toContain(const U& item) {
			auto end = std::end(value);
			if ((std::find(std::begin(value), end, item) != end) == isInverted) {
				throw ExpectedStateException<U>(isInverted ? "not contain" : "contain", item);
			}
		}
	};

	// Any type that supports operator == and operator <= for range checks
	template<typename T>
	class ExpectedValue<T, std::enable_if_t<isLessEqualComparable<T>::value && !isIterable<T>::value>> : public ExpectedValueBase<T> {
	public:
		using ExpectedValueBase<T>::ExpectedValueBase;

		ExpectedValue & not() { isInverted = !isInverted; return *this; }

		void toBeZero() {
			toBe(0);
		}

		void toBeInside(u64 lower, u64 upper) {
			if ((lower <= value && value <= upper) == isInverted) {
				throw ExpectedRangeException<u64>(lower, upper, value);
			}
		}
	};


	class ExpectationException : public std::exception {
	public:
		virtual const char* what() const override {
			return "Received value did not match the expected one";
		}

		virtual void print(std::ostream& o) const = 0;

	protected:
		// Either prints an object to a std::ostream via operator << or shows a default text
		template<typename P, typename = void>
		struct StreamPrinter {
			static void print(std::ostream& o, const P& v) {
				o << "<Not printable>";
			}
		};

		template<typename P>
		struct StreamPrinter<P, std::enable_if_t<isStreamWritable<P>::value>> {
			static void print(std::ostream& o, const P& v) {
				o << std::boolalpha;
				o << '\'' << v << '\'';
			}
		};
	};

	// Thrown if 'toBe'-test is not passed (takes values as references)
	template<typename T>
	class ExpectedValueException : public ExpectationException {
	public:

		ExpectedValueException(const T& e, const T& r)
			: expectedValue(e), receivedValue(r) {}

		virtual void print(std::ostream& o) const override {
			o << "Expected ";
			StreamPrinter<T>::print(o, expectedValue);
			o << " wich differs from received value: ";
			StreamPrinter<T>::print(o, receivedValue);
			o << '\n';
		}

	private:


		const T& receivedValue;
		const T& expectedValue;
	};

	// Thrown if state-test is not passed (takes values as copies)
	template<typename T>
	class ExpectedStateException : public ExpectationException {
	public:

		ExpectedStateException(const char* m, T e, T r)
			: msg(m), expectedValue(e), receivedValue(r) {}


		ExpectedStateException(const char* m, T e)
			: msg(m), expectedValue(e) {}

		ExpectedStateException(const char* m)
			: msg(m) {}

		virtual void print(std::ostream& o) const override {
			o << "Expected value to " << msg << ' ';
			
			if (expectedValue) {
				StreamPrinter<T>::print(o, expectedValue.value());
			}

			if (receivedValue) {
				o << " wich differs from: ";
				StreamPrinter<T>::print(o, receivedValue.value());
			}

			o << '\n';
		}

	private:

		const char* msg;
		const std::optional<T> expectedValue;
		const std::optional<T> receivedValue;
	};

	// Thrown if 'toBeInside'-test is not passed
	template<typename T>
	class ExpectedRangeException : public ExpectationException {
	public:

		ExpectedRangeException(const T& l, const T& u, const T& r)
			: expectedLowerValue(l), expectedUpperValue(u), receivedValue(r) {}

		virtual void print(std::ostream& o) const override {
			o << "Expected a value inside (";
			StreamPrinter<T>::print(o, expectedLowerValue);
			o << ", ";
			StreamPrinter<T>::print(o, expectedUpperValue);
			o << ") wich differs from received value: ";
			StreamPrinter<T>::print(o, receivedValue);
			o << '\n';
		}

	private:


		const T& receivedValue;
		const T& expectedLowerValue;
		const T& expectedUpperValue;
	};

};

class Test {

	void testFailed(const char* name, Testing::ExpectationException* e) {
		failedTestCount++;
		if (e && printFailedTests) {
			std::cout << "Failed test '" << name << "': ";
			e->print(std::cout);
		}
	}

	void testSucceeded() {

	}
public:
	template<typename T>
	Test& test(const char* name, const T& testFunction) {
		testCount++;

		try {
			testFunction();
		}
		catch (Testing::ExpectationException& e) {
			testFailed(name, &e);
			return *this;
		}
		catch (std::exception& e) {
			std::cout << "Uncaught exception in test '" << name << "': '" << e.what() << "'\n";
			testFailed(name, nullptr);
			return *this;
		}
		catch (...) {
			std::cout << "Uncaught exception in test '" << name << "': <Unknwon exception>\n";
			testFailed(name, nullptr);
			return *this;
		}

		testSucceeded();
		return *this;
	}

	template<typename T>
	auto expect(const T& val) {
		return OwnPtr<Testing::ExpectedValue<T>>(new Testing::ExpectedValue<T>(val));
	}

	void printSummary(std::ostream& o) const {
		if (failedTestCount) {
			o << "[FAILURES] ";
		}
		else {
			o << "[SUCCESS ] ";
		}
		o << (testCount - failedTestCount) << '/' << testCount << " tests passed. (" << failedTestCount << " tests failed)\n";
	}

	void mute(bool v = true) {
		printFailedTests = !v;
	}

private:
	u64 testCount{ 0 };
	u64 failedTestCount{ 0 };
	bool printFailedTests{ true };
};

std::ostream& operator << (std::ostream& o, const Test& t);


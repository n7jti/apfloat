#include <apfloat/apfloat.hpp>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
int failures = 0;
void check(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; ++failures; }
}
apfloat::number value(const char* text, std::size_t precision = 128) {
    apfloat::number n;
    check(n.set_precision(precision) == apfloat::error::none, "set precision");
    check(apfloat::number::parse(text, n) == apfloat::error::none, text);
    return n;
}
void text(const apfloat::number& n, const char* expected) {
    if (n.to_string() != expected) {
        std::cerr << "expected " << expected << ", got " << n.to_string() << '\n';
        ++failures;
    }
}
}

int main() {
    using apfloat::error;
    using apfloat::number;
    number n;
    check(n.precision() == 128, "default precision");
    check(n.set_precision(0) == error::invalid_precision, "zero precision");
    check(n.set_precision(number::maximum_precision + 1) == error::invalid_precision, "precision limit");
    check(n.precision() == 128, "invalid precision changes value");
    check(n.set_precision(number::maximum_precision) == error::none, "maximum representable precision for zero");
    check(n.set_precision(33219413) == error::none, "precision beyond ten million decimal digits");
    check(number::parse("1", n) == error::none, "parse at large precision");
    text(n, "1");
    check(n.set_precision(1) == error::none, "round large precision value");
    text(n, "1");
    text(value("123.5"), "123.5");
    text(value("-0"), "-0");
    text(value("1.25e2"), "125");
    text(value(".125"), "0.125");
    text(value("inf"), "inf");
    text(value("-infinity"), "-inf");
    text(value("nan"), "nan");
    n = value("42");
    for (const char* invalid : {"", "+", ".", "1e", "1.2.3", " 1", "1x"}) {
        check(number::parse(invalid, n) == error::invalid_syntax, "invalid syntax");
        text(n, "42");
    }
    check(number::parse("1e99999999999999999999999", n) == error::out_of_range, "decimal exponent limit");
    text(n, "42");
    text(value("1e100001"), "inf");
    text(value("0e100001"), "0");
    text(value("1.125", 3), "1");
    text(value("1.375", 3), "1.5");
    text(value("1.875", 3), "2");
    n = value("1.375");
    check(n.set_precision(3) == error::none, "reduce precision");
    text(n, "1.5");
    check(n.set_precision(20) == error::none, "increase precision");
    text(n, "1.5");
    text(number::divide(value("1", 3), value("3", 3), apfloat::rounding::nearest), "0.3125");
    // Quotients independently rounded with exact .NET BigInteger rational arithmetic.
    text(number::divide(value("1", 128), value("3", 128), apfloat::rounding::nearest),
         "0.333333333333333333333333333333333333333823122646175953128320306890509269032424443981988369800628646544282673858106136322021484375");
    text(number::divide(value("1", 65), value("10", 65), apfloat::rounding::nearest),
         "0.1000000000000000000013552527156068805425093160010874271392822265625");
    text(number::divide(value("1", 256), value("7", 256), apfloat::rounding::nearest),
         "0.142857142857142857142857142857142857142857142857142857142857142857142857142856834422551603769834807630290614271443888714272698704236249153447493978864172770462215303303447418277622837596644780369157250175037613144760940964150819354472332634031772613525390625");
    text(number::add(value("1", 3), value("0.125", 3), apfloat::rounding::nearest), "1");
    text(number::multiply(value("1.5", 3), value("1.5", 3), apfloat::rounding::nearest), "2");
    text(number::subtract(value("9"), value("10"), apfloat::rounding::nearest), "-1");
    text(number::add(value("-0"), value("-0"), apfloat::rounding::nearest), "-0");
    text(number::subtract(value("-0"), value("-0"), apfloat::rounding::nearest), "0");
    text(number::multiply(value("-2"), value("0"), apfloat::rounding::nearest), "-0");
    text(number::divide(value("1"), value("-0"), apfloat::rounding::nearest), "-inf");
    text(number::divide(value("0"), value("0"), apfloat::rounding::nearest), "nan");
    text(number::subtract(value("inf"), value("inf"), apfloat::rounding::nearest), "nan");
    text(number::multiply(value("inf"), value("0"), apfloat::rounding::nearest), "nan");
    text(number::divide(value("inf"), value("inf"), apfloat::rounding::nearest), "nan");
    text(value("1e5000"), "inf");
    text(value("-1e-5000"), "-0");
    text(value("3e4932"), "inf");
    check(value("1e4932") < value("inf"), "finite upper exponent boundary");
    text(value("4e-4933"), "0");
    check(value("5e-4933") == value("8e-4933"), "underflow rounds to minimum finite");
    check(value("0") < value("5e-4933"), "minimum finite is nonzero");
    check(value("5e-4933") < value("1e-4932"), "lower exponent boundary");
    check(value("-0") == value("0"), "signed zero equality");
    check(value("-inf") < value("-123"), "negative infinity order");
    check(value("-2") < value("-1"), "negative order");
    check(value("1") < value("2"), "positive order");
    check(!(value("nan") == value("nan")), "NaN equality");
    check(value("nan") != value("nan"), "NaN inequality");
    check(!(value("nan") <= value("0")), "NaN ordering");
    auto a = value("1", 5), b = value("1", 6);
    check(a == b && !(a != b) && !(a < b) && !(a > b) && a <= b && a >= b,
          "mixed-precision equality and ordering");
    auto low = value("1.5", 3), high = value("1.5625", 65);
    check(low < high && high > low && low != high && low <= high && high >= low,
          "mixed-precision positive ordering");
    check(value("-1.5625", 65) < value("-1.5", 3), "mixed-precision negative ordering");
    check(value("0.5", 1) < value("1", 100), "mixed-precision exponent ordering");
    check(value("1", 1) == value("1", 100), "mixed-precision limb alignment");
    check(value("1", 1) < value("1.0000000000000000000542101086242752217003726400434970855712890625", 65),
          "mixed-precision low-bit ordering");
    check(value("-1.0000000000000000000542101086242752217003726400434970855712890625", 65) <
          value("-1", 1), "mixed-precision negative low-bit ordering");
    check(value("-0", 1) == value("0", 100), "mixed-precision signed zeros");
    check(value("-inf", 1) < value("0", 100) &&
          value("inf", 1) == value("inf", 100), "mixed-precision infinities");
    check(!(value("nan", 1) == value("nan", 100)) &&
          !(value("nan", 1) < value("1", 100)), "mixed-precision NaN");
    for (std::size_t p : {4u, 5u, 31u, 32u, 33u, 64u, 65u, 100u}) {
        for (std::size_t q : {4u, 5u, 31u, 32u, 33u, 64u, 65u, 100u}) {
            for (int x = -7; x <= 7; ++x) {
                for (int y = -7; y <= 7; ++y) {
                    number left = value(std::to_string(x).c_str(), p);
                    number right = value(std::to_string(y).c_str(), q);
                    check((left == right) == (x == y) && (left < right) == (x < y) &&
                          (left > right) == (x > y), "mixed-precision integer comparison");
                }
            }
        }
    }
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}

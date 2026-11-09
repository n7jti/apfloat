#include <apfloat/apfloat.hpp>
#include <cmath>
#include <cstdint>
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
std::uint64_t integer_sqrt(std::uint64_t value) {
    std::uint64_t low = 0;
    std::uint64_t high = std::uint64_t(1) << 32;
    while (low + 1 < high) {
        const std::uint64_t middle = low + (high - low) / 2;
        if (middle <= value / middle) low = middle;
        else high = middle;
    }
    return low;
}
unsigned bit_count(std::uint64_t value) {
    unsigned result = 0;
    while (value != 0) { ++result; value >>= 1; }
    return result;
}
double expected_sqrt(std::uint64_t significand, int exponent, std::size_t precision,
                     apfloat::rounding mode) {
    const int leading = exponent + static_cast<int>(bit_count(significand)) - 1;
    const int top = leading >= 0 ? leading / 2 : -((-leading + 1) / 2);
    const int result_exponent = top - static_cast<int>(precision - 1);
    const unsigned scale = static_cast<unsigned>(exponent - 2 * result_exponent);
    const std::uint64_t radicand = significand << scale;
    std::uint64_t root = integer_sqrt(radicand);
    const std::uint64_t remainder = radicand - root * root;
    if (remainder != 0 && mode == apfloat::rounding::upward) ++root;
    if (mode == apfloat::rounding::nearest && remainder > root) ++root;
    return std::ldexp(static_cast<double>(root), result_exponent);
}
void check_sqrt(std::uint64_t significand, int exponent, std::size_t precision,
                apfloat::rounding mode) {
    apfloat::number input = value(std::to_string(significand).c_str(), precision);
    const auto factor = value(exponent < 0 ? "0.5" : "2", precision);
    for (int i = 0; i < (exponent < 0 ? -exponent : exponent); ++i)
        input = apfloat::number::multiply(input, factor, apfloat::rounding::nearest);
    const double actual = std::stod(apfloat::number::sqrt(input, mode).to_string());
    const double expected = expected_sqrt(significand, exponent, precision, mode);
    check(actual == expected, "small exact square-root rounding");
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
    check(n.set_precision(std::size_t(1) << 32) == error::none, "precision exceeds 32-bit range for zero");
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
    check(value("1e100001") < value("inf"), "large decimal exponent remains finite");
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
    text(number::sqrt(value("4"), apfloat::rounding::nearest), "2");
    text(number::sqrt(value("2", 3), apfloat::rounding::nearest), "1.5");
    text(number::sqrt(value("2", 3), apfloat::rounding::downward), "1.25");
    text(number::sqrt(value("2", 3), apfloat::rounding::upward), "1.5");
    text(number::sqrt(value("-0"), apfloat::rounding::nearest), "-0");
    text(number::sqrt(value("-4"), apfloat::rounding::nearest), "nan");
    text(number::sqrt(value("-inf"), apfloat::rounding::nearest), "nan");
    text(number::sqrt(value("inf"), apfloat::rounding::nearest), "inf");
    text(number::sqrt(value("nan"), apfloat::rounding::nearest), "nan");
    for (std::size_t p = 1; p <= 12; ++p) {
        for (std::uint64_t significand = std::uint64_t(1) << (p - 1);
             significand < (std::uint64_t(1) << p); ++significand) {
            for (int exponent = -3; exponent <= 3; ++exponent) {
                for (auto mode : {apfloat::rounding::nearest, apfloat::rounding::downward,
                                  apfloat::rounding::upward})
                    check_sqrt(significand, exponent, p, mode);
            }
        }
        std::uint32_t random = 0x9e3779b9u;
        for (unsigned i = 0; i < 256; ++i) {
            random = random * 1664525u + 1013904223u;
            const std::uint64_t significand = 0x80000000u | random;
            for (int exponent = -3; exponent <= 3; ++exponent) {
                for (auto mode : {apfloat::rounding::nearest, apfloat::rounding::downward,
                                  apfloat::rounding::upward})
                    check_sqrt(significand, exponent, 32, mode);
            }
        }
        text(number::sqrt(value("18446744065119617025", 64), apfloat::rounding::nearest),
             "4294967295");
    }
    text(number::divide(value("0"), value("0"), apfloat::rounding::nearest), "nan");
    text(number::subtract(value("inf"), value("inf"), apfloat::rounding::nearest), "nan");
    text(number::multiply(value("inf"), value("0"), apfloat::rounding::nearest), "nan");
    text(number::divide(value("inf"), value("inf"), apfloat::rounding::nearest), "nan");
    check(value("1e5000") < value("inf"), "expanded positive exponent");
    check(value("0") < value("1e-5000"), "expanded negative exponent");
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

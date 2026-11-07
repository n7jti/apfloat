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
    check(n.set_precision(number::maximum_precision) == error::none, "maximum precision");
    check(number::parse("1", n) == error::none, "parse at maximum precision");
    text(n, "1");
    check(n.set_precision(1) == error::none, "round maximum precision value");
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
    check(number::parse("1e100001", n) == error::out_of_range, "decimal exponent limit");
    text(n, "42");
    text(value("1.125", 3), "1");
    text(value("1.375", 3), "1.5");
    text(value("1.875", 3), "2");
    n = value("1.375");
    check(n.set_precision(3) == error::none, "reduce precision");
    text(n, "1.5");
    check(n.set_precision(20) == error::none, "increase precision");
    text(n, "1.5");
    text(value("1", 3) / value("3", 3), "0.3125");
    // Quotients independently rounded with exact .NET BigInteger rational arithmetic.
    text(value("1", 128) / value("3", 128),
         "0.333333333333333333333333333333333333333823122646175953128320306890509269032424443981988369800628646544282673858106136322021484375");
    text(value("1", 65) / value("10", 65),
         "0.1000000000000000000013552527156068805425093160010874271392822265625");
    text(value("1", 256) / value("7", 256),
         "0.142857142857142857142857142857142857142857142857142857142857142857142857142856834422551603769834807630290614271443888714272698704236249153447493978864172770462215303303447418277622837596644780369157250175037613144760940964150819354472332634031772613525390625");
    text(value("1", 3) + value("0.125", 3), "1");
    text(value("1.5", 3) * value("1.5", 3), "2");
    text(value("9") - value("10"), "-1");
    text(value("-0") + value("-0"), "-0");
    text(value("-0") - value("-0"), "0");
    text(value("-2") * value("0"), "-0");
    text(value("1") / value("-0"), "-inf");
    text(value("0") / value("0"), "nan");
    text(value("inf") - value("inf"), "nan");
    text(value("inf") * value("0"), "nan");
    text(value("inf") / value("inf"), "nan");
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
    int exceptions = 0;
    try { (void)(a + b); } catch (const apfloat::precision_mismatch&) { ++exceptions; }
    try { (void)(a - b); } catch (const apfloat::precision_mismatch&) { ++exceptions; }
    try { (void)(a * b); } catch (const apfloat::precision_mismatch&) { ++exceptions; }
    try { (void)(a / b); } catch (const apfloat::precision_mismatch&) { ++exceptions; }
    try { (void)(a == b); } catch (const apfloat::precision_mismatch&) { ++exceptions; }
    try { (void)(a < b); } catch (const apfloat::precision_mismatch&) { ++exceptions; }
    try { (void)(a != b); } catch (const apfloat::precision_mismatch&) { ++exceptions; }
    try { (void)(a <= b); } catch (const apfloat::precision_mismatch&) { ++exceptions; }
    try { (void)(a > b); } catch (const apfloat::precision_mismatch&) { ++exceptions; }
    try { (void)(a >= b); } catch (const apfloat::precision_mismatch&) { ++exceptions; }
    check(exceptions == 10, "precision mismatch operators");
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}

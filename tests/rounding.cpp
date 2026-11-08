#include <apfloat/apfloat.hpp>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <initializer_list>
#include <limits>
#include <string>

namespace {
int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        ++failures;
    }
}

apfloat::number value(const std::string& text, std::size_t precision = 4) {
    apfloat::number n;
    check(n.set_precision(precision) == apfloat::error::none, "set precision");
    check(apfloat::number::parse(text, n) == apfloat::error::none, "parse operand");
    return n;
}

double bound(int numerator, int denominator, bool upward) {
    if (denominator < 0) {
        numerator = -numerator;
        denominator = -denominator;
    }
    double best = upward ? std::numeric_limits<double>::infinity()
                         : -std::numeric_limits<double>::infinity();
    for (int exponent = -12; exponent <= 12; ++exponent) {
        for (int significand = 8; significand <= 15; ++significand) {
            double magnitude = std::ldexp(static_cast<double>(significand), exponent - 3);
            for (double candidate : {-magnitude, magnitude}) {
                double scaled = candidate * denominator;
                if (upward && scaled >= numerator && candidate < best) best = candidate;
                if (!upward && scaled <= numerator && candidate > best) best = candidate;
            }
        }
    }
    if (numerator == 0) return 0;
    return best;
}

void check_bound(const apfloat::number& n, int numerator, int denominator, bool upward) {
    double actual = std::stod(n.to_string());
    double expected = bound(numerator, denominator, upward);
    if (actual != expected) {
        std::cerr << "incorrect " << (upward ? "upper" : "lower") << " bound for "
                  << numerator << '/' << denominator << ": " << actual
                  << " expected " << expected << '\n';
        ++failures;
    }
}
}

int main() {
    using apfloat::number;
    using apfloat::rounding;
    for (int x = -7; x <= 7; ++x) {
        for (int y = -7; y <= 7; ++y) {
            for (bool reverse : {false, true}) {
                number a = value(std::to_string(x), reverse ? 4 : 3);
                number b = value(std::to_string(y), reverse ? 3 : 4);
                for (rounding mode : {rounding::downward, rounding::upward}) {
                    bool upward = mode == rounding::upward;
                    auto sum = number::add(a, b, mode);
                    auto difference = number::subtract(a, b, mode);
                    auto product = number::multiply(a, b, mode);
                    check(sum.precision() == 4 && difference.precision() == 4 &&
                          product.precision() == 4, "mixed-precision arithmetic result precision");
                    check_bound(sum, x + y, 1, upward);
                    check_bound(difference, x - y, 1, upward);
                    check_bound(product, x * y, 1, upward);
                    if (y != 0) {
                        auto quotient = number::divide(a, b, mode);
                        check(quotient.precision() == 4, "mixed-precision division result precision");
                        check_bound(quotient, x, y, upward);
                    }
                }
            }
        }
    }

    number n;
    check(n.set_precision(3) == apfloat::error::none, "set precision for parsing");
    check(number::parse("1.125", n, rounding::downward) == apfloat::error::none && n.to_string() == "1", "parse down");
    check(number::parse("1.125", n, rounding::upward) == apfloat::error::none && n.to_string() == "1.25", "parse up");
    check(number::parse("-1.125", n, rounding::downward) == apfloat::error::none && n.to_string() == "-1.25", "negative parse down");
    check(number::parse("-1.125", n, rounding::upward) == apfloat::error::none && n.to_string() == "-1", "negative parse up");
    n = value("1.125", 10);
    check(n.set_precision(3, rounding::upward) == apfloat::error::none && n.to_string() == "1.25", "precision up");
    n = value("-1.125", 10);
    check(n.set_precision(3, rounding::downward) == apfloat::error::none && n.to_string() == "-1.25", "precision down");

    auto wide_one = value("1", 128), wide_three = value("3", 128);
    check(number::divide(wide_one, wide_three, rounding::downward).to_string() ==
          "0.33333333333333333333333333333333333333235375470764809374335938621898146193515111203602326039874270691143465228378772735595703125",
          "multi-limb division down");
    check(number::divide(wide_one, wide_three, rounding::upward).to_string() ==
          "0.333333333333333333333333333333333333333823122646175953128320306890509269032424443981988369800628646544282673858106136322021484375",
          "multi-limb division up");

    auto one = value("1"), minus_one = value("-1");
    check(number::add(one, minus_one, rounding::downward).to_string() == "-0", "downward exact cancellation");
    check(number::add(one, minus_one, rounding::upward).to_string() == "0", "upward exact cancellation");
    check(number::add(value("-0"), value("0"), rounding::downward).to_string() == "-0", "downward mixed zero");
    check(number::divide(value("1"), value("0"), rounding::downward).to_string() == "inf", "division by zero");
    check(number::multiply(value("0"), value("inf"), rounding::upward).to_string() == "nan", "invalid arithmetic");
    check(number::parse("1e-5000", n, rounding::downward) == apfloat::error::none && n.to_string() == "0", "positive underflow down");
    check(number::parse("1e-5000", n, rounding::upward) == apfloat::error::none && value("0", n.precision()) < n, "positive underflow up");
    check(number::parse("-1e-5000", n, rounding::upward) == apfloat::error::none && n.to_string() == "-0", "negative underflow up");
    check(number::parse("-1e-5000", n, rounding::downward) == apfloat::error::none && n < value("0", n.precision()), "negative underflow down");
    check(number::parse("1e5000", n, rounding::upward) == apfloat::error::none && n.to_string() == "inf", "positive overflow up");
    check(number::parse("1e5000", n, rounding::downward) == apfloat::error::none && n < value("inf", n.precision()), "positive overflow down");
    check(number::parse("-1e5000", n, rounding::downward) == apfloat::error::none && n.to_string() == "-inf", "negative overflow down");
    check(number::parse("-1e5000", n, rounding::upward) == apfloat::error::none && value("-inf", n.precision()) < n, "negative overflow up");

    for (bool reverse : {false, true}) {
        number low = value("inf", 3), high = value("1", 70);
        number result = reverse ? number::add(high, low, rounding::nearest)
                                : number::add(low, high, rounding::nearest);
        check(result.precision() == 70 && result.to_string() == "inf", "mixed-precision infinity result");
        low = value("0", 3);
        result = reverse ? number::multiply(high, low, rounding::nearest)
                         : number::multiply(low, high, rounding::nearest);
        check(result.precision() == 70 && result.to_string() == "0", "mixed-precision zero result");
        low = value("nan", 3);
        result = reverse ? number::divide(high, low, rounding::nearest)
                         : number::divide(low, high, rounding::nearest);
        check(result.precision() == 70 && result.to_string() == "nan", "mixed-precision NaN result");
    }
    check(number::add(value("1", 2), value("0.125", 4), rounding::nearest).to_string() == "1.125",
          "mixed-precision nearest addition");
    check(number::divide(value("1", 2), value("3", 4), rounding::nearest).to_string() == "0.34375",
          "mixed-precision nearest division");
    auto small = value("1.5", 3);
    auto large = value("2", 65);
    check(number::subtract(small, large, rounding::nearest).to_string() == "-0.5",
          "mixed-precision subtraction");
    check(number::multiply(small, large, rounding::nearest).to_string() == "3",
          "mixed-precision multiplication");
    check(small.precision() == 3 && large.precision() == 65 &&
          small.to_string() == "1.5" && large.to_string() == "2",
          "mixed-precision operands remain unchanged");
    for (rounding mode : {rounding::nearest, rounding::downward, rounding::upward}) {
        auto nan = number::subtract(value("inf", 2), value("inf", 65), mode);
        auto infinity = number::divide(value("1", 2), value("-0", 65), mode);
        auto zero = number::divide(value("-1", 2), value("inf", 65), mode);
        check(nan.precision() == 65 && nan.to_string() == "nan", "mixed-precision invalid infinity");
        check(infinity.precision() == 65 && infinity.to_string() == "-inf", "mixed-precision division by zero");
        check(zero.precision() == 65 && zero.to_string() == "-0", "mixed-precision signed zero");
    }
    bool invalid_mode = false;
    try { (void)number::parse("1", n, static_cast<rounding>(-1)); }
    catch (const std::invalid_argument&) { invalid_mode = true; }
    check(invalid_mode, "invalid rounding mode");
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}

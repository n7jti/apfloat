#pragma once

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace apfloat {

enum class error {
    none,
    invalid_precision,
    invalid_syntax,
    out_of_range
};

enum class rounding {
    nearest,
    downward,
    upward
};

class precision_mismatch : public std::logic_error {
public:
    precision_mismatch();
};

class number {
public:
    static constexpr std::size_t default_precision = 128;
    // Keep intermediate bit counts and exponent sums representable as int.
    static constexpr std::size_t maximum_precision =
        static_cast<std::size_t>((std::numeric_limits<int>::max)() / 4) - 32768;

    number() = default;

    std::size_t precision() const noexcept { return precision_; }
    error set_precision(std::size_t precision);
    error set_precision(std::size_t precision, rounding mode);
    static error parse(std::string_view text, number& result);
    static error parse(std::string_view text, number& result, rounding mode);
    static number add(const number& a, const number& b, rounding mode);
    static number subtract(const number& a, const number& b, rounding mode);
    static number multiply(const number& a, const number& b, rounding mode);
    static number divide(const number& a, const number& b, rounding mode);
    std::string to_string() const;

    friend bool operator==(const number&, const number&);
    friend bool operator<(const number&, const number&);
    friend bool operator!=(const number& a, const number& b) { return !(a == b); }
    friend bool operator>(const number& a, const number& b) { return b < a; }
    friend bool operator<=(const number& a, const number& b) { return (a < b) || (a == b); }
    friend bool operator>=(const number& a, const number& b) { return (b < a) || (a == b); }

private:
    enum class kind { finite, infinity, nan };
    std::size_t precision_ = default_precision;
    kind kind_ = kind::finite;
    bool negative_ = false;
    int exponent_ = 0;
    std::vector<unsigned int> limbs_;

    struct implementation;
};

}

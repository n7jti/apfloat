#pragma once

#include <cstddef>
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

class precision_mismatch : public std::logic_error {
public:
    precision_mismatch();
};

class number {
public:
    static constexpr std::size_t default_precision = 128;
    static constexpr std::size_t maximum_precision = 65536;

    number() = default;

    std::size_t precision() const noexcept { return precision_; }
    error set_precision(std::size_t precision);
    static error parse(std::string_view text, number& result);
    std::string to_string() const;

    friend number operator+(const number&, const number&);
    friend number operator-(const number&, const number&);
    friend number operator*(const number&, const number&);
    friend number operator/(const number&, const number&);
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

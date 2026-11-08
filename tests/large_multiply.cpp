#include <apfloat/apfloat.hpp>

#include <cstdlib>
#include <iostream>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
template<typename T, typename = void> struct has_add : std::false_type {};
template<typename T> struct has_add<T, std::void_t<decltype(std::declval<T>() + std::declval<T>())>> : std::true_type {};
template<typename T, typename = void> struct has_subtract : std::false_type {};
template<typename T> struct has_subtract<T, std::void_t<decltype(std::declval<T>() - std::declval<T>())>> : std::true_type {};
template<typename T, typename = void> struct has_multiply : std::false_type {};
template<typename T> struct has_multiply<T, std::void_t<decltype(std::declval<T>() * std::declval<T>())>> : std::true_type {};
template<typename T, typename = void> struct has_divide : std::false_type {};
template<typename T> struct has_divide<T, std::void_t<decltype(std::declval<T>() / std::declval<T>())>> : std::true_type {};

static_assert(!has_add<apfloat::number>::value && !has_subtract<apfloat::number>::value &&
              !has_multiply<apfloat::number>::value && !has_divide<apfloat::number>::value,
              "arithmetic must use the named rounding-aware API");

std::string digits(std::size_t count, unsigned seed) {
    std::string result(count, '0');
    for (char& digit : result) {
        seed = seed * 1664525u + 1013904223u;
        digit = static_cast<char>('0' + (seed >> 16) % 10);
    }
    result[0] = '9';
    return result;
}

std::string decimal_product(const std::string& a, const std::string& b) {
    std::vector<unsigned> result(a.size() + b.size(), 0);
    for (std::size_t i = a.size(); i-- > 0;) {
        for (std::size_t j = b.size(); j-- > 0;) {
            result[i + j + 1] += static_cast<unsigned>(a[i] - '0') *
                                 static_cast<unsigned>(b[j] - '0');
        }
    }
    for (std::size_t i = result.size(); i-- > 1;) {
        result[i - 1] += result[i] / 10;
        result[i] %= 10;
    }
    std::string output;
    std::size_t begin = result[0] == 0 ? 1 : 0;
    for (std::size_t i = begin; i < result.size(); ++i)
        output.push_back(static_cast<char>('0' + result[i]));
    return output;
}

bool test(std::size_t a_digits, std::size_t b_digits, bool all_nines = false) {
    const std::string a_text = all_nines ? std::string(a_digits, '9') : digits(a_digits, 1);
    const std::string b_text = all_nines ? std::string(b_digits, '9') : digits(b_digits, 2);
    const std::size_t precision = 4 * (a_digits + b_digits + 1);
    apfloat::number a, b;
    if (a.set_precision(precision) != apfloat::error::none ||
        b.set_precision(precision) != apfloat::error::none ||
        apfloat::number::parse(a_text, a) != apfloat::error::none ||
        apfloat::number::parse(b_text, b) != apfloat::error::none) {
        std::cerr << "unable to prepare multiplication case\n";
        return false;
    }
    const std::string actual =
        apfloat::number::multiply(a, b, apfloat::rounding::nearest).to_string();
    const std::string expected = decimal_product(a_text, b_text);
    if (actual != expected) {
        std::cerr << "multiplication mismatch for " << a_digits << " and "
                  << b_digits << " decimal digits\n";
        return false;
    }
    return true;
}
}

int main() {
    for (auto lengths : {std::pair<std::size_t, std::size_t>{300, 300},
                         {310, 310}, {320, 320}, {620, 620},
                         {1000, 1000}, {1000, 330}, {1300, 700}, {2400, 2400}}) {
        if (!test(lengths.first, lengths.second)) return EXIT_FAILURE;
    }
    if (!test(1000, 1000, true)) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

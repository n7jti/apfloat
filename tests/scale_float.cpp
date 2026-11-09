#include <apfloat/apfloat.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

int main() {
    constexpr std::size_t precision = 33219413;
    apfloat::number one, three;
    if (one.set_precision(precision) != apfloat::error::none ||
        three.set_precision(precision) != apfloat::error::none ||
        apfloat::number::parse("1", one) != apfloat::error::none ||
        apfloat::number::parse("3", three) != apfloat::error::none) {
        std::cerr << "unable to prepare full-scale operands\n";
        return EXIT_FAILURE;
    }
    auto start = std::chrono::steady_clock::now();
    auto lower = apfloat::number::divide(one, three, apfloat::rounding::downward);
    auto upper = apfloat::number::divide(one, three, apfloat::rounding::upward);
    auto elapsed_divide = std::chrono::steady_clock::now();
    auto product = apfloat::number::multiply(lower, three, apfloat::rounding::nearest);
    auto elapsed_multiply = std::chrono::steady_clock::now();
    auto two = apfloat::number::add(one, one, apfloat::rounding::nearest);
    auto root_lower = apfloat::number::sqrt(two, apfloat::rounding::downward);
    auto root_upper = apfloat::number::sqrt(two, apfloat::rounding::upward);
    auto lower_square = apfloat::number::multiply(root_lower, root_lower, apfloat::rounding::downward);
    auto upper_square = apfloat::number::multiply(root_upper, root_upper, apfloat::rounding::upward);
    auto elapsed_sqrt = std::chrono::steady_clock::now();
    // This exact >10-million-digit format/parse round-trip can take 10+ minutes; run only as an explicit scale test.
    std::string decimal = lower.to_string();
    auto elapsed_format = std::chrono::steady_clock::now();
    apfloat::number round_trip;
    if (round_trip.set_precision(precision) != apfloat::error::none ||
        apfloat::number::parse(decimal, round_trip) != apfloat::error::none) {
        std::cerr << "full-scale decimal round-trip parse failed\n";
        return EXIT_FAILURE;
    }
    auto elapsed_parse = std::chrono::steady_clock::now();
    if (!(lower < upper) || !(product == one) || lower_square > two || upper_square < two ||
        !(round_trip == lower) ||
        decimal.size() <= 10000000) {
        std::cerr << "full-scale public arithmetic check failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "precision_bits=" << precision << " directed_divisions_ms="
              << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_divide - start).count()
              << " multiplication_ms="
              << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_multiply - elapsed_divide).count()
              << " sqrt_ms="
              << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_sqrt - elapsed_multiply).count()
              << " decimal_digits=" << decimal.size()
              << " format_ms="
              << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_format - elapsed_sqrt).count()
              << " parse_ms="
              << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_parse - elapsed_format).count()
              << '\n';
    return EXIT_SUCCESS;
}

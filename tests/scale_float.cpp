#include <apfloat/apfloat.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>

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
    if (!(lower < upper) || !(product == one)) {
        std::cerr << "full-scale public arithmetic check failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "precision_bits=" << precision << " directed_divisions_ms="
              << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_divide - start).count()
              << " multiplication_ms="
              << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_multiply - elapsed_divide).count()
              << '\n';
    return EXIT_SUCCESS;
}

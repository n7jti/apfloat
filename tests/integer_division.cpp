// Compile the implementation here to check private quotient and remainder directly.
#include "../src/apfloat.cpp"

#include <cstdlib>
#include <iostream>
#include <random>
#include <vector>

namespace {
using digits = std::vector<unsigned int>;

void trim(digits& n) {
    while (!n.empty() && n.back() == 0) n.pop_back();
}

int compare(const digits& a, const digits& b) {
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    for (std::size_t i = a.size(); i-- > 0;) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

std::pair<digits, digits> reference_divide(const digits& a, const digits& b) {
    digits quotient(a.size(), 0), remainder;
    for (std::size_t bit = a.size() * 32; bit-- > 0;) {
        std::uint32_t carry = (a[bit / 32] >> (bit % 32)) & 1;
        for (auto& limb : remainder) {
            std::uint32_t next = limb >> 31;
            limb = (limb << 1) | carry;
            carry = next;
        }
        if (carry) remainder.push_back(carry);
        if (compare(remainder, b) >= 0) {
            std::uint64_t borrow = 0;
            for (std::size_t i = 0; i < remainder.size(); ++i) {
                std::uint64_t subtrahend = borrow + (i < b.size() ? b[i] : 0);
                std::uint32_t old = remainder[i];
                remainder[i] = static_cast<std::uint32_t>(old - subtrahend);
                borrow = old < subtrahend ? 1 : 0;
            }
            trim(remainder);
            quotient[bit / 32] |= std::uint32_t(1) << (bit % 32);
        }
    }
    trim(quotient);
    return {quotient, remainder};
}

bool test(digits a, digits b) {
    trim(a);
    trim(b);
    if (b.empty()) return false;
    auto expected = reference_divide(a, b);
    auto actual = apfloat::divide_integers(apfloat::integer(a), apfloat::integer(b));
    if (actual.first.v != expected.first || actual.second.v != expected.second) {
        std::cerr << "division mismatch for " << a.size() << " / " << b.size()
                  << " limbs\n";
        return false;
    }
    return true;
}

digits product_with_remainder(const digits& quotient, const digits& divisor,
                              const digits& remainder) {
    digits result(quotient.size() + divisor.size() + 1, 0);
    for (std::size_t i = 0; i < quotient.size(); ++i) {
        std::uint64_t carry = 0;
        for (std::size_t j = 0; j < divisor.size(); ++j) {
            std::uint64_t value = std::uint64_t(quotient[i]) * divisor[j] +
                                  result[i + j] + carry;
            result[i + j] = static_cast<unsigned int>(value);
            carry = value >> 32;
        }
        result[i + divisor.size()] = static_cast<unsigned int>(carry);
    }
    std::uint64_t carry = 0;
    for (std::size_t i = 0; i < result.size(); ++i) {
        std::uint64_t value = std::uint64_t(result[i]) +
                              (i < remainder.size() ? remainder[i] : 0) + carry;
        result[i] = static_cast<unsigned int>(value);
        carry = value >> 32;
    }
    trim(result);
    return result;
}

bool test_constructed(digits quotient, digits divisor, digits remainder) {
    trim(quotient);
    trim(divisor);
    trim(remainder);
    const digits dividend = product_with_remainder(quotient, divisor, remainder);
    auto actual = apfloat::divide_integers(apfloat::integer(dividend), apfloat::integer(divisor));
    if (actual.first.v != quotient || actual.second.v != remainder) {
        std::cerr << "constructed division mismatch for " << dividend.size() << " / "
                  << divisor.size() << " limbs\n";
        return false;
    }
    return true;
}
}

int main() {
    std::mt19937 generator(0x86537912u);
    for (unsigned trial = 0; trial < 12000; ++trial) {
        digits a(1 + generator() % 32), b(1 + generator() % 20);
        for (auto& limb : a) limb = generator();
        for (auto& limb : b) limb = generator();
        if (trial % 8 == 0) b.back() = 1;
        if (trial % 8 == 1) b.back() = 0x80000000u;
        if (trial % 8 == 2) b.back() = 0xffffffffu;
        if (trial % 8 == 3) for (auto& limb : a) limb = 0xffffffffu;
        if (!test(a, b)) return EXIT_FAILURE;
    }
    for (unsigned bit = 0; bit < 128; ++bit) {
        digits divisor(1 + bit / 32, 0), dividend(9, 0xffffffffu);
        divisor[bit / 32] = std::uint32_t(1) << (bit % 32);
        if (!test(dividend, divisor)) return EXIT_FAILURE;
    }
    for (unsigned shift = 1; shift <= 96; ++shift) {
        apfloat::integer dividend(123456789u), divisor(87654321u);
        dividend = dividend.shifted(shift);
        divisor = divisor.shifted(shift);
        if (!test(dividend.v, divisor.v)) return EXIT_FAILURE;
    }
    for (std::size_t length : {64u, 65u, 127u, 128u, 129u, 255u, 256u, 257u}) {
        for (unsigned trial = 0; trial < 8; ++trial) {
            digits a(length * 2 + trial % 3), b(length);
            for (auto& limb : a) limb = generator();
            for (auto& limb : b) limb = generator();
            if (trial == 0) b.back() = 1;
            if (trial == 1) b.back() = 0x80000000u;
            if (trial == 2) b.back() = 0xffffffffu;
            if (!test(a, b)) return EXIT_FAILURE;
        }
    }
    for (std::size_t length : {65u, 128u, 129u, 512u, 1024u}) {
        for (unsigned trial = 0; trial < 5; ++trial) {
            digits divisor(length), quotient(length * (trial == 4 ? 3 : 1)), remainder(length - 1);
            for (auto& limb : divisor) limb = generator();
            for (auto& limb : quotient) limb = generator();
            for (auto& limb : remainder) limb = generator();
            if (trial == 0) {
                std::fill(divisor.begin(), divisor.end(), 0xffffffffu);
                std::fill(quotient.begin(), quotient.end(), 0xffffffffu);
            }
            if (trial == 1) divisor.back() = 1;
            if (trial == 2) remainder.clear();
            if (!test_constructed(quotient, divisor, remainder)) return EXIT_FAILURE;
        }
    }
    for (std::size_t length : {128u, 256u, 512u}) {
        digits divisor(length, 0xffffffffu), quotient(length / 2, 0xffffffffu);
        digits remainder = divisor;
        --remainder[0];
        if (!test_constructed(quotient, divisor, remainder)) return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

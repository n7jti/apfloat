#include <apfloat/apfloat.hpp>

#include <cstdlib>
#include <iostream>
#include <initializer_list>
#include <string>

namespace {
std::string multiply_decimal(std::string value, unsigned factor) {
    unsigned carry = 0;
    for (std::size_t i = value.size(); i-- > 0;) {
        unsigned digit = static_cast<unsigned>(value[i] - '0') * factor + carry;
        value[i] = static_cast<char>('0' + digit % 10);
        carry = digit / 10;
    }
    if (carry) value.insert(value.begin(), static_cast<char>('0' + carry));
    return value;
}

bool check_integer(std::size_t digits) {
    std::string input;
    for (std::size_t i = 0; i < digits; ++i)
        input += static_cast<char>('0' + ((i * 37 + 13) % 10));
    input[0] = '7';
    apfloat::number n;
    if (n.set_precision(digits * 4) != apfloat::error::none ||
        apfloat::number::parse(input, n) != apfloat::error::none ||
        n.to_string() != input) {
        std::cerr << "decimal integer conversion failed for " << digits << " digits\n";
        return false;
    }
    return true;
}

bool check_fraction(unsigned exponent) {
    std::string numerator = "1";
    for (unsigned i = 0; i < exponent; ++i)
        numerator = multiply_decimal(numerator, 5);
    std::string expected = "0." + std::string(exponent - numerator.size(), '0') + numerator;
    apfloat::number n;
    if (n.set_precision(exponent + 1) != apfloat::error::none ||
        apfloat::number::parse(expected, n) != apfloat::error::none ||
        n.to_string() != expected) {
        std::cerr << "exact decimal fraction conversion failed for 2^-" << exponent << '\n';
        return false;
    }
    return true;
}
}

int main() {
    for (std::size_t length = 1; length <= 80; ++length)
        if (!check_integer(length)) return EXIT_FAILURE;
    for (std::size_t length : {127u, 128u, 129u, 287u, 288u, 289u, 1000u, 3000u})
        if (!check_integer(length)) return EXIT_FAILURE;
    for (unsigned exponent : {1u, 29u, 32u, 127u, 1024u, 3000u})
        if (!check_fraction(exponent)) return EXIT_FAILURE;
    apfloat::number n;
    std::string large_integer(100001, '9');
    apfloat::number infinity;
    if (apfloat::number::parse("inf", infinity) != apfloat::error::none)
        return EXIT_FAILURE;
    if (apfloat::number::parse(large_integer, n) != apfloat::error::none ||
        !(n < infinity)) {
        std::cerr << "decimal input beyond former digit limit failed\n";
        return EXIT_FAILURE;
    }
    std::string large_zero = "-0." + std::string(10000001, '0');
    if (apfloat::number::parse(large_zero, n) != apfloat::error::none ||
        n.to_string() != "-0") {
        std::cerr << "ten-million-place zero conversion failed\n";
        return EXIT_FAILURE;
    }
    if (apfloat::number::parse("-0e1000000000", n) != apfloat::error::none ||
        n.to_string() != "-0" ||
        apfloat::number::parse("0e-1000000000", n) != apfloat::error::none ||
        n.to_string() != "0") {
        std::cerr << "large zero decimal exponents failed\n";
        return EXIT_FAILURE;
    }
    const std::size_t decimal_limit = apfloat::number::maximum_precision / 8;
    if (apfloat::number::parse("-0e" + std::to_string(decimal_limit), n) != apfloat::error::none ||
        n.to_string() != "-0" ||
        apfloat::number::parse("0e" + std::to_string(decimal_limit + 1), n) != apfloat::error::out_of_range ||
        n.to_string() != "-0" ||
        apfloat::number::parse("0e-" + std::to_string(decimal_limit + 1), n) != apfloat::error::out_of_range ||
        n.to_string() != "-0") {
        std::cerr << "decimal scale representational boundary failed\n";
        return EXIT_FAILURE;
    }
    if (apfloat::number::parse("0000000123000000e-6", n) != apfloat::error::none ||
        n.to_string() != "123" ||
        apfloat::number::parse("1." + std::string(1000000, '0'), n) != apfloat::error::none ||
        n.to_string() != "1") {
        std::cerr << "decimal zero normalization failed\n";
        return EXIT_FAILURE;
    }
    const std::string large_fraction = "1." + std::string(100001, '0') + "125";
    if (apfloat::number::parse(large_fraction, n, apfloat::rounding::downward) != apfloat::error::none ||
        n.to_string() != "1") {
        std::cerr << "large nonzero fractional conversion down failed\n";
        return EXIT_FAILURE;
    }
    if (apfloat::number::parse(large_fraction, n, apfloat::rounding::upward) != apfloat::error::none ||
        n.to_string() != "1.0000000000000000000000000000000000000058774717541114375398436826861112283890933277838604376075437585313920862972736358642578125") {
        std::cerr << "large nonzero fractional conversion up failed\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

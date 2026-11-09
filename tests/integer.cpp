#include <apfloat/apfloat.hpp>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        ++failures;
    }
}
}

int main() {
    apfloat::integer zero;
    apfloat::integer one(1);
    apfloat::integer max(0xffffffffu);
    check(zero.zero(), "default construction is zero");
    check(!one.zero(), "nonzero construction");
    check(zero.compare(one) < 0 && one.compare(one) == 0 && one.compare(zero) > 0,
          "comparison");

    max.add(1);
    check(max.to_string() == "4294967296", "addition carries across limbs");
    max.multiply(0xffffffffu);
    check(max.to_string() == "18446744069414584320", "small multiplication");
    max.add(4294967295u);
    check(max.to_string() == "18446744073709551615", "small addition");

    apfloat::integer product(123456789u);
    product.multiply(apfloat::integer(987654321u));
    check(product.to_string() == "121932631112635269", "integer multiplication");
    product.subtract(apfloat::integer(112635269u));
    check(product.to_string() == "121932631000000000", "subtraction");
    product.subtract(product);
    check(product.zero() && product.to_string() == "0", "zero decimal formatting");

    bool threw = false;
    try { one.subtract(apfloat::integer(2)); }
    catch (const std::invalid_argument&) { threw = true; }
    check(threw, "negative subtraction rejected");
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}

#include <apfloat/apfloat.hpp>
#include <cstdlib>

int main() {
    apfloat::number n;
    if (apfloat::number::parse("42", n) != apfloat::error::none ||
        n.to_string() != "42") return EXIT_FAILURE;
    apfloat::number three;
    if (three.set_precision(64) != apfloat::error::none ||
        apfloat::number::parse("3", three) != apfloat::error::none ||
        apfloat::number::divide(n, three, apfloat::rounding::downward).precision() != n.precision() ||
        !(apfloat::number::divide(n, three, apfloat::rounding::downward) ==
          apfloat::number::divide(n, three, apfloat::rounding::upward))) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

#include <apfloat/apfloat.hpp>
#include <cstdlib>

int main() {
    apfloat::number n;
    if (apfloat::number::parse("42", n) != apfloat::error::none ||
        n.to_string() != "42") return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

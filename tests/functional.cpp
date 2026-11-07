#include <apfloat/apfloat.hpp>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>
#ifdef _WIN32
#include <crtdbg.h>
#include <windows.h>
#endif

int main(int argc, char**) {
    using apfloat::number;
    number a, b;
    if (argc > 1) {
#ifdef _WIN32
        SetErrorMode(SEM_NOGPFAULTERRORBOX);
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
        b.set_precision(64);
        (void)(a + b);
        return EXIT_SUCCESS;
    }
    if (a.set_precision(256) != apfloat::error::none ||
        b.set_precision(256) != apfloat::error::none ||
        number::parse("123456789012345678901234567890", a) != apfloat::error::none ||
        number::parse("98765432109876543210", b) != apfloat::error::none) return EXIT_FAILURE;
    // Exact integer fixtures verified with .NET BigInteger.
    if ((a + b).to_string() != "123456789111111111011111111100" ||
        (a - b).to_string() != "123456788913580246791358024680" ||
        (a * b).to_string() != "12193263113702179522496570642237463801111263526900") {
        std::cerr << "large integer fixture failed\n";
        return EXIT_FAILURE;
    }
    unsigned state = 0x12345678;
    for (int i = 0; i < 200; ++i) {
        state = state * 1664525u + 1013904223u;
        int x = static_cast<int>(state % 200001u) - 100000;
        state = state * 1664525u + 1013904223u;
        int y = static_cast<int>(state % 200001u) - 100000;
        if (number::parse(std::to_string(x), a) != apfloat::error::none ||
            number::parse(std::to_string(y), b) != apfloat::error::none ||
            (a + b).to_string() != std::to_string(x + y) ||
            (a - b).to_string() != std::to_string(x - y) ||
            (a * b).to_string() != std::to_string(static_cast<long long>(x) * y) ||
            (a < b) != (x < y) || (a == b) != (x == y)) {
            std::cerr << "native integer oracle failed for " << x << ", " << y << '\n';
            return EXIT_FAILURE;
        }
    }
    if (a.set_precision(53) != apfloat::error::none ||
        b.set_precision(53) != apfloat::error::none) return EXIT_FAILURE;
    for (int i = 0; i < 500; ++i) {
        state = state * 1664525u + 1013904223u;
        int x = static_cast<int>(state % 200001u) - 100000;
        state = state * 1664525u + 1013904223u;
        int y = static_cast<int>(state % 200000u) + 1;
        std::ostringstream oracle;
        oracle.imbue(std::locale::classic());
        oracle << std::setprecision(17) << (static_cast<double>(x) / y);
        number expected;
        if (expected.set_precision(53) != apfloat::error::none ||
            number::parse(std::to_string(x), a) != apfloat::error::none ||
            number::parse(std::to_string(y), b) != apfloat::error::none ||
            number::parse(oracle.str(), expected) != apfloat::error::none ||
            !((a / b) == expected)) {
            std::cerr << "native double oracle failed for " << x << " / " << y << '\n';
            return EXIT_FAILURE;
        }
    }
    if (a.set_precision(256) != apfloat::error::none ||
        b.set_precision(256) != apfloat::error::none) return EXIT_FAILURE;
    for (const char* input : {"0.1", "3.1415926535897932384626433832795028841971", "-1e-100", "1e100"}) {
        if (number::parse(input, a) != apfloat::error::none) return EXIT_FAILURE;
        std::string encoded = a.to_string();
        if (number::parse(encoded, b) != apfloat::error::none || !(a == b)) {
            std::cerr << "round trip failed for " << input << '\n';
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

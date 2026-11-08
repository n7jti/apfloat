#include <apfloat/apfloat.hpp>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "expected output CSV path\n"; return EXIT_FAILURE; }
    std::ofstream out(argv[1]);
    if (!out) { std::cerr << "cannot open benchmark output\n"; return EXIT_FAILURE; }
#ifdef _WIN32
    const char* platform = "windows";
#elif defined(__aarch64__)
    const char* platform = "linux-arm64";
#else
    const char* platform = "other";
#endif
    out << "platform,compiler,configuration,precision,operation,sample,iterations,nanoseconds\n";
    volatile std::size_t sink = 0;
    for (std::size_t precision : {64u, 128u, 256u}) {
        apfloat::number a, b;
        if (a.set_precision(precision) != apfloat::error::none ||
            b.set_precision(precision) != apfloat::error::none ||
            apfloat::number::parse("123456.789", a) != apfloat::error::none ||
            apfloat::number::parse("123.456", b) != apfloat::error::none) return EXIT_FAILURE;
        for (const std::string op : {"parse", "format", "add", "subtract", "multiply", "divide"}) {
            for (int sample = -1; sample < 5; ++sample) {
                auto start = std::chrono::steady_clock::now();
                for (int i = 0; i < 10; ++i) {
                    if (op == "parse") {
                        apfloat::number n;
                        if (n.set_precision(precision) != apfloat::error::none ||
                            apfloat::number::parse("123456.789", n) != apfloat::error::none) return EXIT_FAILURE;
                        sink += n == a ? 1 : 2;
                    } else if (op == "format") sink += a.to_string().size();
                    else if (op == "add") sink += apfloat::number::add(a, b, apfloat::rounding::nearest) < a ? 1 : 2;
                    else if (op == "subtract") sink += apfloat::number::subtract(a, b, apfloat::rounding::nearest) < a ? 1 : 2;
                    else if (op == "multiply") sink += apfloat::number::multiply(a, b, apfloat::rounding::nearest) < a ? 1 : 2;
                    else sink += apfloat::number::divide(a, b, apfloat::rounding::nearest) < a ? 1 : 2;
                }
                auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - start).count();
                if (sample >= 0) out << platform << ',' << APFLOAT_COMPILER << ','
                                     << APFLOAT_CONFIGURATION << ',' << precision << ',' << op << ','
                                     << sample << ",10," << elapsed << '\n';
            }
        }
    }
    if (!out) { std::cerr << "cannot write benchmark output\n"; return EXIT_FAILURE; }
    return sink == 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

// Compile the implementation here to validate private integer multiplication directly.
#include "../src/apfloat.cpp"

#include <cstdlib>
#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__)
#include <sys/resource.h>
#endif

namespace {
std::vector<unsigned int> reference_product(const std::vector<unsigned int>& a,
                                            const std::vector<unsigned int>& b) {
    std::vector<unsigned int> result(a.size() + b.size(), 0);
    for (std::size_t i = 0; i < a.size(); ++i) {
        std::uint64_t carry = 0;
        for (std::size_t j = 0; j < b.size(); ++j) {
            std::uint64_t sum = std::uint64_t(a[i]) * b[j] + result[i + j] + carry;
            result[i + j] = static_cast<unsigned int>(sum);
            carry = sum >> 32;
        }
        result[i + b.size()] = static_cast<unsigned int>(carry);
    }
    while (!result.empty() && result.back() == 0) result.pop_back();
    return result;
}

bool check(std::size_t left, std::size_t right, bool maximum, std::mt19937& random) {
    std::vector<unsigned int> a(left), b(right);
    for (auto& limb : a) limb = maximum ? 0xffffffffu : random();
    for (auto& limb : b) limb = maximum ? 0xffffffffu : random();
    const auto expected = reference_product(a, b);
    const auto actual = apfloat::multiply_integers(apfloat::integer(a), apfloat::integer(b));
    if (actual.v != expected) {
        std::cerr << "integer multiplication mismatch for " << left << " and " << right << " limbs\n";
        return false;
    }
    return true;
}

bool check_full_scale() {
    constexpr std::size_t limbs = (33219413 + 31) / 32;
    apfloat::integer a;
    a.v.assign(limbs, 0xffffffffu);
    auto start = std::chrono::steady_clock::now();
    apfloat::integer result = apfloat::multiply_integers(a, a);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    if (result.v.size() != 2 * limbs || result.v[0] != 1 ||
        result.v[limbs] != 0xfffffffeu) return false;
    for (std::size_t i = 1; i < limbs; ++i)
        if (result.v[i] != 0) return false;
    for (std::size_t i = limbs + 1; i < 2 * limbs; ++i)
        if (result.v[i] != 0xffffffffu) return false;

    std::uint64_t peak = 0;
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
        std::cerr << "unable to measure peak memory\n";
        return false;
    }
    peak = counters.PeakWorkingSetSize;
#elif defined(__linux__)
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        std::cerr << "unable to measure peak memory\n";
        return false;
    }
    peak = static_cast<std::uint64_t>(usage.ru_maxrss) * 1024;
#else
    std::cerr << "peak memory measurement is unavailable on this platform\n";
    return false;
#endif
    std::cout << "operand_bits=" << limbs * 32 << " elapsed_ms=" << elapsed
              << " peak_working_set_bytes=" << peak << '\n';
#if defined(__linux__) && defined(__aarch64__)
    if (peak >= (std::uint64_t(8) << 30)) {
        std::cerr << "peak memory exceeded 8 GiB\n";
        return false;
    }
#endif
    return true;
}
}

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--full") {
        if (!check_full_scale()) {
            std::cerr << "full-scale exact multiplication failed\n";
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
    if (argc != 1) {
        std::cerr << "unexpected arguments\n";
        return EXIT_FAILURE;
    }
    std::mt19937 random(0xb17b00b5u);
    for (std::size_t length : {511u, 512u, 513u, 1000u, 1024u, 1025u, 2048u}) {
        if (!check(length, length, false, random) || !check(length, length, true, random))
            return EXIT_FAILURE;
    }
    if (!check(1200, 700, false, random) || !check(1200, 700, true, random))
        return EXIT_FAILURE;
    for (std::size_t length : {31u, 32u, 33u, 511u, 512u, 513u}) {
        if (!check(length, length * 3 + 1, false, random) ||
            !check(length * 3 + 1, length, true, random))
            return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

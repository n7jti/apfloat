# apfloat

A C++17 binary floating-point static library with per-value precision, portable CMake builds, and dependency-free tests and benchmarks. The project was generated with GitHub Copilot and is intended for hobby calculations of pi on a Raspberry Pi. The supported targets are MSVC on Windows and native 64-bit Raspberry Pi OS/Linux with a C++17 compiler. The Raspberry Pi workflow requires a separately provisioned runner.

## Build and test

Install CMake 3.20 or newer and a C++17 compiler, then from a suitable compiler environment:

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release -L "unit|functional" --output-on-failure
ctest --test-dir build -C Release -L performance --output-on-failure
cmake --install build --config Release --prefix install
```

The installed package exports `apfloat::apfloat`; consume it with `find_package(apfloat CONFIG REQUIRED)` and set `CMAKE_PREFIX_PATH` to the installation prefix. Set `BUILD_TESTING=OFF` to build only the library. `tests/consumer` is a minimal external consumer.

## Usage

```cpp
#include <apfloat/apfloat.hpp>
#include <iostream>

int main() {
    apfloat::number a, b;
    if (a.set_precision(256) != apfloat::error::none ||
        b.set_precision(256) != apfloat::error::none ||
        apfloat::number::parse("1.5", a) != apfloat::error::none ||
        apfloat::number::parse("2", b) != apfloat::error::none) {
        return 1;
    }
    std::cout << (a * b).to_string() << '\n'; // 3
}
```

## Numeric and error contract

- Each `number` defaults to positive zero at 128 bits of binary significand precision. `precision()` inspects that precision; `set_precision` accepts 1 through 65,536 bits. Reducing precision immediately rounds the stored value to nearest, ties to even. Increasing it preserves the value but cannot recover information already rounded away.
- Binary arithmetic and comparisons require equal precision, including for zero, infinity, and NaN. A mismatch throws `apfloat::precision_mismatch`; the library does not catch it. An application that leaves this exception uncaught terminates; C++ callers can still catch it.
- Finite arithmetic and decimal parsing round to nearest, ties to even. Normalized nonzero finite values have a leading-bit exponent from -16,384 through 16,384, inclusive. There are no subnormal values: values below the minimum round to zero or the minimum finite value according to nearest-even rounding. Overflow rounds to signed infinity.
- Signed zeros, infinities and NaNs follow IEEE-style arithmetic and unordered NaN comparisons. The library does not claim full IEEE 754 compliance, support status flags or selectable rounding, or preserve NaN payloads.
- `number::parse` accepts decimal numbers with an optional sign, decimal point and `e`/`E` exponent, or lowercase `inf`, `infinity`, and `nan`, optionally signed. Whitespace is not accepted. Inputs are limited to 100,000 significand digits and a decimal scale between -100,000 and 100,000. `to_string()` emits exact decimal text that round-trips at the same precision; it is not necessarily the shortest representation.
- `parse` and `set_precision` return explicit `error` values for invalid input or limits and leave the destination unchanged on those errors. IEEE-style division by zero and invalid arithmetic yield infinity or NaN, rather than an error result.
- Allocation failures and other unexpected C++ exceptions are not caught by the library. Operating-system faults such as access violations are not portable C++ exceptions and are not handled by the library.

## Tests and performance

CTest labels are `unit`, `functional`, and `performance`. The suites check multi-limb and rounded-division fixtures independently calculated with .NET `BigInteger`, and compare deterministic arithmetic with native integer and `double` calculations. The performance test writes `build/benchmark.csv`: each operation and precision has one warm-up and five recorded samples of ten iterations. The parse and arithmetic measurements include a comparison to consume each result. Results include platform, compiler and build configuration. They are informational and have no regression threshold; compare runs only on suitably controlled machines.

## CI

The Windows GitHub Actions workflow builds, runs unit and functional tests and benchmarks, and uploads the installed static library and CSV results. The Raspberry Pi workflow is manually triggered and remains unavailable until a self-hosted runner is registered with `self-hosted`, `Linux`, `ARM64`, and `raspberry-pi` labels. Provision a 64-bit Pi with GitHub Actions runner prerequisites, CMake 3.20 or newer and a C++17 compiler.

Register a repository-scoped Linux ARM64 runner using the repository's **Settings > Actions > Runners > New self-hosted runner** instructions. Verify the runner package against GitHub's published checksum, install it in a private directory outside the project, and add the `raspberry-pi` label when configuring it. Use interactive prompts for the repository URL and short-lived registration token rather than recording credentials in shell history or passing them as command arguments. Runner authentication files remain on the Pi and must be protected by filesystem permissions; never commit them or runner diagnostic logs. Do not install the runner as a service.

To run a Pi check, connect to the Pi interactively, change to the runner installation directory, and start `./run.sh` in the foreground. In GitHub Actions, manually dispatch the **Raspberry Pi** workflow on the default branch; its job skips other branches. Check the run and its uploaded artifacts, then stop the foreground runner with Ctrl+C. A registered runner can execute other matching queued jobs while online: restrict repository administration and workflow changes, and never run untrusted pull-request code on the Pi. The Windows workflow retains its automatic triggers. Neither workflow publishes releases.

# apfloat

A C++17 binary floating-point static library with per-value precision and a portable CMake package. The supported targets are MSVC on Windows and native 64-bit Raspberry Pi OS/Linux with a C++17 compiler. The Raspberry Pi workflow requires a separately provisioned runner.

## Build and test

Install CMake 3.20 or newer and a C++17 compiler, then from a suitable compiler environment:

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release -L "unit|functional" --output-on-failure
ctest --test-dir build -C Release -L performance --output-on-failure
cmake --install build --config Release --prefix install
```

The installed package exports `apfloat::apfloat`; consume it with `find_package(apfloat CONFIG REQUIRED)` and set `CMAKE_PREFIX_PATH` to the installation prefix. Set `BUILD_TESTING=OFF` to build only the library. `tests/consumer` is built in CI against both the installed package and the source tree.

## Consume from another CMake project

The recommended approach for a built or installed dependency is the exported CMake package:

```cmake
find_package(apfloat CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE apfloat::apfloat)
```

Set `CMAKE_PREFIX_PATH` to the install prefix when configuring the consumer. The install is relocatable and includes the public headers, static library, CMake package files, and MIT license.

For an in-tree source dependency, add the project and link the same namespaced target:

```cmake
add_subdirectory(path/to/apfloat EXCLUDE_FROM_ALL)
target_link_libraries(my_app PRIVATE apfloat::apfloat)
```

`FetchContent_MakeAvailable(apfloat)` can be used in place of `add_subdirectory`; pin `GIT_TAG` to a release tag or commit in the consuming project. The project does not yet publish release tags. When included as a subproject, apfloat does not add its own test or benchmark targets to the parent build. When configured as the top-level project, `BUILD_TESTING` controls those targets and defaults to `ON` through CTest.

The numeric implementation is not yet validated for computing millions of digits of pi. Although the precision limit now permits more than ten million decimal digits, large-scale division, multiplication and decimal conversion have not yet been validated at that size.

### Build and test in VS Code

1. Open the repository root as a folder in VS Code and trust the workspace. Install and enable Microsoft's **CMake Tools** (`ms-vscode.cmake-tools`) and **C/C++** (`ms-vscode.cpptools`) extensions in the active VS Code profile. For Remote SSH, install or enable them on the machine where the folder is open.
2. Install a C++ compiler and CMake 3.20 or newer. On Windows, use Visual Studio or Build Tools with the **Desktop development with C++** workload; on Raspberry Pi, use the prerequisites described below.
3. If prompted, select a compiler kit, such as Visual Studio/MSVC on Windows or GCC on the Pi. Otherwise run **CMake: Select a Kit** from the Command Palette, followed by **CMake: Configure**. The project uses `out/vscode` for VS Code builds so they do not conflict with command-line builds.
4. Use **Build** in the status bar or **CMake: Build** from the Command Palette. Use **CMake: Run Tests** or the Testing view to execute CTest tests. The performance test writes `out/vscode/benchmark.csv`; it is included when running all tests.

The project is a static library, not a standalone application. To launch a test executable, use **CMake: Set Launch/Debug Target** and select `apfloat_unit` or `apfloat_functional`, then **CMake: Run Without Debugging** (or **CMake: Debug**). `apfloat_benchmark` requires an output path argument, so run it through CTest instead.

If the controls are missing, check that the status bar is visible (View > Appearance > Status Bar), the folder containing `CMakeLists.txt` is open, and **CMake Tools**, not just a CMake syntax extension, is enabled in the current profile and local/remote environment. Reload the VS Code window after installing or enabling it. The CMake Tools commands in the Command Palette and its Project Outline remain available even if status-bar items are hidden. If configuration fails, open the **CMake/Build** output channel and check the compiler and CMake paths; select a kit again after installing the compiler.

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
    std::cout << apfloat::number::multiply(a, b, apfloat::rounding::nearest).to_string() << '\n'; // 3
}
```

## Numeric and error contract

- Each `number` defaults to positive zero at 128 bits of binary significand precision. `precision()` inspects that precision; `set_precision` accepts 1 through `number::maximum_precision` bits, a representational bound derived from the range of `int`. Available memory can impose a lower practical limit. Reducing precision immediately rounds the stored value to nearest, ties to even. Increasing it preserves the value but cannot recover information already rounded away.
- Binary arithmetic accepts different operand precisions and produces a result at the greater operand precision, including for zero, infinity, and NaN. The operands are not modified; increasing result precision cannot recover information previously rounded out of an operand. Comparisons use the represented values exactly, independently of precision. The legacy `precision_mismatch` exception type remains available but is not thrown for mixed-precision operations.
- Named arithmetic functions `number::add`, `subtract`, `multiply`, and `divide` require an explicit rounding mode. `parse` and `set_precision` default to round-to-nearest, ties-to-even, and also accept a rounding mode. The modes `rounding::downward` and `rounding::upward` round toward negative or positive infinity, respectively. An invalid rounding mode throws `std::invalid_argument`.
- Normalized nonzero finite values have a leading-bit exponent from -16,384 through 16,384, inclusive. There are no subnormal values. Nearest rounding selects zero or the minimum finite value on underflow and signed infinity on overflow. Directed rounding selects the appropriate bounding value: signed zero or the minimum magnitude on underflow, and the maximum finite magnitude or signed infinity on overflow. Exact cancellation rounds to negative zero only in downward mode; zero and infinity special cases retain their documented arithmetic behavior.
- Signed zeros, infinities and NaNs follow IEEE-style arithmetic and unordered NaN comparisons. The library does not claim full IEEE 754 compliance, support status flags, or preserve NaN payloads.
- `number::parse` accepts decimal numbers with an optional sign, decimal point and `e`/`E` exponent, or lowercase `inf`, `infinity`, and `nan`, optionally signed. Whitespace is not accepted. The significand digit count and effective decimal scale are bounded by `number::maximum_precision / 8`; the explicit exponent magnitude is bounded by twice that value. These are representation-safety limits, not guarantees that enough memory is available. `to_string()` emits exact decimal text that round-trips at the same precision; it is not necessarily the shortest representation.
- `parse` and `set_precision` return explicit `error` values for invalid input or limits and leave the destination unchanged on those errors. IEEE-style division by zero and invalid arithmetic yield infinity or NaN, rather than an error result.
- Allocation failures and other unexpected C++ exceptions are not caught by the library. Operating-system faults such as access violations are not portable C++ exceptions and are not handled by the library.

Directed rounding can be used to construct bounds, for example by dividing `1` by `3` once in each direction. Callers must also bound their algorithm's approximation error, propagate both endpoints correctly, and handle NaNs, infinities, and intervals crossing zero; directed operations alone do not certify an entire calculation. Million-digit calculations have not yet been verified.

```cpp
apfloat::number one, three;
if (apfloat::number::parse("1", one) != apfloat::error::none ||
    apfloat::number::parse("3", three) != apfloat::error::none) {
    return 1;
}
auto lower = apfloat::number::divide(one, three, apfloat::rounding::downward);
auto upper = apfloat::number::divide(one, three, apfloat::rounding::upward);
```

## Tests and performance

CTest labels are `unit`, `functional`, and `performance`. The suites check multi-limb and rounded-division fixtures independently calculated with .NET `BigInteger`, and compare deterministic arithmetic with native integer and `double` calculations. The performance test writes `build/benchmark.csv`: each operation and precision has one warm-up and five recorded samples of ten iterations. The parse and arithmetic measurements include a comparison to consume each result. Results include platform, compiler and build configuration. They are informational and have no regression threshold; compare runs only on suitably controlled machines.

For optional large arithmetic checks, configure with `-DAPFLOAT_ENABLE_SCALE_TESTS=ON`, build, and run `ctest --test-dir build -C Release -L scale -V`. The integer test checks every limb of the exact square of a 33,219,424-bit integer and reports elapsed time and peak resident memory; on Linux ARM64 it fails at 8 GiB peak memory. A separate public-API test checks directed division of 1 by 3 and multiplication by 3 at 33,219,413 bits. These checks do not cover general large division or decimal conversion and do not establish full-scale readiness.

## CI

The Windows GitHub Actions workflow builds, runs unit and functional tests and benchmarks, and uploads the installed static library and CSV results. The manually triggered Raspberry Pi workflow also runs the opt-in arithmetic scale checks. It requires a self-hosted runner registered with `self-hosted`, `Linux`, `ARM64`, and `raspberry-pi` labels. Provision a 64-bit Pi with GitHub Actions runner prerequisites, CMake 3.20 or newer and a C++17 compiler.

### Pi setup and test sessions

The scripts require a 64-bit Debian-based Raspberry Pi OS/Linux, an administrator with `sudo`, an interactive terminal, outbound HTTPS access to GitHub, and at least 1 GiB of free space in the home filesystem. If connecting over SSH, verify the host-key fingerprint independently before accepting it. Sync the published repository to an administrator-accessible checkout, then run from that checkout:

```sh
./scripts/pi-create-user.sh
```

The script uses interactive sudo to install missing build and runner packages and create `gitrunr` as a non-root account with a locked password, private home and no supplementary groups or sudo access. It does not configure direct SSH access or install a service. If the account exists, the script verifies its expected properties and preserves it; conflicts and partial homes require manual inspection, not automatic replacement. A locked password and absence of local authorized keys do not override site-wide or centrally configured authentication policies. Review those policies if direct login must be prohibited.

Enter the account using `sudo -iu gitrunr`, then clone or sync the published repository into its private home. Do not copy the administrator's credentials or Git configuration into it. For this public repository, an ordinary HTTPS clone needs no username or password. From the runner account's checkout:

```sh
./scripts/pi-setup.sh
```

Setup runs without sudo; it checks installed dependencies, CMake 3.20 and a working C++17 compiler, then downloads the official Linux ARM64 runner. If packages are missing, have the administrator rerun the provisioning script. The archive is verified against the SHA-256 digest published by GitHub's release API before extraction. Both digest and archive come from GitHub over HTTPS; this is not an independent signature. Setup stops if the digest is unavailable or verification fails.

The runner is installed under `~/.local/share/apfloat/actions-runner`, outside the checkout, with a private parent directory. At the runner's prompts enter the repository URL and a short-lived registration token obtained from **Settings > Actions > Runners > New self-hosted runner**. Do not put credentials in commands, shell history, chat, logs, or project files, and do not record the terminal session. The script supplies the `raspberry-pi` label; confirm the default Linux and ARM64 labels in GitHub Settings. Runner authentication files remain on the Pi, protected by filesystem permissions; do not copy them or runner logs into the repository.

If setup finds a registered runner, it preserves it and asks you to verify its repository and labels in GitHub Settings. An incomplete installation is not overwritten; inspect it and resolve or remove it manually only after confirming what it contains. The script does not install a service.

For each session, connect to the Pi, use `sudo -iu gitrunr` to switch to the runner account, change to its checkout, and run:

```sh
./scripts/pi-session.sh
```

Leave that terminal open, then manually dispatch the **Raspberry Pi** workflow on the default branch in GitHub Actions. Its job skips other branches. Inspect the run and its uploaded artifacts, and press Ctrl+C in the Pi terminal after it finishes to stop the runner. If the job remains queued, verify that the runner is online and has all `self-hosted`, `Linux`, `ARM64`, and `raspberry-pi` labels. For package, network, checksum, or registration errors, resolve the reported problem before retrying; setup will not automatically replace a partial installation.

## License

This project is distributed under the MIT License; see [LICENSE](LICENSE).

GitHub warns that self-hosted runners on public repositories are risky. Any matching queued job can run while the runner is online. Restrict repository and workflow administration, do not run untrusted code, and keep the runner offline when not needed. Windows retains its automatic triggers. Neither workflow publishes releases. See the [design document](docs/design.md) for the numerical architecture and decisions.

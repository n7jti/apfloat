# apfloat design

This document describes the current implementation and its intended contract. Source and tests are authoritative if they differ; update this document when either changes. It is intended for maintainers, users, and future AI-assisted work, not as a roadmap or an assurance of full IEEE 754 compliance.

## Requirements and scope

- Implement arbitrary-precision **binary** floating-point arithmetic in C++17, with a portable CMake build and a static library usable on Windows/MSVC and 64-bit Raspberry Pi OS/Linux.
- Give each value mutable precision, defaulting to 128 significand bits. Support decimal input and output, comparison, and addition, subtraction, multiplication and division.
- Use round-to-nearest, ties-to-even. Support signed zero, infinity and NaN with IEEE-style behavior, without claiming complete IEEE 754 conformance.
- Return explicit errors for expected invalid input and precision. Unlike those errors, operations on mismatched precisions deliberately throw `precision_mismatch`; the library does not catch it.
- Include dependency-free unit, functional and informational performance tests and Windows and native Pi CI. Pi runs are manually dispatched on the default branch and use a manually started self-hosted runner.

## Components and public API

[The public header](../include/apfloat/apfloat.hpp) exposes `apfloat::number`, `apfloat::error`, and `apfloat::precision_mismatch`. The implementation is in [apfloat.cpp](../src/apfloat.cpp). `number` defaults to positive zero at 128 bits; `precision()` inspects precision, `set_precision()` changes it, `parse()` writes to an existing destination using that destination's precision, and `to_string()` returns exact decimal text. Operators implement arithmetic and comparisons; binary results use the common operand precision.

There are no public integer, native floating-point, or string constructors; parsing is the entry point for numeric text. There is no public configurable rounding mode, status flag, NaN payload, or API for direct inspection of the sign or classification. Consumers may compare or format values, subject to the precision rules.

## Value model and limits

Each instance stores precision, a kind (`finite`, `infinity`, or `nan`), a sign, an `int` binary exponent, and an unsigned significand in a vector of 32-bit limbs, least-significant limb first. A finite value represents `(-1)^sign * significand * 2^exponent`. An empty significand represents signed zero. Nonzero finite significands produced by the rounding routine have exactly the selected number of significant bits, including trailing zero bits. A static assertion requires 32-bit `unsigned int`; intermediate limb arithmetic uses 64-bit unsigned integers.

Precision ranges from 1 through 65,536 bits. The highest set bit of a nonzero finite result has a binary exponent in [-16,384, 16,384]. There are no subnormal values: below the minimum, rounding selects signed zero or the minimum normal magnitude, including the tie to zero. Overflow yields signed infinity. These finite limits and the fixed maximum precision bound implementation resources; “arbitrary precision” here means user-selectable precision within those documented bounds, not infinite capacity.

Changing precision immediately rounds a finite value to the new precision. Increasing precision preserves the represented value but does not restore lost bits. Special values retain their kind and sign. Invalid precision is reported and leaves the destination unchanged.

## Algorithms and rationale

The private integer helper implements comparison, shifts, addition, subtraction, multiplication, small-integer operations, and binary long division. Straightforward limb-based arithmetic and standard-width intermediates avoid runtime dependencies and compiler-specific 128-bit types, at the cost of speed for large values.

The shared `rounded` routine receives an exact unsigned numerator, denominator, binary scale, precision and sign. It determines the leading exponent, handles range boundaries, scales to the requested precision, divides to obtain a quotient and remainder, then rounds using twice the remainder and the quotient's low bit. A carry may increment the exponent. Addition and subtraction align significands before combining them; multiplication forms an exact integer product; division forms an exact rational; each uses the same rounding path. Operands of different precision are rejected before special-value handling.

The parser reads decimal digits and an optional decimal exponent into an integer rational, converts it through the same rounding path, and accepts lowercase `inf`, `infinity`, and `nan`, each optionally signed. It rejects whitespace and malformed text. A maximum of 100,000 significand digits and a decimal scale in [-100,000, 100,000] are enforced; further internal bit-size checks also apply. Formatting converts the exact binary value into decimal using shifts or powers of five and emits enough digits to recover the value at the same precision, not necessarily the shortest text. A NaN formats as `nan`; its input sign is not retained in text.

Special arithmetic handles infinities, zero and NaN before finite arithmetic. Positive and negative zero compare equal. NaN compares unequal to every value, including itself, and ordered comparisons involving NaN are false. A zero divided by zero, infinity divided by infinity, zero multiplied by infinity, and opposite infinities added produce NaN; finite nonzero division by signed zero produces signed infinity. See [unit tests](../tests/unit.cpp) for representative cases.

## Error handling and failures

`parse` reports `invalid_syntax` or `out_of_range`; `set_precision` reports `invalid_precision`. Both return `none` on success and leave the destination unchanged on reported errors. Overflow and underflow in numerical operations are represented by infinity or zero/minimum finite as described above, not by error codes. Ordinary IEEE-style invalid operations yield NaN.

Every binary arithmetic or comparison operator requires equal precision, including NaNs and infinities. Mismatch throws `precision_mismatch`, derived from `std::logic_error`; it is not caught inside the library. It terminates a process only if the caller leaves it uncaught. Allocation and other unexpected exceptions also propagate. OS faults such as access violations are not portable C++ exceptions and are not handled by the library.

## Build, tests, and CI

[CMakeLists.txt](../CMakeLists.txt) builds the `apfloat` static library, publishes the `apfloat::apfloat` target, installs headers and CMake package files, and enables CTest through `BUILD_TESTING`. [The consumer example](../tests/consumer/) validates the installed package.

[Unit tests](../tests/unit.cpp) cover rounding, edge cases, special values and errors. [Functional tests](../tests/functional.cpp) check multi-limb fixtures, small integer arithmetic, native-double division comparisons and decimal round trips. [The subprocess check](../tests/uncaught.cmake) checks that uncaught precision mismatch terminates. On Linux, [Pi script tests](../tests/pi-scripts.sh) exercise setup and session behavior with isolated mocked commands. The [benchmark](../tests/benchmark.cpp) records parse, format and arithmetic samples at 64, 128 and 256 bits, including comparison overhead used to consume results. It emits CSV; results are informational, not performance regression gates.

The [Windows workflow](../.github/workflows/windows.yml) runs on pushes, pull requests and manual dispatch. The [Pi workflow](../.github/workflows/raspberry-pi.yml) is manually dispatched, runs its job only on the default branch, requires a Linux ARM64 runner with the `raspberry-pi` label, and uploads package and benchmark artifacts. [Pi scripts](../scripts/) provision a dedicated `gitrunr` account, separate administrator package installation from runner registration, and start the runner in the foreground. The account has a locked password and no sudo privileges; the administrator switches to it interactively. Runner-local state is outside the project. A workflow file or mock test does not establish that a real Pi run has passed; check the actual Actions run for hardware validation.

## Security, limitations, and maintenance

The Pi runner executes code on its host and GitHub warns against self-hosted runners for public repositories. Manual dispatch and default-branch restrictions reduce exposure but do not protect against a malicious authorized workflow change or other matching queued jobs. Restrict access, never run untrusted code, and keep the runner offline when not needed. Credentials and Pi login information must not be stored in this repository; machine-local runner authentication state uses normal protected filesystem permissions.

The design favors portable, simple exact-integer calculations over optimized high-precision algorithms. Large decimal inputs, large precision and long formatted output may require substantial time and memory. Full IEEE 754 interchange formats, subnormals, status flags, selectable rounding, NaN payloads, and optimized multiplication/division are not implemented. Tests provide finite coverage and do not prove all rounding and range cases correct. Keep this document, README and tests synchronized with any future behavior changes.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace apfloat {

/// A nonnegative arbitrary-precision integer.
class integer {
private:
    using word = std::uint32_t;

public:
    integer() = default;
    explicit integer(std::uint32_t n) { if (n) v.push_back(n); }

    bool zero() const noexcept { return v.empty(); }
    int compare(const integer& b) const noexcept {
        if (v.size() != b.v.size()) return v.size() < b.v.size() ? -1 : 1;
        for (std::size_t i = v.size(); i-- > 0;) {
            if (v[i] != b.v[i]) return v[i] < b.v[i] ? -1 : 1;
        }
        return 0;
    }
    void add(const integer& b) {
        v.resize((std::max)(v.size(), b.v.size()), 0);
        std::uint64_t carry = 0;
        for (std::size_t i = 0; i < v.size(); ++i) {
            std::uint64_t n = std::uint64_t(v[i]) + (i < b.v.size() ? b.v[i] : 0) + carry;
            v[i] = word(n);
            carry = n >> 32;
        }
        if (carry) v.push_back(word(carry));
    }
    void add(std::uint32_t n) { add(integer(n)); }

    /// Subtracts b. Throws std::invalid_argument if *this is less than b.
    void subtract(const integer& b) {
        if (compare(b) < 0) throw std::invalid_argument("integer subtraction would be negative");
        std::uint64_t borrow = 0;
        for (std::size_t i = 0; i < v.size(); ++i) {
            std::uint64_t a = v[i];
            std::uint64_t sub = (i < b.v.size() ? std::uint64_t(b.v[i]) : 0) + borrow;
            v[i] = word(a - sub);
            borrow = a < sub;
        }
        trim();
    }
    void multiply(const integer& b);
    void multiply(std::uint32_t b);
    std::string to_string() const;

#if defined(APFLOAT_INTERNAL_INTEGER_ACCESS)
public:
#else
private:
#endif
    explicit integer(const std::vector<word>& n) : v(n) { trim(); }

    void trim() noexcept { while (!v.empty() && v.back() == 0) v.pop_back(); }
    std::size_t bits() const noexcept {
        if (zero()) return 0;
        word n = v.back();
        std::size_t result = (v.size() - 1) * 32;
        while (n) { ++result; n >>= 1; }
        return result;
    }
    integer shifted(std::size_t bits) const {
        if (zero()) return integer();
        integer r;
        const auto whole = bits / 32;
        const auto part = unsigned(bits % 32);
        if (whole > r.v.max_size() - v.size() ||
            (part && whole == r.v.max_size() - v.size()))
            throw std::length_error("integer shift exceeds representable size");
        r.v.resize(v.size() + whole + (part ? 1 : 0), 0);
        std::uint64_t carry = 0;
        for (std::size_t i = 0; i < v.size(); ++i) {
            std::uint64_t n = (std::uint64_t(v[i]) << part) | carry;
            r.v[i + whole] = word(n);
            carry = n >> 32;
        }
        if (part) r.v[v.size() + whole] = word(carry);
        r.trim();
        return r;
    }
    void shift_right(std::size_t bits) noexcept {
        const std::size_t whole = bits / 32;
        const unsigned part = unsigned(bits % 32);
        if (whole >= v.size()) { v.clear(); return; }
        v.erase(v.begin(), v.begin() + whole);
        if (part) {
            word carry = 0;
            for (std::size_t i = v.size(); i-- > 0;) {
                word next = v[i] << (32 - part);
                v[i] = (v[i] >> part) | carry;
                carry = next;
            }
        }
        trim();
    }
    std::size_t trailing_zero_bits() const noexcept {
        std::size_t count = 0;
        for (word n : v) {
            if (n == 0) { count += 32; continue; }
            while ((n & 1) == 0) { ++count; n >>= 1; }
            break;
        }
        return count;
    }
    bool odd() const noexcept { return !zero() && (v[0] & 1); }
    word divide_small(word b) {
        std::uint64_t rem = 0;
        for (std::size_t i = v.size(); i-- > 0;) {
            std::uint64_t n = (rem << 32) | v[i];
            v[i] = word(n / b);
            rem = n % b;
        }
        trim();
        return word(rem);
    }

    std::vector<word> v;
};

}

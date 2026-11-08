#include <apfloat/apfloat.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

namespace apfloat {
namespace {

using word = unsigned int;
using wide = std::uint64_t;
static_assert(std::numeric_limits<word>::digits == 32, "32-bit unsigned int required");

struct integer {
    std::vector<word> v;

    integer() = default;
    explicit integer(word n) { if (n) v.push_back(n); }
    explicit integer(const std::vector<word>& n) : v(n) { trim(); }

    void trim() { while (!v.empty() && v.back() == 0) v.pop_back(); }
    bool zero() const { return v.empty(); }
    std::size_t bits() const {
        if (zero()) return 0;
        word n = v.back();
        std::size_t result = (v.size() - 1) * 32;
        while (n) { ++result; n >>= 1; }
        return result;
    }
    int compare(const integer& b) const {
        if (v.size() != b.v.size()) return v.size() < b.v.size() ? -1 : 1;
        for (std::size_t i = v.size(); i-- > 0;) {
            if (v[i] != b.v[i]) return v[i] < b.v[i] ? -1 : 1;
        }
        return 0;
    }
    void add(const integer& b) {
        v.resize((std::max)(v.size(), b.v.size()), 0);
        wide carry = 0;
        for (std::size_t i = 0; i < v.size(); ++i) {
            wide n = wide(v[i]) + (i < b.v.size() ? b.v[i] : 0) + carry;
            v[i] = word(n);
            carry = n >> 32;
        }
        if (carry) v.push_back(word(carry));
    }
    void subtract(const integer& b) {
        wide borrow = 0;
        for (std::size_t i = 0; i < v.size(); ++i) {
            wide a = v[i];
            wide sub = (i < b.v.size() ? wide(b.v[i]) : 0) + borrow;
            v[i] = word(a - sub);
            borrow = a < sub;
        }
        trim();
    }
    integer shifted(std::size_t bits) const {
        if (zero()) return integer();
        integer r;
        const auto whole = bits / 32;
        const auto part = unsigned(bits % 32);
        r.v.resize(v.size() + whole + (part ? 1 : 0), 0);
        wide carry = 0;
        for (std::size_t i = 0; i < v.size(); ++i) {
            wide n = (wide(v[i]) << part) | carry;
            r.v[i + whole] = word(n);
            carry = n >> 32;
        }
        if (part) r.v[v.size() + whole] = word(carry);
        r.trim();
        return r;
    }
    void shift_right(std::size_t bits) {
        std::size_t whole = bits / 32;
        unsigned part = unsigned(bits % 32);
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
    std::size_t trailing_zero_bits() const {
        std::size_t count = 0;
        for (word n : v) {
            if (n == 0) { count += 32; continue; }
            while ((n & 1) == 0) { ++count; n >>= 1; }
            break;
        }
        return count;
    }
    bool odd() const { return !zero() && (v[0] & 1); }
    word divide_small(word b) {
        wide rem = 0;
        for (std::size_t i = v.size(); i-- > 0;) {
            wide n = (rem << 32) | v[i];
            v[i] = word(n / b);
            rem = n % b;
        }
        trim();
        return word(rem);
    }
};

integer multiply_schoolbook(const integer& a, const integer& b) {
    integer r;
    if (a.zero() || b.zero()) return r;
    r.v.resize(a.v.size() + b.v.size(), 0);
    for (std::size_t i = 0; i < a.v.size(); ++i) {
        wide carry = 0;
        for (std::size_t j = 0; j < b.v.size(); ++j) {
            wide n = wide(a.v[i]) * b.v[j] + r.v[i + j] + carry;
            r.v[i + j] = word(n);
            carry = n >> 32;
        }
        r.v[i + b.v.size()] = word(carry);
    }
    r.trim();
    return r;
}

integer limb_slice(const integer& value, std::size_t begin, std::size_t end) {
    integer result;
    end = (std::min)(end, value.v.size());
    if (begin < end) {
        result.v.assign(value.v.begin() + begin, value.v.begin() + end);
        result.trim();
    }
    return result;
}

void add_at(integer& target, const integer& value, std::size_t offset) {
    if (value.zero()) return;
    if (target.v.size() < offset + value.v.size()) target.v.resize(offset + value.v.size(), 0);
    wide carry = 0;
    std::size_t i = 0;
    for (; i < value.v.size(); ++i) {
        wide sum = wide(target.v[offset + i]) + value.v[i] + carry;
        target.v[offset + i] = word(sum);
        carry = sum >> 32;
    }
    while (carry) {
        if (offset + i == target.v.size()) target.v.push_back(0);
        wide sum = wide(target.v[offset + i]) + carry;
        target.v[offset + i] = word(sum);
        carry = sum >> 32;
        ++i;
    }
}

word modular_power(word value, wide exponent, word modulus) {
    wide result = 1;
    wide factor = value;
    while (exponent) {
        if (exponent & 1) result = result * factor % modulus;
        factor = factor * factor % modulus;
        exponent >>= 1;
    }
    return word(result);
}

void transform(std::vector<word>& values, word modulus, bool inverse) {
    const std::size_t n = values.size();
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        while (j & bit) { j ^= bit; bit >>= 1; }
        j ^= bit;
        if (i < j) std::swap(values[i], values[j]);
    }
    for (std::size_t length = 2; length <= n; length *= 2) {
        word root = modular_power(3, (modulus - 1) / length, modulus);
        if (inverse) root = modular_power(root, modulus - 2, modulus);
        for (std::size_t start = 0; start < n; start += length) {
            wide factor = 1;
            for (std::size_t i = 0; i < length / 2; ++i) {
                word left = values[start + i];
                word right = word(wide(values[start + i + length / 2]) * factor % modulus);
                word sum = left + right;
                values[start + i] = sum >= modulus ? sum - modulus : sum;
                values[start + i + length / 2] =
                    left >= right ? left - right : left + modulus - right;
                factor = factor * root % modulus;
            }
        }
    }
    if (inverse) {
        word scale = modular_power(word(n), modulus - 2, modulus);
        for (word& value : values) value = word(wide(value) * scale % modulus);
    }
}

void convolution(const integer& a, const integer& b, word modulus,
                 std::vector<word>& left, std::vector<word>& right) {
    std::fill(left.begin(), left.end(), 0);
    std::fill(right.begin(), right.end(), 0);
    for (std::size_t i = 0; i < a.v.size(); ++i) {
        left[2 * i] = a.v[i] & 0xffffu;
        left[2 * i + 1] = a.v[i] >> 16;
    }
    for (std::size_t i = 0; i < b.v.size(); ++i) {
        right[2 * i] = b.v[i] & 0xffffu;
        right[2 * i + 1] = b.v[i] >> 16;
    }
    transform(left, modulus, false);
    transform(right, modulus, false);
    for (std::size_t i = 0; i < left.size(); ++i)
        left[i] = word(wide(left[i]) * right[i] % modulus);
    transform(left, modulus, true);
}

integer multiply_transform(const integer& a, const integer& b, std::size_t size) {
    constexpr word p1 = 998244353;
    constexpr word p2 = 469762049;
    const word inverse = modular_power(p1 % p2, p2 - 2, p2);
    std::vector<word> left(size), right(size);
    convolution(a, b, p1, left, right);
    std::vector<word> first = std::move(left);
    left.resize(size);
    convolution(a, b, p2, left, right);

    integer result;
    result.v.resize(a.v.size() + b.v.size(), 0);
    wide carry = 0;
    for (std::size_t i = 0; i < result.v.size() * 2; ++i) {
        word r1 = first[i] % p2;
        word difference = left[i] >= r1 ? left[i] - r1 : left[i] + p2 - r1;
        wide coefficient = first[i] + wide(p1) * (wide(difference) * inverse % p2);
        wide digit = coefficient + carry;
        carry = digit >> 16;
        result.v[i / 2] |= word(digit & 0xffffu) << (16 * (i % 2));
    }
    if (carry != 0) throw std::logic_error("integer multiplication carry overflow");
    result.trim();
    return result;
}

integer multiply_integers(const integer& a, const integer& b) {
    constexpr std::size_t threshold = 32;
    const std::size_t smaller = (std::min)(a.v.size(), b.v.size());
    const std::size_t larger = (std::max)(a.v.size(), b.v.size());
    if (smaller < threshold) return multiply_schoolbook(a, b);
    if (smaller < larger / 2) {
        const integer& short_operand = a.v.size() < b.v.size() ? a : b;
        const integer& long_operand = a.v.size() < b.v.size() ? b : a;
        integer result;
        for (std::size_t offset = 0; offset < larger;) {
            const std::size_t count = (std::min)(smaller, larger - offset);
            integer part = limb_slice(long_operand, offset, offset + count);
            add_at(result, multiply_integers(short_operand, part), offset);
            offset += count;
        }
        return result;
    }

    // Both moduli support 2^23 roots; 16-bit convolution coefficients fit their product.
    constexpr std::size_t max_transform = std::size_t(1) << 23;
    if (smaller >= 512 && a.v.size() <= max_transform / 2 &&
        b.v.size() <= max_transform / 2 - a.v.size()) {
        std::size_t size = 1;
        const std::size_t needed = 2 * (a.v.size() + b.v.size());
        while (size < needed) size *= 2;
        return multiply_transform(a, b, size);
    }

    const std::size_t split = larger / 2;
    integer a0 = limb_slice(a, 0, split);
    integer a1 = limb_slice(a, split, a.v.size());
    integer b0 = limb_slice(b, 0, split);
    integer b1 = limb_slice(b, split, b.v.size());
    integer low = multiply_integers(a0, b0);
    integer high = multiply_integers(a1, b1);
    a0.add(a1);
    b0.add(b1);
    integer middle = multiply_integers(a0, b0);
    middle.subtract(low);
    middle.subtract(high);
    integer result;
    add_at(result, low, 0);
    add_at(result, middle, split);
    add_at(result, high, split * 2);
    result.trim();
    return result;
}

std::pair<integer, integer> divide_base(integer a, const integer& b) {
    integer q;
    if (a.compare(b) < 0) return {q, a};
    if (b.v.size() == 1 && b.v[0] == 1) return {std::move(a), q};
    std::size_t common_zeros = (std::min)(a.trailing_zero_bits(), b.trailing_zero_bits());
    if (common_zeros != 0) {
        a.shift_right(common_zeros);
        integer divisor = b;
        divisor.shift_right(common_zeros);
        auto result = divide_base(std::move(a), divisor);
        result.second = result.second.shifted(common_zeros);
        return result;
    }
    std::size_t divisor_bits = b.bits();
    if (b.trailing_zero_bits() == divisor_bits - 1) {
        std::size_t shift = divisor_bits - 1;
        q = a;
        q.shift_right(shift);
        a.subtract(q.shifted(shift));
        return {std::move(q), std::move(a)};
    }
    if (b.v.size() == 1) {
        word remainder = a.divide_small(b.v[0]);
        return {std::move(a), integer(remainder)};
    }

    // Normalize the divisor so its most significant bit bounds each quotient estimate.
    constexpr wide base = wide(1) << 32;
    const unsigned normalization = static_cast<unsigned>(b.v.size() * 32 - divisor_bits);
    const std::size_t n = b.v.size();
    const std::size_t m = a.v.size() - n;
    integer divisor = b.shifted(normalization);
    integer dividend = a.shifted(normalization);
    dividend.v.resize(a.v.size() + 1, 0);
    q.v.resize(m + 1, 0);

    for (std::size_t j = m + 1; j-- > 0;) {
        wide top = (wide(dividend.v[j + n]) << 32) | dividend.v[j + n - 1];
        wide estimate = top / divisor.v[n - 1];
        wide remainder = top % divisor.v[n - 1];
        if (estimate >= base) {
            estimate = base - 1;
            remainder = top - estimate * divisor.v[n - 1];
        }
        while (remainder < base &&
               estimate * divisor.v[n - 2] > (remainder << 32) + dividend.v[j + n - 2]) {
            --estimate;
            remainder += divisor.v[n - 1];
        }

        wide borrow = 0;
        for (std::size_t i = 0; i < n; ++i) {
            wide product = estimate * divisor.v[i] + borrow;
            word low = word(product);
            word previous = dividend.v[j + i];
            dividend.v[j + i] = previous - low;
            borrow = (product >> 32) + (previous < low ? 1 : 0);
        }
        word high = dividend.v[j + n];
        dividend.v[j + n] = word(wide(high) - borrow);
        if (wide(high) < borrow) {
            --estimate;
            wide carry = 0;
            for (std::size_t i = 0; i < n; ++i) {
                wide sum = wide(dividend.v[j + i]) + divisor.v[i] + carry;
                dividend.v[j + i] = word(sum);
                carry = sum >> 32;
            }
            dividend.v[j + n] += word(carry);
        }
        q.v[j] = word(estimate);
    }

    q.trim();
    dividend.v.resize(n);
    dividend.shift_right(normalization);
    dividend.trim();
    return {std::move(q), std::move(dividend)};
}

// Divide a 2n-limb dividend by a normalized n-limb divisor, with a < b*B^n.
std::pair<integer, integer> divide_2n_1n(const integer& a, const integer& b,
                                        std::size_t n);

// Divide a 3n-limb dividend by a normalized 2n-limb divisor, with a < b*B^n.
std::pair<integer, integer> divide_3n_2n(const integer& a, const integer& b,
                                        std::size_t n) {
    integer upper = limb_slice(a, n, a.v.size());
    integer high_divisor = limb_slice(b, n, 2 * n);
    integer top = limb_slice(a, 2 * n, a.v.size());
    integer q, remainder;
    if (top.compare(high_divisor) < 0) {
        auto result = divide_2n_1n(upper, high_divisor, n);
        q = std::move(result.first);
        remainder = std::move(result.second);
    } else {
        q.v.assign(n, (std::numeric_limits<word>::max)());
        remainder = std::move(upper);
        remainder.subtract(high_divisor.shifted(n * 32));
        remainder.add(high_divisor);
    }

    integer combined = remainder.shifted(n * 32);
    add_at(combined, limb_slice(a, 0, n), 0);
    integer product = multiply_integers(q, limb_slice(b, 0, n));
    // The high-half estimate cannot be too small; correct any overestimate.
    while (combined.compare(product) < 0) {
        q.subtract(integer(1));
        combined.add(b);
    }
    combined.subtract(product);
    return {std::move(q), std::move(combined)};
}

std::pair<integer, integer> divide_2n_1n(const integer& a, const integer& b,
                                        std::size_t n) {
    constexpr std::size_t basecase_limbs = 64;
    if (a.compare(b) < 0) return {integer(), a};
    if (n <= basecase_limbs || a.v.size() - b.v.size() < basecase_limbs)
        return divide_base(a, b);
    const std::size_t half = n / 2;
    integer upper = limb_slice(a, half, a.v.size());
    auto high = divide_3n_2n(upper, b, half);
    integer lower = high.second.shifted(half * 32);
    add_at(lower, limb_slice(a, 0, half), 0);
    auto low = divide_3n_2n(lower, b, half);
    add_at(low.first, high.first, half);
    return {std::move(low.first), std::move(low.second)};
}

std::pair<integer, integer> divide_integers(integer a, const integer& b) {
    constexpr std::size_t basecase_limbs = 64;
    if (a.compare(b) < 0 || b.v.size() <= basecase_limbs ||
        a.v.size() - b.v.size() < basecase_limbs)
        return divide_base(std::move(a), b);

    const std::size_t common_zeros =
        (std::min)(a.trailing_zero_bits(), b.trailing_zero_bits());
    if (common_zeros != 0) {
        a.shift_right(common_zeros);
        integer divisor = b;
        divisor.shift_right(common_zeros);
        auto result = divide_integers(std::move(a), divisor);
        result.second = result.second.shifted(common_zeros);
        return result;
    }
    if (b.trailing_zero_bits() == b.bits() - 1)
        return divide_base(std::move(a), b);

    std::size_t n = 1;
    while (n < b.v.size()) n *= 2;
    const unsigned normalization = static_cast<unsigned>(b.v.size() * 32 - b.bits());
    const std::size_t shift = (n - b.v.size()) * 32 + normalization;
    integer divisor = b.shifted(shift);
    integer dividend = a.shifted(shift);
    integer q, remainder;
    const std::size_t blocks = (dividend.v.size() - 1) / n + 1;
    for (std::size_t block = blocks; block-- > 0;) {
        integer current = remainder.shifted(n * 32);
        add_at(current, limb_slice(dividend, block * n, (block + 1) * n), 0);
        auto result = divide_2n_1n(current, divisor, n);
        add_at(q, result.first, block * n);
        remainder = std::move(result.second);
    }
    remainder.shift_right(shift);
    return {std::move(q), std::move(remainder)};
}

integer power(word base, std::size_t exponent) {
    integer result(1), factor(base);
    while (exponent != 0) {
        if (exponent & 1) result = multiply_integers(result, factor);
        exponent >>= 1;
        if (exponent) factor = multiply_integers(factor, factor);
    }
    return result;
}

std::vector<integer> decimal_powers(std::size_t groups) {
    std::vector<integer> powers;
    if (groups == 0) return powers;
    powers.emplace_back(1000000000);
    for (std::size_t width = 1; width <= groups / 2; width *= 2)
        powers.push_back(multiply_integers(powers.back(), powers.back()));
    return powers;
}

integer from_decimal_groups(const std::vector<word>& groups, std::size_t begin,
                            std::size_t count, const std::vector<integer>& powers) {
    if (count == 0) return integer();
    if (count == 1) return integer(groups[begin]);
    std::size_t low_count = 1;
    std::size_t power_index = 0;
    while (low_count <= (count - 1) / 2) {
        low_count *= 2;
        ++power_index;
    }
    integer high = from_decimal_groups(groups, begin, count - low_count, powers);
    integer low = from_decimal_groups(groups, begin + count - low_count, low_count, powers);
    integer result = multiply_integers(high, powers[power_index]);
    result.add(low);
    return result;
}

void append_decimal(const integer& n, std::size_t minimum_digits,
                    const std::vector<integer>& powers, std::string& output) {
    if (n.v.size() <= 1) {
        std::string digits = std::to_string(n.zero() ? 0 : n.v[0]);
        if (digits.size() < minimum_digits) output.append(minimum_digits - digits.size(), '0');
        output += digits;
        return;
    }
    std::size_t index = powers.size();
    while (index > 0 && n.compare(powers[index - 1]) < 0) --index;
    if (index == 0) {
        std::string digits = std::to_string(n.v[0]);
        if (digits.size() < minimum_digits) output.append(minimum_digits - digits.size(), '0');
        output += digits;
        return;
    }
    --index;
    const std::size_t lower_digits = std::size_t(9) << index;
    auto parts = divide_integers(n, powers[index]);
    append_decimal(parts.first, minimum_digits > lower_digits ? minimum_digits - lower_digits : 0,
                   powers, output);
    append_decimal(parts.second, lower_digits, powers, output);
}

std::string decimal(const integer& n) {
    if (n.zero()) return "0";
    const auto powers = decimal_powers(n.bits() / 29 + 1);
    std::string result;
    append_decimal(n, 0, powers, result);
    return result;
}

}

precision_mismatch::precision_mismatch()
    : std::logic_error("apfloat precision mismatch") {}

struct number::implementation {
    static constexpr int min_exponent = -16384;
    static constexpr int max_exponent = 16384;

    static void check(rounding mode) {
        if (mode != rounding::nearest && mode != rounding::downward && mode != rounding::upward)
            throw std::invalid_argument("invalid apfloat rounding mode");
    }
    static bool away_from_zero(rounding mode, bool negative) {
        return (mode == rounding::upward && !negative) || (mode == rounding::downward && negative);
    }
    static number special(std::size_t p, kind k, bool negative = false) {
        number r;
        r.precision_ = p;
        r.kind_ = k;
        r.negative_ = negative;
        return r;
    }
    static number overflow(std::size_t p, bool negative, rounding mode) {
        if (mode == rounding::nearest || away_from_zero(mode, negative))
            return special(p, kind::infinity, negative);
        number r = special(p, kind::finite, negative);
        integer mantissa = integer(1).shifted(p);
        mantissa.subtract(integer(1));
        r.limbs_ = std::move(mantissa.v);
        r.exponent_ = max_exponent - static_cast<int>(p - 1);
        return r;
    }
    static number rounded(integer n, integer d, int shift, std::size_t p, bool negative, rounding mode) {
        number r = special(p, kind::finite, negative);
        if (n.zero()) return r;
        int k = static_cast<int>(n.bits()) - static_cast<int>(d.bits());
        int cmp = k >= 0 ? n.compare(d.shifted(static_cast<std::size_t>(k)))
                         : n.shifted(static_cast<std::size_t>(-k)).compare(d);
        if (cmp < 0) --k;
        int top = k + shift;
        if (top < min_exponent) {
            bool round_up = away_from_zero(mode, negative);
            if (mode == rounding::nearest && top == min_exponent - 1) {
                integer lhs = n.shifted(static_cast<std::size_t>((std::max)(0, -k)));
                integer rhs = d.shifted(static_cast<std::size_t>((std::max)(0, k)));
                round_up = lhs.compare(rhs) > 0;
            }
            if (round_up) {
                integer one(1);
                r.limbs_ = one.shifted(p - 1).v;
                r.exponent_ = min_exponent - static_cast<int>(p - 1);
            }
            return r;
        }
        if (top > max_exponent) return overflow(p, negative, mode);
        int adjust = static_cast<int>(p - 1) - k;
        if (adjust >= 0) n = n.shifted(static_cast<std::size_t>(adjust));
        else d = d.shifted(static_cast<std::size_t>(-adjust));
        auto qr = divide_integers(std::move(n), d);
        if (mode == rounding::nearest) {
            int comparison = qr.second.shifted(1).compare(d);
            if (comparison > 0 || (comparison == 0 && qr.first.odd())) qr.first.add(integer(1));
        } else if (!qr.second.zero() && away_from_zero(mode, negative)) {
            qr.first.add(integer(1));
        }
        if (qr.first.bits() > p) {
            qr.first.shift_right(1);
            ++top;
        }
        if (top > max_exponent) return overflow(p, negative, mode);
        r.limbs_ = std::move(qr.first.v);
        r.exponent_ = top - static_cast<int>(p - 1);
        return r;
    }
    static number add(const number& a, const number& b, bool subtract, rounding mode) {
        const std::size_t precision = (std::max)(a.precision_, b.precision_);
        bool bs = b.negative_ != subtract;
        if (a.kind_ == kind::nan || b.kind_ == kind::nan) return special(precision, kind::nan);
        if (a.kind_ == kind::infinity && b.kind_ == kind::infinity && a.negative_ != bs)
            return special(precision, kind::nan);
        if (a.kind_ == kind::infinity) return special(precision, kind::infinity, a.negative_);
        if (b.kind_ == kind::infinity) return special(precision, kind::infinity, bs);
        integer x(a.limbs_), y(b.limbs_);
        if (x.zero() && y.zero())
            return special(precision, kind::finite,
                           (a.negative_ && bs) || (a.negative_ != bs && mode == rounding::downward));
        int exp = x.zero() ? b.exponent_ : y.zero() ? a.exponent_ : (std::min)(a.exponent_, b.exponent_);
        if (!x.zero()) x = x.shifted(static_cast<std::size_t>(a.exponent_ - exp));
        if (!y.zero()) y = y.shifted(static_cast<std::size_t>(b.exponent_ - exp));
        bool sign = a.negative_;
        if (sign == bs) x.add(y);
        else if (x.compare(y) >= 0) x.subtract(y);
        else { y.subtract(x); x = std::move(y); sign = bs; }
        if (x.zero()) sign = mode == rounding::downward;
        return rounded(std::move(x), integer(1), exp, precision, sign, mode);
    }
    static number muldiv(const number& a, const number& b, bool division, rounding mode) {
        const std::size_t precision = (std::max)(a.precision_, b.precision_);
        bool sign = a.negative_ != b.negative_;
        if (a.kind_ == kind::nan || b.kind_ == kind::nan) return special(precision, kind::nan);
        bool az = a.kind_ == kind::finite && a.limbs_.empty();
        bool bz = b.kind_ == kind::finite && b.limbs_.empty();
        if (division) {
            if ((az && bz) || (a.kind_ == kind::infinity && b.kind_ == kind::infinity))
                return special(precision, kind::nan);
            if (a.kind_ == kind::infinity || bz) return special(precision, kind::infinity, sign);
            if (b.kind_ == kind::infinity || az) return special(precision, kind::finite, sign);
            return rounded(integer(a.limbs_), integer(b.limbs_), a.exponent_ - b.exponent_, precision, sign, mode);
        }
        if ((az && b.kind_ == kind::infinity) || (bz && a.kind_ == kind::infinity))
            return special(precision, kind::nan);
        if (a.kind_ == kind::infinity || b.kind_ == kind::infinity)
            return special(precision, kind::infinity, sign);
        return rounded(multiply_integers(integer(a.limbs_), integer(b.limbs_)), integer(1),
                       a.exponent_ + b.exponent_, precision, sign, mode);
    }
    static int magnitude(const number& a, const number& b) {
        if (a.kind_ == kind::infinity) return b.kind_ == kind::infinity ? 0 : 1;
        if (b.kind_ == kind::infinity) return -1;
        if (a.limbs_.empty() || b.limbs_.empty())
            return a.limbs_.empty() ? (b.limbs_.empty() ? 0 : -1) : 1;
        const std::int64_t top_a = std::int64_t(a.exponent_) + std::int64_t(a.precision_) - 1;
        const std::int64_t top_b = std::int64_t(b.exponent_) + std::int64_t(b.precision_) - 1;
        if (top_a != top_b) return top_a < top_b ? -1 : 1;
        const int base = (std::min)(a.exponent_, b.exponent_);
        const auto limb_at = [](const number& value, std::size_t index, std::size_t shift) {
            const std::size_t whole = shift / 32;
            const unsigned part = static_cast<unsigned>(shift % 32);
            if (index < whole) return word(0);
            const std::size_t source = index - whole;
            word result = source < value.limbs_.size() ? word(value.limbs_[source] << part) : 0;
            if (part != 0 && source != 0 && source - 1 < value.limbs_.size())
                result |= value.limbs_[source - 1] >> (32 - part);
            return result;
        };
        const std::size_t shift_a = static_cast<std::size_t>(a.exponent_ - base);
        const std::size_t shift_b = static_cast<std::size_t>(b.exponent_ - base);
        const std::size_t count = static_cast<std::size_t>(top_a - base) / 32 + 1;
        for (std::size_t i = count; i-- > 0;) {
            word x = limb_at(a, i, shift_a), y = limb_at(b, i, shift_b);
            if (x != y) return x < y ? -1 : 1;
        }
        return 0;
    }
};

error number::set_precision(std::size_t precision) {
    return set_precision(precision, rounding::nearest);
}

error number::set_precision(std::size_t precision, rounding mode) {
    implementation::check(mode);
    if (precision == 0 || precision > maximum_precision) return error::invalid_precision;
    if (precision == precision_) return error::none;
    if (kind_ == kind::finite) {
        *this = implementation::rounded(integer(limbs_), integer(1), exponent_, precision, negative_, mode);
    } else {
        precision_ = precision;
    }
    return error::none;
}

error number::parse(std::string_view text, number& result) {
    return parse(text, result, rounding::nearest);
}

error number::parse(std::string_view text, number& result, rounding mode) {
    implementation::check(mode);
    // Bound decimal intermediates so their binary sizes remain representable as int.
    constexpr int decimal_limit = static_cast<int>(maximum_precision / 8);
    if (text.empty()) return error::invalid_syntax;
    std::size_t pos = 0;
    bool negative = false;
    if (text[pos] == '+' || text[pos] == '-') { negative = text[pos] == '-'; ++pos; }
    if (pos == text.size()) return error::invalid_syntax;
    auto rest = text.substr(pos);
    if (rest == "inf" || rest == "infinity" || rest == "nan") {
        result = implementation::special(result.precision_, rest == "nan" ? kind::nan : kind::infinity, negative);
        return error::none;
    }
    std::string significand;
    int fractional = 0;
    std::size_t digits = 0;
    bool point = false, digit = false;
    while (pos < text.size()) {
        char c = text[pos];
        if (c >= '0' && c <= '9') {
            digit = true;
            if (++digits > static_cast<std::size_t>(decimal_limit)) return error::out_of_range;
            if (point) ++fractional;
            significand.push_back(c);
        } else if (c == '.' && !point) {
            point = true;
        } else break;
        ++pos;
    }
    if (!digit) return error::invalid_syntax;
    int exp = 0;
    bool expneg = false;
    if (pos < text.size() && (text[pos] == 'e' || text[pos] == 'E')) {
        ++pos;
        if (pos < text.size() && (text[pos] == '+' || text[pos] == '-')) {
            expneg = text[pos] == '-';
            ++pos;
        }
        std::size_t start = pos;
        while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
            int next = text[pos++] - '0';
            constexpr int exponent_limit = decimal_limit * 2;
            if (exp > (exponent_limit - next) / 10) return error::out_of_range;
            exp = exp * 10 + next;
        }
        if (pos == start) return error::invalid_syntax;
    }
    if (pos != text.size()) return error::invalid_syntax;
    exp = (expneg ? -exp : exp) - fractional;
    if (exp < -decimal_limit || exp > decimal_limit) return error::out_of_range;
    if (significand.find_first_not_of('0') == std::string::npos) {
        result = implementation::special(result.precision_, kind::finite, negative);
        return error::none;
    }
    std::vector<word> groups;
    groups.reserve((significand.size() + 8) / 9);
    std::size_t group_size = significand.size() % 9;
    if (group_size == 0) group_size = 9;
    for (std::size_t i = 0; i < significand.size();) {
        word group = 0;
        for (std::size_t end = i + group_size; i < end; ++i)
            group = group * 10 + static_cast<word>(significand[i] - '0');
        groups.push_back(group);
        group_size = 9;
    }
    const auto powers = decimal_powers(groups.size());
    integer n = from_decimal_groups(groups, 0, groups.size(), powers);
    integer d(1);
    if (exp >= 0) n = multiply_integers(n, power(10, static_cast<std::size_t>(exp)));
    else d = power(10, static_cast<std::size_t>(-exp));
    result = implementation::rounded(std::move(n), std::move(d), 0, result.precision_, negative, mode);
    return error::none;
}

std::string number::to_string() const {
    if (kind_ == kind::nan) return "nan";
    if (kind_ == kind::infinity) return negative_ ? "-inf" : "inf";
    integer n(limbs_);
    if (n.zero()) return negative_ ? "-0" : "0";
    int exp = exponent_;
    if (exp < 0) {
        std::size_t cancel = (std::min)(n.trailing_zero_bits(), static_cast<std::size_t>(-exp));
        n.shift_right(cancel);
        exp += static_cast<int>(cancel);
    }
    if (exp >= 0) n = n.shifted(static_cast<std::size_t>(exp));
    else n = multiply_integers(n, power(5, static_cast<std::size_t>(-exp)));
    std::string s = decimal(n);
    if (exp < 0) {
        std::size_t places = static_cast<std::size_t>(-exp);
        if (s.size() <= places) s.insert(0, places + 1 - s.size(), '0');
        s.insert(s.size() - places, 1, '.');
        while (s.back() == '0') s.pop_back();
        if (s.back() == '.') s.pop_back();
    }
    if (negative_) s.insert(s.begin(), '-');
    return s;
}

number number::add(const number& a, const number& b, rounding mode) {
    implementation::check(mode);
    return implementation::add(a, b, false, mode);
}
number number::subtract(const number& a, const number& b, rounding mode) {
    implementation::check(mode);
    return implementation::add(a, b, true, mode);
}
number number::multiply(const number& a, const number& b, rounding mode) {
    implementation::check(mode);
    return implementation::muldiv(a, b, false, mode);
}
number number::divide(const number& a, const number& b, rounding mode) {
    implementation::check(mode);
    return implementation::muldiv(a, b, true, mode);
}
bool operator==(const number& a, const number& b) {
    if (a.kind_ == number::kind::nan || b.kind_ == number::kind::nan) return false;
    if (a.kind_ == number::kind::finite && b.kind_ == number::kind::finite && a.limbs_.empty() && b.limbs_.empty()) return true;
    return a.negative_ == b.negative_ && number::implementation::magnitude(a, b) == 0;
}
bool operator<(const number& a, const number& b) {
    if (a.kind_ == number::kind::nan || b.kind_ == number::kind::nan) return false;
    if (a.kind_ == number::kind::finite && b.kind_ == number::kind::finite && a.limbs_.empty() && b.limbs_.empty()) return false;
    if (a.negative_ != b.negative_) return a.negative_;
    int cmp = number::implementation::magnitude(a, b);
    return a.negative_ ? cmp > 0 : cmp < 0;
}

}

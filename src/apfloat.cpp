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
    void shift_right_one() {
        word carry = 0;
        for (std::size_t i = v.size(); i-- > 0;) {
            word next = v[i] & 1;
            v[i] = (v[i] >> 1) | (carry << 31);
            carry = next;
        }
        trim();
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
    void set_bit(std::size_t bit) {
        if (v.size() <= bit / 32) v.resize(bit / 32 + 1, 0);
        v[bit / 32] |= word(1) << (bit % 32);
    }
    bool odd() const { return !zero() && (v[0] & 1); }
    void multiply_small(word b) {
        wide carry = 0;
        for (auto& x : v) {
            wide n = wide(x) * b + carry;
            x = word(n);
            carry = n >> 32;
        }
        if (carry) v.push_back(word(carry));
        trim();
    }
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

integer multiply(const integer& a, const integer& b) {
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

std::pair<integer, integer> divide(integer a, const integer& b) {
    integer q;
    if (a.compare(b) < 0) return {q, a};
    if (b.v.size() == 1 && b.v[0] == 1) return {std::move(a), q};
    std::size_t shift = a.bits() - b.bits();
    integer divisor = b.shifted(shift);
    for (std::size_t i = shift + 1; i-- > 0;) {
        if (a.compare(divisor) >= 0) { a.subtract(divisor); q.set_bit(i); }
        divisor.shift_right_one();
    }
    return {q, a};
}

std::string decimal(integer n) {
    if (n.zero()) return "0";
    std::vector<word> groups;
    while (!n.zero()) groups.push_back(n.divide_small(1000000000));
    std::string s = std::to_string(groups.back());
    for (std::size_t i = groups.size() - 1; i-- > 0;) {
        std::string group = std::to_string(groups[i]);
        s.append(9 - group.size(), '0');
        s += group;
    }
    return s;
}

}

precision_mismatch::precision_mismatch()
    : std::logic_error("apfloat precision mismatch") {}

struct number::implementation {
    static constexpr int min_exponent = -16384;
    static constexpr int max_exponent = 16384;

    static void check(const number& a, const number& b) {
        if (a.precision_ != b.precision_) throw precision_mismatch();
    }
    static number special(std::size_t p, kind k, bool negative = false) {
        number r;
        r.precision_ = p;
        r.kind_ = k;
        r.negative_ = negative;
        return r;
    }
    static number rounded(integer n, integer d, int shift, std::size_t p, bool negative) {
        number r = special(p, kind::finite, negative);
        if (n.zero()) return r;
        int k = static_cast<int>(n.bits()) - static_cast<int>(d.bits());
        int cmp = k >= 0 ? n.compare(d.shifted(static_cast<std::size_t>(k)))
                         : n.shifted(static_cast<std::size_t>(-k)).compare(d);
        if (cmp < 0) --k;
        int top = k + shift;
        if (top < min_exponent) {
            if (top == min_exponent - 1) {
                integer lhs = n.shifted(static_cast<std::size_t>((std::max)(0, -k)));
                integer rhs = d.shifted(static_cast<std::size_t>((std::max)(0, k)));
                if (lhs.compare(rhs) > 0) {
                    integer one(1);
                    r.limbs_ = one.shifted(p - 1).v;
                    r.exponent_ = min_exponent - static_cast<int>(p - 1);
                }
            }
            return r;
        }
        if (top > max_exponent) return special(p, kind::infinity, negative);
        int adjust = static_cast<int>(p - 1) - k;
        if (adjust >= 0) n = n.shifted(static_cast<std::size_t>(adjust));
        else d = d.shifted(static_cast<std::size_t>(-adjust));
        auto qr = divide(std::move(n), d);
        int rounding = qr.second.shifted(1).compare(d);
        if (rounding > 0 || (rounding == 0 && qr.first.odd())) qr.first.add(integer(1));
        if (qr.first.bits() > p) {
            qr.first.shift_right_one();
            ++top;
        }
        if (top > max_exponent) return special(p, kind::infinity, negative);
        r.limbs_ = std::move(qr.first.v);
        r.exponent_ = top - static_cast<int>(p - 1);
        return r;
    }
    static number add(const number& a, const number& b, bool subtract) {
        check(a, b);
        bool bs = b.negative_ != subtract;
        if (a.kind_ == kind::nan || b.kind_ == kind::nan) return special(a.precision_, kind::nan);
        if (a.kind_ == kind::infinity && b.kind_ == kind::infinity && a.negative_ != bs)
            return special(a.precision_, kind::nan);
        if (a.kind_ == kind::infinity) return a;
        if (b.kind_ == kind::infinity) return special(a.precision_, kind::infinity, bs);
        integer x(a.limbs_), y(b.limbs_);
        if (x.zero() && y.zero()) return special(a.precision_, kind::finite, a.negative_ && bs);
        int exp = x.zero() ? b.exponent_ : y.zero() ? a.exponent_ : (std::min)(a.exponent_, b.exponent_);
        if (!x.zero()) x = x.shifted(static_cast<std::size_t>(a.exponent_ - exp));
        if (!y.zero()) y = y.shifted(static_cast<std::size_t>(b.exponent_ - exp));
        bool sign = a.negative_;
        if (sign == bs) x.add(y);
        else if (x.compare(y) >= 0) x.subtract(y);
        else { y.subtract(x); x = std::move(y); sign = bs; }
        if (x.zero()) sign = false;
        return rounded(std::move(x), integer(1), exp, a.precision_, sign);
    }
    static number muldiv(const number& a, const number& b, bool division) {
        check(a, b);
        bool sign = a.negative_ != b.negative_;
        if (a.kind_ == kind::nan || b.kind_ == kind::nan) return special(a.precision_, kind::nan);
        bool az = a.kind_ == kind::finite && a.limbs_.empty();
        bool bz = b.kind_ == kind::finite && b.limbs_.empty();
        if (division) {
            if ((az && bz) || (a.kind_ == kind::infinity && b.kind_ == kind::infinity))
                return special(a.precision_, kind::nan);
            if (a.kind_ == kind::infinity || bz) return special(a.precision_, kind::infinity, sign);
            if (b.kind_ == kind::infinity || az) return special(a.precision_, kind::finite, sign);
            return rounded(integer(a.limbs_), integer(b.limbs_), a.exponent_ - b.exponent_, a.precision_, sign);
        }
        if ((az && b.kind_ == kind::infinity) || (bz && a.kind_ == kind::infinity))
            return special(a.precision_, kind::nan);
        if (a.kind_ == kind::infinity || b.kind_ == kind::infinity)
            return special(a.precision_, kind::infinity, sign);
        return rounded(multiply(integer(a.limbs_), integer(b.limbs_)), integer(1),
                       a.exponent_ + b.exponent_, a.precision_, sign);
    }
    static int magnitude(const number& a, const number& b) {
        if (a.kind_ == kind::infinity) return b.kind_ == kind::infinity ? 0 : 1;
        if (b.kind_ == kind::infinity) return -1;
        integer x(a.limbs_), y(b.limbs_);
        if (x.zero() || y.zero()) return x.zero() ? (y.zero() ? 0 : -1) : 1;
        if (a.exponent_ != b.exponent_) return a.exponent_ < b.exponent_ ? -1 : 1;
        return x.compare(y);
    }
};

error number::set_precision(std::size_t precision) {
    if (precision == 0 || precision > maximum_precision) return error::invalid_precision;
    if (precision == precision_) return error::none;
    if (kind_ == kind::finite) {
        *this = implementation::rounded(integer(limbs_), integer(1), exponent_, precision, negative_);
    } else {
        precision_ = precision;
    }
    return error::none;
}

error number::parse(std::string_view text, number& result) {
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
    integer n;
    int fractional = 0;
    std::size_t digits = 0;
    bool point = false, digit = false;
    while (pos < text.size()) {
        char c = text[pos];
        if (c >= '0' && c <= '9') {
            digit = true;
            if (++digits > 100000) return error::out_of_range;
            if (point) ++fractional;
            n.multiply_small(10);
            n.add(integer(static_cast<word>(c - '0')));
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
            if (exp > 100000) return error::out_of_range;
            exp = exp * 10 + (text[pos++] - '0');
        }
        if (pos == start) return error::invalid_syntax;
    }
    if (pos != text.size()) return error::invalid_syntax;
    exp = (expneg ? -exp : exp) - fractional;
    if (exp < -100000 || exp > 100000 || n.bits() > 400000) return error::out_of_range;
    integer d(1);
    if (exp >= 0) for (int i = 0; i < exp; ++i) n.multiply_small(10);
    else for (int i = 0; i < -exp; ++i) d.multiply_small(10);
    result = implementation::rounded(std::move(n), std::move(d), 0, result.precision_, negative);
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
    else for (int i = 0; i < -exp; ++i) n.multiply_small(5);
    std::string s = decimal(std::move(n));
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

number operator+(const number& a, const number& b) { return number::implementation::add(a, b, false); }
number operator-(const number& a, const number& b) { return number::implementation::add(a, b, true); }
number operator*(const number& a, const number& b) { return number::implementation::muldiv(a, b, false); }
number operator/(const number& a, const number& b) { return number::implementation::muldiv(a, b, true); }
bool operator==(const number& a, const number& b) {
    number::implementation::check(a, b);
    if (a.kind_ == number::kind::nan || b.kind_ == number::kind::nan) return false;
    if (a.kind_ == number::kind::finite && b.kind_ == number::kind::finite && a.limbs_.empty() && b.limbs_.empty()) return true;
    return a.negative_ == b.negative_ && number::implementation::magnitude(a, b) == 0;
}
bool operator<(const number& a, const number& b) {
    number::implementation::check(a, b);
    if (a.kind_ == number::kind::nan || b.kind_ == number::kind::nan) return false;
    if (a.kind_ == number::kind::finite && b.kind_ == number::kind::finite && a.limbs_.empty() && b.limbs_.empty()) return false;
    if (a.negative_ != b.negative_) return a.negative_;
    int cmp = number::implementation::magnitude(a, b);
    return a.negative_ ? cmp > 0 : cmp < 0;
}

}

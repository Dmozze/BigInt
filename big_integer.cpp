#include "big_integer.h"

#include <string>
#include <stdexcept>
#include <algorithm>
#include <tuple>
#include <cmath>


big_integer::big_integer() : data(), sign(false) {
}

big_integer::big_integer(big_integer const &other) = default;

big_integer::big_integer(int a) : data(1), sign(a < 0) {
    data[0] = static_cast<uint32_t>(a);
    shrink_to_fit();
}


big_integer::big_integer(const std::string &s) : big_integer() {

    for (size_t i = 0; i < s.size(); i++) {
        if ((s[i] == '+' || s[i] == '-') && i == 0) continue;
        if (s[i] < '0' || '9' < s[i]) {
            throw std::runtime_error("invalid string");
        }
        (*this) = (*this).mul_by_uint32_t(10) + static_cast<uint32_t>(s[i] - '0');
    }
    if (!s.empty() && s[0] == '-') {
        (*this) = -(*this);
    }
    shrink_to_fit();
}

big_integer::~big_integer() = default;

big_integer &big_integer::operator=(big_integer const &other) = default;


big_integer &big_integer::operator+=(big_integer const &rhs) {
    data.resize(std::max(data.size(), rhs.data.size()) + 1, empty_block());
    uint64_t carry = 0;
    for (size_t i = 0; i < data.size(); i++) {
        carry = carry + data[i] + (i < rhs.data.size() ? rhs.data[i] : rhs.empty_block());
        data[i] = static_cast<uint32_t>(carry);
        carry >>= BIT_DEPTH;
    }
    set_sign();
    shrink_to_fit();
    return *this;
}

big_integer &big_integer::operator-=(big_integer const &rhs) {
    return (*this) += (-rhs);
}

big_integer square_mul(big_integer const &left, big_integer const &right) {
    big_integer result;
    result.data.resize(left.data.size() + right.data.size(), 0);
    big_integer temp;
    uint64_t carry = 0;
    for (size_t i = 0; i < right.data.size(); i++) {
        temp = left.mul_by_uint32_t(right.data[i]);
        for (size_t j = 0; j + i < result.data.size(); j++) {
            if (carry == 0 && j >= temp.data.size()) break;
            carry = carry + static_cast<uint64_t>(result.data[i + j]) + (j < temp.data.size() ? temp.data[j] : 0);
            result.data[i + j] = static_cast<uint32_t>(carry);
            carry >>= big_integer::BIT_DEPTH;
        }
    }
    result.shrink_to_fit();
    return result;
}

big_integer copy(big_integer const &left, size_t l, size_t r) {
    if (l >= r){
        return big_integer();
    }
    big_integer temp;
    temp.data.resize(r - l, 0);
    for (size_t i = l; i < r; i++) {
        temp.data[i - l] = left.data[i];
    }
    return temp;
}


big_integer big_integer::Karatsuba_mul(big_integer const &left, big_integer const &right) {
    if (left.data.empty() || right.data.empty()) {
        return 0;
    }
    if (left.data.size() == 1) {
        return right.mul_by_uint32_t(left.data.back());
    }
    if (right.data.size() == 1) {
        return left.mul_by_uint32_t(right.data.back());
    }
    if (left.data.size()  < 16 ||  right.data.size() < 16) {
        return square_mul(left, right);
    }
    size_t n = std::max(left.data.size(), right.data.size());
    size_t ndiv2 = n / 2;
    big_integer left_l = copy(left, ndiv2, left.data.size());
    big_integer left_r = copy(left, 0, std::min(ndiv2, left.data.size()));
    big_integer right_l = copy(right, ndiv2, right.data.size());
    big_integer right_r = copy(right, 0, std::min(ndiv2, right.data.size()));

    big_integer product_1 = Karatsuba_mul(left_l, right_l);
    big_integer product_2 = Karatsuba_mul(left_r, right_r);
    big_integer product_3 = Karatsuba_mul(left_l + left_r, right_l + right_r);

    return (product_1 << (BIT_DEPTH * 2 * ndiv2)) + ((product_3 - product_1 - product_2) << (BIT_DEPTH * ndiv2)) + product_2;
}


big_integer &big_integer::operator*=(big_integer const &rhs) {
    big_integer left = abs(*this);
    big_integer right = abs(rhs);
    big_integer &result = *this;
    bool result_sign = sign ^ rhs.sign;
    result = Karatsuba_mul(left, right);
    if (result_sign) {
        result = -result;
    }
    return result;
}

big_integer &big_integer::operator/=(big_integer const &rhs) {
    if (rhs == 0) {
        throw std::runtime_error("division by zero");
    }
    bool result_sign = sign ^ rhs.sign;
    big_integer rhs_abs = abs(rhs), &this_abs = *this;
    this_abs = abs(this_abs);
    if (rhs_abs > this_abs) {
        return this_abs = 0;
    }
    if (rhs_abs.data.size() == 1) {
        this_abs = this_abs.div_by_uint32_t(rhs_abs.data.back()).first;
        if (result_sign) {
            this_abs = -this_abs;
        }
        return this_abs;
    }

    size_t n = rhs_abs.data.size(), m = this_abs.data.size();
    uint64_t f = BASE / static_cast<uint64_t>(rhs_abs.data.back() + 1);
    big_integer r = this_abs.mul_by_uint32_t(static_cast<uint32_t>(f));
    big_integer d = rhs_abs.mul_by_uint32_t(static_cast<uint32_t>(f));
    big_integer result;

    result.data.resize(m - n + 1, empty_block());

    for (ptrdiff_t k = m - n; k >= 0; k--) {
        r.data.resize(m + n + 1, empty_block());
        unsigned __int128 dividend = r.data[n + k];
        dividend <<= BIT_DEPTH;
        dividend += r.data[n + k - 1];
        dividend <<= BIT_DEPTH;
        dividend += r.data[n + k - 2];

        uint64_t divisor = ((static_cast<uint64_t>(1) * d.data[n - 1]) << BIT_DEPTH) + d.data[n - 2];
        uint32_t qt = dividend / divisor > BASE - 1 ? BASE - 1 : dividend / divisor;
        big_integer temp = d.mul_by_uint32_t(qt);
        big_integer dq;
        dq.data.resize(temp.data.size() + k, 0);
        for (size_t index = 0; index < temp.data.size(); index++) {
            dq.data[index + k] = temp.data[index];
        }
        if (r < dq) {
            qt--;
            dq = (d.mul_by_uint32_t(qt) << (BIT_DEPTH * k));
        }
        result.data[k] = static_cast<uint32_t>(qt);
        r -= dq;
    }


    if (result_sign) {
        result = -result;
    }
    result.shrink_to_fit();
    return *this = result;
}

big_integer &big_integer::operator%=(big_integer const &rhs) {
    return *this = *this - (*this / rhs) * rhs;
}

big_integer &big_integer::common_fun_bits(big_integer const &rhs, const std::function<uint32_t (uint32_t, uint32_t)> &fn) {
    data.resize(std::max(data.size(), rhs.data.size()) + 1, empty_block());
    for (size_t i = 0; i < data.size(); i++) {
        data[i] = fn(data[i], i < rhs.data.size() ? rhs.data[i] : rhs.empty_block());
    }
    set_sign();
    shrink_to_fit();
    return *this;
}

big_integer &big_integer::operator&=(big_integer const &rhs) {
    return common_fun_bits(rhs, [](uint32_t a, uint32_t b) -> uint32_t { return a & b; });
}

big_integer &big_integer::operator|=(big_integer const &rhs) {
    return common_fun_bits(rhs, [](uint32_t a, uint32_t b) -> uint32_t { return a | b; });
}

big_integer &big_integer::operator^=(big_integer const &rhs) {
    return common_fun_bits(rhs, [](uint32_t a, uint32_t b) -> uint32_t { return a ^ b; });
}

big_integer &big_integer::operator<<=(int rhs) {
    if (rhs == 0) {
        return *this;
    }
    size_t shiftBlocks = rhs / BIT_DEPTH, shiftBits = rhs % BIT_DEPTH;
    data.resize(data.size() + shiftBlocks + (shiftBits > 0), empty_block());
    uint32_t current_digit, previous_digit;
    for (ptrdiff_t i = data.size() - 1; i >= 0; --i) {
        if (static_cast<uint32_t>(i) >= shiftBlocks) {
            current_digit = data[i - shiftBlocks];
            previous_digit = static_cast<size_t>(i) >= shiftBlocks + 1 ? data[i - shiftBlocks - 1] : 0;
        } else {
            current_digit = 0;
            previous_digit = 0;
        }
        data[i] = (current_digit << shiftBits) + (shiftBits == 0 ? 0 : previous_digit >> (BIT_DEPTH - shiftBits));
    }
    shrink_to_fit();
    return *this;
}

big_integer &big_integer::operator>>=(int rhs) {
    if (rhs == 0) {
        return *this;
    }
    size_t shiftBlocks = rhs / BIT_DEPTH, shiftBits = rhs % BIT_DEPTH;
    uint32_t current_digit, next_digit;
    for (size_t i = 0; i < data.size(); ++i) {
        if (i + shiftBlocks < data.size()) {
            current_digit = data[i + shiftBlocks];
            next_digit = i + shiftBlocks + 1 < data.size() ? data[i + shiftBlocks + 1] : empty_block();
        } else {
            current_digit = empty_block();
            next_digit = empty_block();
        }
        data[i] = (current_digit >> shiftBits) +  (shiftBits == 0 ? 0 : next_digit << (BIT_DEPTH - shiftBits));
    }
    shrink_to_fit();
    return *this;
}

big_integer big_integer::operator+() const {
    return *this;
}

big_integer big_integer::operator-() const {
    big_integer r = ~*this + 1;
    return r;
}

big_integer big_integer::operator~() const {
    big_integer r = *this;
    for (size_t i = 0; i < data.size(); i++) {
        r.data[i] = ~r.data[i];
    }
    r.sign = !r.sign;
    r.shrink_to_fit();
    return r;
}


big_integer &big_integer::operator++() {
    return *this = *this + 1;
}

const big_integer big_integer::operator++(int) {
    big_integer r = *this;
    *this += 1;
    return r;
}

big_integer &big_integer::operator--() {
    return *this = *this - 1;
}

const big_integer big_integer::operator--(int) {
    big_integer r = *this;
    *this -= 1;
    return r;
}

big_integer operator+(big_integer a, big_integer const &b) {
    return a += b;
}

big_integer operator-(big_integer a, big_integer const &b) {
    return a -= b;
}

big_integer operator*(big_integer a, big_integer const &b) {
    return a *= b;
}

big_integer operator/(big_integer a, big_integer const &b) {
    return a /= b;
}

big_integer operator%(big_integer a, big_integer const &b) {
    return a %= b;
}

big_integer operator&(big_integer a, big_integer const &b) {
    return a &= b;
}

big_integer operator|(big_integer a, big_integer const &b) {
    return a |= b;
}

big_integer operator^(big_integer a, big_integer const &b) {
    return a ^= b;
}

big_integer operator<<(big_integer a, int b) {
    return a <<= b;
}

big_integer operator>>(big_integer a, int b) {
    return a >>= b;
}

int32_t comparator(big_integer const &a, big_integer const &b) {
    if (a.sign != b.sign) {
        return a.sign ? -1 : 1;
    }
    for (size_t index = std::max(a.data.size(), b.data.size()); index + 1 > index; index--) {
        uint32_t a_value = index < a.data.size() ? a.data[index] : a.empty_block(),
                 b_value = index < b.data.size() ? b.data[index] : b.empty_block();
        if (a_value != b_value) {
            return a_value > b_value ? 1 : -1;
        }
    }
    return 0;
}

bool operator==(big_integer const &a, big_integer const &b) {
    return comparator(a, b) == 0;
}

bool operator<(big_integer const &a, big_integer const &b) {
    return comparator(a, b) < 0;
}

bool operator!=(big_integer const &a, big_integer const &b) {
    return comparator(a, b) != 0;
}

bool operator>(big_integer const &a, big_integer const &b) {
    return comparator(a, b) > 0;
}

bool operator<=(big_integer const &a, big_integer const &b) {
    return comparator(a, b) <= 0;
}

bool operator>=(big_integer const &a, big_integer const &b) {
    return comparator(a, b) >= 0;
}

std::string to_string(const big_integer &rhs) {
    if (rhs == 0) {
        return "0";
    }
    big_integer x = abs(rhs);
    std::string res;
    while (x > 0) {
        std::pair<big_integer, uint32_t> div_mod_10 = x.div_by_uint32_t(10);
        uint32_t cur = div_mod_10.second;
        res.push_back(static_cast<char>(cur + '0'));
        x = div_mod_10.first;
    }
    if (rhs.sign) {
        res.push_back('-');
    }
    reverse(res.begin(), res.end());
    return res;
}

std::ostream &operator<<(std::ostream &s, big_integer const &a) {
    return s << to_string(a);
}


big_integer abs(big_integer const &a) {
    return a.sign ? -a : a;
}


uint32_t big_integer::empty_block() const {
    return sign ? UINT32_MAX : 0;
}


void big_integer::set_sign() { // in some cases shrink_to_fit is used without set_sign
    sign = data.back() >> 31;
}

std::pair<big_integer, uint32_t> big_integer::div_by_uint32_t(uint32_t const rhs) const {
    if (rhs == 0) {
        throw std::runtime_error("division by zero");
    }
    big_integer res;
    res.data.resize(data.size(), 0);
    uint64_t temp = 0;
    for (ptrdiff_t i = res.data.size() - 1; i >= 0; i--) {
        temp = (temp << BIT_DEPTH) + data[i];
        res.data[i] = static_cast<uint32_t>(temp / rhs);
        temp %= rhs;
    }
    res.shrink_to_fit();
    return std::make_pair(res, temp);
}

big_integer big_integer::mul_by_uint32_t(uint32_t const rhs) const {
    big_integer res;
    uint64_t carry = 0;
    res.data.resize(data.size() + 1, 0);
    for (size_t i = 0; i < data.size(); i++) {
        carry = static_cast<uint64_t>(data[i]) * rhs + carry;
        res.data[i] = static_cast<uint32_t>(carry);
        carry >>= BIT_DEPTH;
    }
    if (carry > 0) {
        res.data[data.size()] = carry;
    }
    res.shrink_to_fit();
    return res;
}

void big_integer::shrink_to_fit() {
    while (!data.empty() && data.back() == empty_block()) {
        data.pop_back();
    }
}

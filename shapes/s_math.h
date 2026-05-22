#ifndef S_MATH_H
#define S_MATH_H

#include <cmath>
#include <limits>
#include <numeric>
#include <type_traits>

#include "s_def.h"

namespace S_Shape2D {

constexpr double pi = 3.141592653589793238462643383279;
constexpr double piX2 = pi * 2;
constexpr double pi_2 = pi / 2;

class Math {
public:
    template <class Type>
    static Type correctAngle(Type angle)
    {
        return std::fmod(pi + std::remainder(angle - pi, piX2), piX2);
    }

    template <class Type>
    static Type multiply(Type lhs, Type rhs)
    {
        return lhs * rhs;
    }

    template <class Type>
    static Type cos(Type value)
    {
        return std::cos(value);
    }

    template <class Type>
    static Type sin(Type value)
    {
        return std::sin(value);
    }

    template <class Type>
    static Type atan(Type value)
    {
        return std::atan(value);
    }

    template <class Type>
    static Type atan2(Type y, Type x)
    {
        return std::atan2(y, x);
    }
};

template <class T>
struct FastGCD {
    typename std::enable_if<std::is_integral<T>::value, T>::type
    operator()(T lhs, T rhs) const
    {
        return std::gcd(lhs, rhs);
    }
};

struct flag_floating {
};

struct flag_integral {
};

template <class T1, class GCD = FastGCD<T1>, class T2 = T1>
class Rational {
    using T = typename std::enable_if<std::is_integral<T1>::value, T1>::type;
    using TD = typename std::enable_if<std::is_integral<T2>::value, T2>::type;

public:
    Rational() = default;

    template <typename ValueT, typename = typename std::enable_if<std::is_integral<ValueT>::value>::type>
    Rational(ValueT value, flag_floating = {})
        : num(static_cast<T>(value))
    {
    }

    template <typename ValueT, typename = typename std::enable_if<std::is_floating_point<ValueT>::value>::type>
    Rational(ValueT value, flag_integral = {})
        : num(T((value + std::numeric_limits<ValueT>::epsilon()) * std::pow(10, std::numeric_limits<ValueT>::digits10)))
        , den(TD(std::pow(10, std::numeric_limits<ValueT>::digits10)))
    {
        normsign();
        normalize();
    }

    explicit Rational(T numerator, T denominator)
        : num(numerator)
        , den(denominator)
    {
        normsign();
        normalize();
    }

    operator double() const
    {
        return static_cast<double>(num) / static_cast<double>(den);
    }

    bool operator>(const Rational& other) const
    {
        return TD(other.den) * num > TD(den) * other.num;
    }

    bool operator<(const Rational& other) const
    {
        return TD(other.den) * num < TD(den) * other.num;
    }

    bool operator==(const Rational& other) const
    {
        return TD(other.den) * num == TD(den) * other.num;
    }

    bool operator!=(const Rational& other) const
    {
        return !(*this == other);
    }

    bool operator<=(const Rational& other) const
    {
        return !(*this > other);
    }

    bool operator>=(const Rational& other) const
    {
        return !(*this < other);
    }

    bool operator<(const T& value) const { return TD(num) < TD(value) * den; }
    bool operator>(const T& value) const { return TD(num) > TD(value) * den; }
    bool operator<=(const T& value) const { return TD(num) <= TD(value) * den; }
    bool operator>=(const T& value) const { return TD(num) >= TD(value) * den; }

    Rational& operator*=(const Rational& other)
    {
        num *= other.num;
        den *= other.den;
        normsign();
        normalize();
        return *this;
    }

    Rational& operator/=(const Rational& other)
    {
        num *= other.den;
        den *= other.num;
        normsign();
        normalize();
        return *this;
    }

    Rational& operator+=(const Rational& other)
    {
        num = other.den * num + other.num * den;
        den *= other.den;
        normalize();
        return *this;
    }

    Rational& operator-=(const Rational& other)
    {
        num = other.den * num - other.num * den;
        den *= other.den;
        normalize();
        return *this;
    }

    Rational& operator*=(const T& value)
    {
        const T gcd = GCD()(value, den);
        num *= value / gcd;
        den /= gcd;
        normsign();
        return *this;
    }

    Rational& operator/=(const T& value)
    {
        if (num == T {})
            return *this;

        const T gcd = GCD()(num, value);
        num /= gcd;
        den *= value / gcd;
        normsign();
        return *this;
    }

    Rational& operator+=(const T& value)
    {
        num += value * den;
        return *this;
    }

    Rational& operator-=(const T& value)
    {
        num -= value * den;
        return *this;
    }

    Rational operator*(const Rational& value) const
    {
        auto tmp = *this;
        tmp *= value;
        return tmp;
    }

    Rational operator/(const Rational& value) const
    {
        auto tmp = *this;
        tmp /= value;
        return tmp;
    }

    Rational operator+(const Rational& value) const
    {
        auto tmp = *this;
        tmp += value;
        return tmp;
    }

    Rational operator-(const Rational& value) const
    {
        auto tmp = *this;
        tmp -= value;
        return tmp;
    }

    Rational operator-() const
    {
        auto tmp = *this;
        tmp.num = -num;
        return tmp;
    }

    T numerator() const { return num; }
    T denominator() const { return den; }

private:
    void normsign()
    {
        if (den < 0) {
            den = -den;
            num = -num;
        }
    }

    void normalize()
    {
        const T gcd = GCD()(num, den);
        if (gcd == T {})
            return;

        num /= gcd;
        den /= gcd;
    }

private:
    T num = 0;
    T den = 1;
};

using Rational64 = Rational<__int64>;

}

USE_S_(Rational64);

#endif // S_MATH_H

#ifndef MATH_H
#define MATH_H

#include <cmath>
#include <limits>
#include <type_traits>

#include "s_common.hpp"
#include "s_def.h"

#include <QDebug>

namespace S_Shape2D {

constexpr double pi = 3.141592653589793238462643383279; // pi
constexpr double piX2 = pi * 2; //2*PI
constexpr double pi_2 = pi / 2; //2*PI

class Math {
public:
    template<class Type>
    inline static Type correctAngle(Type a)
    {
        return fmod(pi + remainder(a - pi, piX2), piX2);
    }

    template<class Type>
    inline static Type multiply(Type a, Type b)
    {
        return a*b;
    }

    template<class Type>
    inline static Type cos(Type a)
    {
        return std::cos(a);
    }

    template<class Type>
    inline static Type sin(Type a)
    {
        return std::sin(a);
    }

    template<class Type>
    inline static Type atan(Type a)
    {
        return std::atan(a);
    }

    template<class Type>
    inline static Type atan2(Type x, Type y)
    {
        return std::atan2(x, y);
    }
};

template <class T>
struct FastGCD {
    typename std::enable_if<std::is_integral<T>::value, T>::type
    operator()(T x, T y)
    {
        x = abs(x);
        y = abs(y);
        T deep = 1;
        while (y != 0) {
            if (x < y) {
                x ^= y;
                y ^= x;
                x ^= y;
                continue;
            }

            if ((x & 0x01) == 0) {
                if ((y & 0x01) == 0) {
                    y >>= 1;
                    x >>= 1;
                    deep <<= 1;
                } else {
                    x >>= 1;
                }
            } else {
                if ((y & 0x01) == 0) {
                    y >>= 1;

                } else {
                    T tmp = x - y;
                    x = y;
                    y = tmp;
                }
            }
        }
        return deep * x;
    }
};

// A very simple representation of an unnormalized rational number.
// The sign of the denominator is still normalized to be always positive.

struct flag_floating {
};
struct flag_integral {
};

template <class T1, class GCD = FastGCD<T1>, class T2 = T1>
class Rational {

    using T = typename std::enable_if<std::is_integral<T1>::value, T1>::type;
    using TD = typename std::enable_if<std::is_integral<T2>::value, T2>::type;
    T num;
    T den = 1;

    inline void normsign()
    {
        if (den < 0) {
            den = -den;
            num = -num;
        }
    }
    inline void normalize()
    {
        T n = GCD()(num, den);
        num /= n;
        den /= n;
    }

public:
    inline Rational()
        : num(T(0))
        , den(T(1))
    {
    }

    template <typename ValueT, typename = typename std::enable_if<std::is_integral<ValueT>::value>::type>
    inline Rational(ValueT d, flag_floating = {})
        : num((T)d)
        , den(1)
    {
    }

    template <typename ValueT, typename = typename std::enable_if<std::is_floating_point<ValueT>::value>::type>
    inline Rational(ValueT d, flag_integral = {})
        : num(T((d + std::numeric_limits<ValueT>::epsilon()) * pow(10, std::numeric_limits<ValueT>::digits10)))
        , den(TD(pow(10, std::numeric_limits<ValueT>::digits10)))
    {
        normsign();
        normalize();
    }

    inline explicit Rational(T n, T d)
        : num(n)
        , den(d)
    {
        normsign();
        normalize();
    }

    operator double() const
    {
        return ((double)num) / (double)den;
    }

    inline bool operator>(const Rational& o) const
    {
        return TD(o.den) * num > TD(den) * o.num;
    }

    inline bool operator<(const Rational& o) const
    {
        return TD(o.den) * num < TD(den) * o.num;
    }

    inline bool operator==(const Rational& o) const
    {
        return TD(o.den) * num == TD(den) * o.num;
    }

    inline bool operator!=(const Rational& o) const { return !(*this == o); }

    inline bool operator<=(const Rational& o) const
    {
        return TD(o.den) * num <= TD(den) * o.num;
    }

    inline bool operator>=(const Rational& o) const
    {
        return TD(o.den) * num >= TD(den) * o.num;
    }

    inline bool operator<(const T& v) const { return TD(num) < TD(v) * den; }
    inline bool operator>(const T& v) const { return TD(num) > TD(v) * den; }
    inline bool operator<=(const T& v) const { return TD(num) <= TD(v) * den; }
    inline bool operator>=(const T& v) const { return TD(num) >= TD(v) * den; }

    inline Rational& operator*=(const Rational& o)
    {
        num *= o.num;
        den *= o.den;
        normsign();
        normalize();
        return *this;
    }

    inline Rational& operator/=(const Rational& o)
    {
        num *= o.den;
        den *= o.num;
        normsign();
        normalize();
        return *this;
    }

    inline Rational& operator+=(const Rational& o)
    {
        num = o.den * num + o.num * den;
        den *= o.den;
        normalize();
        return *this;
    }

    inline Rational& operator-=(const Rational& o)
    {
        num = o.den * num - o.num * den;
        den *= o.den;
        normalize();
        return *this;
    }

    inline Rational& operator*=(const T& v)
    {
        const T gcd = GCD()(v, den);
        num *= v / gcd;
        den /= gcd;
        return *this;
    }

    inline Rational& operator/=(const T& v)
    {
        if (num == T {})
            return *this;

        // Avoid overflow and preserve normalization
        const T gcd = GCD()(num, v);
        num /= gcd;
        den *= v / gcd;

        if (den < T {}) {
            num = -num;
            den = -den;
        }

        den *= v;
        return *this;
    }

    inline Rational& operator+=(const T& v)
    {
        num += v * den;
        return *this;
    }
    inline Rational& operator-=(const T& v)
    {
        num -= v * den;
        return *this;
    }

    inline Rational operator*(const Rational& v) const
    {
        auto tmp = *this;
        tmp *= v;
        return tmp;
    }
    inline Rational operator/(const Rational& v) const
    {
        auto tmp = *this;
        tmp /= v;
        return tmp;
    }
    inline Rational operator+(const Rational& v) const
    {
        auto tmp = *this;
        tmp += v;
        return tmp;
    }
    inline Rational operator-(const Rational& v) const
    {
        auto tmp = *this;
        tmp -= v;
        return tmp;
    }
    inline Rational operator-() const
    {
        auto tmp = *this;
        tmp.num = -num;
        return tmp;
    }

    inline T numerator() const { return num; }
    inline T denominator() const { return den; }
};

using Rational64 = Rational<__int64>;
}

USE_S_(Rational64);

#endif // MATH_H

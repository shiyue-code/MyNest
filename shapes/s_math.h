#ifndef MATH_H
#define MATH_H

#include <cmath>
#include <limits>
#include <type_traits>

#include "s_common.hpp"
#include "s_def.h"

#include <QDebug>

namespace S_Shape2D {

constexpr double pi = 3.14159265358979323846; // pi
constexpr double piX2 = pi * 2; //2*PI
constexpr double pi_2 = pi / 2; //2*PI

class Math {
public:
    static double correctAngle(double a)
    {
        return fmod(pi + remainder(a - pi, piX2), piX2);
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

    template <typename ValueT, typename = typename std::enable_if<std::is_integral<ValueT>::value>::ty
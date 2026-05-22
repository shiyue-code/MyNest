#ifndef S_COMMON_H
#define S_COMMON_H

#include <cmath>
#include <type_traits>

namespace S_Shape2D {

template <typename T>
using rm_cv_t = typename std::remove_cv<T>::type;

template <typename T>
using rm_ref_t = typename std::remove_reference<T>::type;

template <typename T>
using rm_cvref_t = rm_cv_t<rm_ref_t<T>>;

template <typename T>
inline T eps(T)
{
    return static_cast<T>(0.000001);
}

template <>
inline double eps<double>(double)
{
    return 0.0000001;
}

template <typename T>
typename std::enable_if<!std::is_arithmetic<rm_cvref_t<T>>::value, bool>::type
isEqual(const T& lhs, const T& rhs)
{
    return lhs == rhs;
}

template <typename T>
typename std::enable_if<std::is_integral<rm_cvref_t<T>>::value, bool>::type
isEqual(T lhs, T rhs)
{
    return lhs == rhs;
}

template <typename T>
typename std::enable_if<std::is_floating_point<rm_cvref_t<T>>::value, bool>::type
isEqual(T lhs, T rhs)
{
    return std::abs(lhs - rhs) < eps(lhs);
}

}

#endif // S_COMMON_H

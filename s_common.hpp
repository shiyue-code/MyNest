#ifndef S_COMMON_H
#define S_COMMON_H

#include <cfloat>
#include <cmath>
#include <limits>
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
    return 0.000001;
}

template <>
inline double eps<double>(double)
{
    return 0.000000001;
}

//非基础数据类型使用该模板
template <typename T>
typename std::enable_if<!std::is_arithmetic<rm_cvref_t<T>>::value, bool>::type
isEqual(const T& v1, const T& v2)
{
    return v1 == v2;
}

//整形使用该模板
template <typename T>
typename std::enable_if<std::is_integral<rm_cvref_t<T>>::value, bool>::type
isEqual(T v1, T v2)
{
    return v1 == v2;
}

//浮点型使用该模板
template <typename T>
typename std::enable_if<std::is_floating_point<rm_cvref_t<T>>::value, bool>::type
isEqual(T v1, T v2)
{
    return abs(v1 - v2) < eps(v1);
}

template <class T>
struct hasAt {
    // We test if the type has serialize using decltype and declval.
    template <typename C>
    static constexpr decltype(std::declval<C>().at(0), true) test(int /* unused */)
    {
        // We can return values, thanks to constexpr instead of playing with sizeof.
        return true;
    }

    template <typename C>
    static constexpr bool test(...)
    {
        return false;
    }

    // int is used to give the precedence!
    static constexpr bool value = test<T>(int());
};
}
#endif // S_COMMON_H

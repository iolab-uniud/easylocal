#include <limits>
#include <cmath>
#include <cstdint>

#include "easylocal/utils/types.hh"

namespace EasyLocal {
  
  namespace Core {
      
    template <>
    bool IsZero<int>(int value)
    { return value == 0; }

    template <>
    bool EqualTo<int>(int value1, int value2)
    { return value1 == value2; }

    template <>
    bool LessThan<int>(int value1, int value2)
    { return value1 < value2; }

    template <>
    bool LessOrEqualThan<int>(int value1, int value2)
    { return value1 <= value2; }

    template <>
    bool GreaterThan<int>(int value1, int value2)
    { return value1 > value2; }

    template <>
    bool GreaterOrEqualThan<int>(int value1, int value2)
    { return value1 >= value2; }

    template <>
    bool IsZero<int64_t>(int64_t value)
    { return value == 0; }


    template <>
    bool EqualTo<int64_t>(int64_t value1, int64_t value2)
    { return value1 == value2; }

    template <>
    bool LessThan<int64_t>(int64_t value1, int64_t value2)
    { return value1 < value2; }

    template <>
    bool LessOrEqualThan<int64_t>(int64_t value1, int64_t value2)
    { return value1 <= value2; }

    template <>
    bool GreaterThan<int64_t>(int64_t value1, int64_t value2)
    { return value1 > value2; }

    template <>
    bool GreaterOrEqualThan<int64_t>(int64_t value1, int64_t value2)
    { return value1 >= value2; }


    template <>
    bool IsZero<float>(float value)
    { return fabsf(value) <= std::numeric_limits<float>::epsilon(); }

    template <>
    bool EqualTo<float>(float value1, float value2)
    { return fabsf(value1 - value2) <= std::numeric_limits<float>::epsilon(); }

    template <>
    bool LessThan<float>(float value1, float value2)
    { return value1 + std::numeric_limits<float>::epsilon() < value2; }

    template <>
    bool LessOrEqualThan<float>(float value1, float value2)
    { return value1  <= value2; }

    template <>
    bool GreaterThan<float>(float value1, float value2)
    { return value1 + std::numeric_limits<float>::epsilon() > value2; }

    template <>
    bool GreaterOrEqualThan<float>(float value1, float value2)
    { return value1 >= value2; }

    template <>
    bool IsZero<double>(double value)
    { return fabs(value) <= std::numeric_limits<double>::epsilon(); }

    template <>
    bool EqualTo<double>(double value1, double value2)
    { return fabs(value1 - value2) <= std::numeric_limits<double>::epsilon(); }

    template <>
    bool LessThan<double>(double value1, double value2)
    { return value1 + std::numeric_limits<double>::epsilon() < value2; }

    template <>
    bool LessOrEqualThan<double>(double value1, double value2)
    { return value1 <= value2; }

    template <>
    bool GreaterThan<double>(double value1, double value2)
    { return value1 - std::numeric_limits<double>::epsilon() > value2; }

    template <>
    bool GreaterOrEqualThan<double>(double value1, double value2)
    { return value1  >= value2; }
      
    
  }
}

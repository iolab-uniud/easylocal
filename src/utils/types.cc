#include <limits>
#include <cmath>
#include <cstdint>

#include "easylocal/utils/types.hh"

namespace EasyLocal
{
  
  namespace Core
  {
    
    template <>
    bool IsZero<int>(int value)
    {
      return value == 0;
    }
    
    template <>
    bool EqualTo<int>(int value1, int value2)
    {
      return value1 == value2;
    }
    
    template <>
    bool LessThan<int>(int value1, int value2)
    {
      return value1 < value2;
    }
    
    template <>
    bool LessThanOrEqualTo<int>(int value1, int value2)
    {
      return value1 <= value2;
    }
    
    template <>
    bool GreaterThan<int>(int value1, int value2)
    {
      return value1 > value2;
    }
    
    template <>
    bool GreaterThanOrEqualTo<int>(int value1, int value2)
    {
      return value1 >= value2;
    }
    
    template <>
    bool IsZero<int64_t>(int64_t value)
    {
      return value == 0;
    }
    
    template <>
    bool EqualTo<int64_t>(int64_t value1, int64_t value2)
    {
      return value1 == value2;
    }
    
    template <>
    bool LessThan<int64_t>(int64_t value1, int64_t value2)
    {
      return value1 < value2;
    }
    
    template <>
    bool LessThanOrEqualTo<int64_t>(int64_t value1, int64_t value2)
    {
      return value1 <= value2;
    }
    
    template <>
    bool GreaterThan<int64_t>(int64_t value1, int64_t value2)
    {
      return value1 > value2;
    }
    
    template <>
    bool GreaterThanOrEqualTo<int64_t>(int64_t value1, int64_t value2)
    {
      return value1 >= value2;
    }
    
    template <>
    bool IsZero<float>(float value)
    {
      return fabsf(value) <= std::numeric_limits<float>::epsilon();
    }
    
    template <>
    bool EqualTo<float>(float value1, float value2)
    {
      return fabsf(value1 - value2) <= std::numeric_limits<float>::epsilon();
    }
    
    template <>
    bool LessThan<float>(float value1, float value2)
    {
      return value1 + std::numeric_limits<float>::epsilon() < value2;
    }
    
    template <>
    bool LessThanOrEqualTo<float>(float value1, float value2)
    {
      return value1 <= value2;
    }
    
    template <>
    bool GreaterThan<float>(float value1, float value2)
    {
      return value1 + std::numeric_limits<float>::epsilon() > value2;
    }
    
    template <>
    bool GreaterThanOrEqualTo<float>(float value1, float value2)
    {
      return value1 >= value2;
    }
    
    template <>
    bool IsZero<double>(double value)
    {
      return fabs(value) <= std::numeric_limits<double>::epsilon();
    }
    
    template <>
    bool EqualTo<double>(double value1, double value2)
    {
      return fabs(value1 - value2) <= std::numeric_limits<double>::epsilon();
    }
    
    template <>
    bool LessThan<double>(double value1, double value2)
    {
      return value1 + std::numeric_limits<double>::epsilon() < value2;
    }
    
    template <>
    bool LessThanOrEqualTo<double>(double value1, double value2)
    {
      return value1 <= value2;
    }
    
    template <>
    bool GreaterThan<double>(double value1, double value2)
    {
      return value1 - std::numeric_limits<double>::epsilon() > value2;
    }
    
    template <>
    bool GreaterThanOrEqualTo<double>(double value1, double value2)
    {
      return value1 >= value2;
    }
    
    std::vector<std::string> split(const std::string &input, const std::regex &regex)
    {
      // passing -1 as the submatch index parameter performs splitting
      std::sregex_token_iterator first{input.begin(), input.end(), regex, -1}, last;
      return {first, last};
    }
    
    template <typename T>
    std::string GetTypeName()
    {
      return typeid(T).name();
    }
    
    // specializations
    
    template <>
    std::string GetTypeName<bool>()
    {
      return "bool";
    }
    
    template <>
    std::string GetTypeName<char>()
    {
      return "char";
    }
    
    template <>
    std::string GetTypeName<int>()
    {
      return "int";
    }
    
    template <>
    std::string GetTypeName<unsigned int>()
    {
      return "unsigned int";
    }
    
    template <>
    std::string GetTypeName<long>()
    {
      return "long";
    }
    
    template <>
    std::string GetTypeName<unsigned long>()
    {
      return "unsigned long";
    }
    
    template <>
    std::string GetTypeName<float>()
    {
      return "float";
    }
    
    template <>
    std::string GetTypeName<double>()
    {
      return "double";
    }
    
    template <>
    std::string GetTypeName<std::string>()
    {
      return "string";
    }
    
    template <>
    std::string GetTypeName<std::vector<int>>()
    {
      return "vector<int>";
    }
    
    template <>
    std::string GetTypeName<std::vector<unsigned int>>()
    {
      return "vector<unsigned int>";
    }
    
    template <>
    std::string GetTypeName<std::vector<float>>()
    {
      return "vector<float>";
    }
    
    template <>
    std::string GetTypeName<std::vector<double>>()
    {
      return "vector<double>";
    }
  } // namespace Core
} // namespace EasyLocal

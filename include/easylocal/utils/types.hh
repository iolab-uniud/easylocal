#pragma once

#include <vector>
#include <iostream>
#include <cmath>
#include <regex>

namespace EasyLocal
{
  
  namespace Core
  {
    
    template <typename CFtype>
    bool IsZero(CFtype value);
    
    template <typename CFtype>
    bool EqualTo(CFtype value1, CFtype value2);
    
    template <typename CFtype>
    bool LessThan(CFtype value1, CFtype value2);
    
    template <typename CFtype>
    bool LessThanOrEqualTo(CFtype value1, CFtype value2);
    
    template <typename CFtype>
    bool GreaterThan(CFtype value1, CFtype value2);
    
    template <typename CFtype>
    bool GreaterThanOrEqualTo(CFtype value1, CFtype value2);
    
    template <typename CFtype>
    CFtype max(const std::vector<CFtype> &values)
    {
      CFtype max_val = values[0];
      for (unsigned int i = 1; i < values.size(); i++)
        if (values[i] > max_val)
          max_val = values[i];
      
      return max_val;
    }
    
    template <typename CFtype>
    CFtype min(const std::vector<CFtype> &values)
    {
      CFtype min_val = values[0];
      for (unsigned int i = 1; i < values.size(); i++)
        if (values[i] < min_val)
          min_val = values[i];
      
      return min_val;
    }
    
    /** Checks whether @ref Move m2 would undo @ref Move m1 (unless otherwise specified by overloading this helper).
     @param m1 first @ref Move
     @param m2 second @ref Move
     */
    template <class Move>
    bool IsInverse(const Move &m1, const Move &m2)
    {
      return m1 == m2;
    }
    
    std::vector<std::string> split(const std::string &input, const std::regex &regex);
    
    /** returns the type name as a string */
    template <typename T>
    std::string GetTypeName();
  } // namespace Core
} // namespace EasyLocal

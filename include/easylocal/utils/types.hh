#pragma once

#include <vector>
#include <iostream>
#include <cmath>
#include <regex>

/* template <class T,
 typename std::enable_if<std::is_integral<T>::value,
 T>::type* = nullptr>
 void do_stuff(T& t) {
 std::cout << "do_stuff integral\n";
 // an implementation for integral types (int, char, unsigned, etc.)
 }
 
 template <class T,
 typename std::enable_if<std::is_class<T>::value,
 T>::type* = nullptr>
 void do_stuff(T& t) {
 // an implementation for class types
 } */

namespace EasyLocal
{
  
  namespace Core
  {
    
    template <typename CFtype, typename std::enable_if<std::is_floating_point<CFtype>::value>::type* = nullptr>
    bool LessThan(CFtype a, CFtype b)
    {
      // according to Knuth's The Art of Computer Programming definitely less than
      return (b - a) > (std::max(std::abs(a), std::abs(b)) * std::numeric_limits<CFtype>::epsilon());
    }
    
    template <typename CFtype, typename std::enable_if<std::is_integral<CFtype>::value>::type* = nullptr>
    bool LessThan(CFtype a, CFtype b)
    {
      return a < b;
    }
    
    template <typename CFtype, typename std::enable_if<std::is_floating_point<CFtype>::value>::type* = nullptr>
    bool LessThanOrEqualTo(CFtype a, CFtype b)
    {
      // according to Knuth's The Art of Computer Programming definitely less than
      return (b - a) >= (std::max(std::abs(a), std::abs(b)) * std::numeric_limits<CFtype>::epsilon());
    }
    
    template <typename CFtype, typename std::enable_if<std::is_integral<CFtype>::value>::type* = nullptr>
    bool LessThanOrEqualTo(CFtype a, CFtype b)
    {
      return a <= b;
    }
    
    template <typename CFtype, typename std::enable_if<std::is_floating_point<CFtype>::value>::type* = nullptr>
    bool GreaterThan(CFtype a, CFtype b)
    {
      // TODO: check if meaningful
      // according to Knuth's The Art of Computer Programming definitely less than
      return (a - b) > (std::max(std::abs(a), std::abs(b)) * std::numeric_limits<CFtype>::epsilon());
    }
    
    template <typename CFtype, typename std::enable_if<std::is_integral<CFtype>::value>::type* = nullptr>
    bool GreaterThan(CFtype a, CFtype b)
    {
      return a > b;
    }
    
    template <typename CFtype, typename std::enable_if<std::is_floating_point<CFtype>::value>::type* = nullptr>
    bool EqualTo(CFtype a, CFtype b)
    {
      return std::abs(a - b) <= (std::max(std::abs(a), std::abs(b)) * std::numeric_limits<CFtype>::epsilon());
    }
    
    template <typename CFtype, typename std::enable_if<std::is_integral<CFtype>::value>::type* = nullptr>
    bool EqualTo(CFtype a, CFtype b)
    {
      return a == b;
    }
    
    template <typename CFtype, typename std::enable_if<std::is_floating_point<CFtype>::value>::type* = nullptr>
    bool IsZero(CFtype a)
    {
      return std::abs(a) <= std::numeric_limits<CFtype>::epsilon();
    }
    
    template <typename CFtype, typename std::enable_if<std::is_integral<CFtype>::value>::type* = nullptr>
    bool IsZero(CFtype a)
    {
      return a == 0;
    }
    
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
    
    inline std::vector<std::string> split(const std::string &input, const std::regex &regex)
    {
      // passing -1 as the submatch index parameter performs splitting
      std::sregex_token_iterator first{input.begin(), input.end(), regex, -1}, last;
      return {first, last};
    }
    
    /** returns the type name as a string */
    template <typename T>
    std::string GetTypeName()
    {
      return typeid(T).name();
    }
  } // namespace Core
} // namespace EasyLocal

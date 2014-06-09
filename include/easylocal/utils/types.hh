#if !defined(_TYPES_HH_)
#define _TYPES_HH_

#include <vector>
#include <iostream>
#include <cmath>

namespace EasyLocal {

  namespace Core {

    template <typename CFtype>
    bool IsZero(CFtype value);

    template <typename CFtype>
    bool EqualTo(CFtype value1, CFtype value2);

    template <typename CFtype>
    bool LessThan(CFtype value1, CFtype value2);

    template <typename CFtype>
    bool LessOrEqualThan(CFtype value1, CFtype value2);

    template <typename CFtype>
    bool GreaterThan(CFtype value1, CFtype value2);

    template <typename CFtype>
    bool GreaterOrEqualThan(CFtype value1, CFtype value2);

    template <typename CFtype>
    CFtype max(const std::vector<CFtype>& values)
    {
      CFtype max_val = values[0];
      for (unsigned int i = 1; i < values.size(); i++)
        if (values[i] > max_val)
          max_val = values[i];
 
      return max_val;
    }
 
    template <typename CFtype>
    CFtype min(const std::vector<CFtype>& values)
    {
      CFtype min_val = values[0];
      for (unsigned int i = 1; i < values.size(); i++)
        if (values[i] < min_val)
          min_val = values[i];
 
      return min_val;
    }
    
      /** Helper function to check whether two @ref Move are related. As a general rule all @ref Move are related (unless otherwise specified by overloading this helper).
          @param m1 first @ref Move
          @param m2 second @ref Move
          @remarks the ordering of moves is important
      */
      template <class Move1, class Move2>
      bool IsRelated(const Move1& m1, const Move2& m2)
      {
        return true;
      }

      /** Checks whether @ref Move m2 would undo @ref Move m1 (unless otherwise specified by overloading this helper).
          @param m1 first @ref Move
          @param m2 second @ref Move 
      */
      template <class Move>
      bool IsInverse(const Move& m1, const Move& m2) 
      {
        return m1 == m2;
      }
  }
}

#endif // _TYPES_HH_

#pragma once

#include <iostream>

/** This class is meant to be temporary, handling deprecated methods in the transition from input-ful to input-less (i.e., functional) easylocal interface.
 If the compilation flag INPUTLESS_STRICT is defined, the use of the deprecated versions of the methods will issue a runtime-error
 */
template <class Input>
class DeprecationHandler
{
public:
#if defined(INPUTLESS_STRICT)
  [[ noreturn ]]
#endif
  const Input& GetInput() const
  {
#if !defined(INPUTLESS_STRICT)
    if (!p_in)
      throw std::runtime_error("You are currently mixing the old-style and new-style easylocal usage. This method could be called only with the old-style usage");
    return *p_in;
#else
    throw std::runtime_error("This version of the method is not available anymore, you should refer to the Input-less variant");
#endif
  }
protected:
  DeprecationHandler(const Input& in) : p_in(&in)
  {
#if defined(INPUTLESS_STRICT)
    throw std::runtime_error("This version of the constructor is not available anymore, you should refer to the Input-less variant");
#else
    std::cerr << "WARNING:" << std::endl;
    std::cerr << "You are currently working with an old-style easylocal usage (namely passing a const reference to the Input object to all classes. This has been deprecated in favour of a functional-style passing of the input object to the relevant method." << std::endl;
    std::cerr << "While runners and solvers are still working with this old-style interface, the helpers might experience some problems, so it is adivsable to update them by removing the Input object from the constructor and adding it to the relevant methods." << std::endl;
    std::cerr << "GO THROUGH THE DEPRECATION WARNINGS AND FIX THEM." << std::endl;
#endif
  }
  DeprecationHandler() : p_in(nullptr) {}
  /** A reference to the input, for the old-style interface. */
private:
  Input const * const p_in;
};
    

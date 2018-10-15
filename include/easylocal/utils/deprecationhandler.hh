#pragma once

#include <iostream>

/** This class is meant to be temporary, handling deprecated methods in the transition from input-ful to input-less (i.e., functional) easylocal interface */
template <class Input>
class DeprecationHandler
{
public:
  const Input& GetInput() const
  {
    if (!p_in)
      throw std::runtime_error("You are currently mixing the old-style and new-style easylocal usage. This method could be called only with the old-style usage");
    return *p_in;
  }
protected:
  DeprecationHandler(const Input& in) : p_in(&in)
  {
    std::cerr << "WARNING:" << std::endl;
    std::cerr << "You are currently working with an old-style easylocal usage (namely passing a const reference to the Input object to all classes. This has been deprecated in favour of a functional-style passing of the input object to the relevant method." << std::endl;
    std::cerr << "While runners and solvers are still working with this old-style interface, the helpers might experience some problems, so it is adivsable to update them by removing the Input object from the constructor and adding it to the relevant methods." << std::endl;
  }
  DeprecationHandler() : p_in(nullptr) {}
  /** A reference to the input, for the old-style interface. */
private:
  Input const * const p_in;
};
    

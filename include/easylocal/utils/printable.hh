#if !defined(_PRINTABLE_HH_)
#define _PRINTABLE_HH_

#include <iostream>

namespace EasyLocal {
  
  namespace Core {
    
    /** Interface for printable objects. Defines output operator based on print method. */
    class Printable
    {
    public:
      
      /** Prints the object.
       @param os output stream to print to
       */
      virtual void Print(std::ostream& os = std::cout) const = 0;
    };
    
    /** Output operator.
     @param os output stream to print to
     @param p printable object
     */
    std::ostream& operator<<(std::ostream& os, const Printable& p);
    
  }
}

#endif
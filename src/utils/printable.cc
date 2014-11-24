#include "easylocal/utils/printable.hh"

namespace EasyLocal {
  namespace Core {
    
    std::ostream& operator<<(std::ostream& os, const Printable& p)
    {
      p.Print(os);
      return os;
    }
  }
}

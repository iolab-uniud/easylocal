#include "utils/Printable.hh"

namespace easylocal {
  std::ostream& operator<<(std::ostream& os, const Printable& p)
  {
    p.print(os);
    return os;
  }
}

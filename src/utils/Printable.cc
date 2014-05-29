#include "utils/Printable.hh"

using namespace EasyLocal;
using namespace Core;

std::ostream& operator<<(std::ostream& os, const Printable& p)
{
p.Print(os);
return os;
}

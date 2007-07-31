#include "EasyLocalObject.hh"
#include <string>

EasyLocalObject::EasyLocalObject()
: name("No name assigned")
{}

EasyLocalObject::EasyLocalObject(const std::string& e_name)
: name(e_name)
{}

EasyLocalObject::~EasyLocalObject()
{}

void EasyLocalObject::Print(std::ostream& os) const
{
    os << name << std::endl;
}

void EasyLocalObject::SetName(const std::string& n)
{
  name = n;
}

const std::string& EasyLocalObject::GetName() const
{
  return name;
}

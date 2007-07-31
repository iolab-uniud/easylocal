#include "RegistrableObject.hh"

#include "EasyLocalSystemObjects.hh"

RegistrableObject::RegistrableObject()
{
#ifdef USE_EXPSPEC
    EasyLocalSystemObjects::Register(this);
#endif
}

RegistrableObject::~RegistrableObject()
{
#ifdef USE_EXPSPEC
    EasyLocalSystemObjects::Unregister(this);
#endif
}

#ifndef OBJECTNOTFOUNDEXCEPTION_HH_
#define OBJECTNOTFOUNDEXCEPTION_HH_

#include "EasyLocalException.hh"
#include <string>

class ObjectNotFoundException
            : public EasyLocalException
{
public:
    ObjectNotFoundException(const string& o_name);
};

#endif /*OBJECTNOTFOUNDEXCEPTION_HH_*/

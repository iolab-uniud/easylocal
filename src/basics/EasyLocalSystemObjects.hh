#ifndef EASYLOCALSYSTEMOBJECTS_HH_
#define EASYLOCALSYSTEMOBJECTS_HH_

#include "EasyLocalObject.hh"
#include "RegistrableObject.hh"
#include "StoppableObject.hh"
#include "EasyLocalException.hh"

#include <sys/resource.h>
#include <list>
#include <stack>
#include <string>


using namespace std;

class EasyLocalSystemObjects
{
    friend void cpulimit_elapsed(int);
    friend class StoppableObject;
    friend class RegistrableObject;
public:
    static EasyLocalObject* Lookup(const string& name) throw(EasyLocalException);
protected:
    static void Register(RegistrableObject* obj);
    static void Unregister(RegistrableObject* obj) throw(EasyLocalException);
private:
    static list<RegistrableObject*> system_objects;
};

#endif /*EASYLOCALSYSTEMOBJECTS_HH_*/

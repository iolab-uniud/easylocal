#include "EasyLocalSystemObjects.hh"

#include "ObjectNotFoundException.hh"
#include "RegistrableObject.hh"

using namespace std;

list<RegistrableObject*> EasyLocalSystemObjects::system_objects;

void EasyLocalSystemObjects::Register(RegistrableObject* obj)
{
    system_objects.push_back(obj);
}

EasyLocalObject* EasyLocalSystemObjects::Lookup(const string& name) throw(EasyLocalException)
{
    static list<RegistrableObject*>::const_iterator i_obj, end_obj;
    for (i_obj = system_objects.begin(), end_obj = system_objects.end(); i_obj != end_obj; i_obj++)
    {
        if ((*i_obj)->GetName() == name)
            return (*i_obj);
    }
    throw ObjectNotFoundException(name);
}

void EasyLocalSystemObjects::Unregister(RegistrableObject* obj) throw(EasyLocalException)
{
    static list<RegistrableObject*>::iterator i_obj, end_obj;
    for (i_obj = system_objects.begin(), end_obj = system_objects.end(); i_obj != end_obj; i_obj++)
        if (*i_obj == obj)
        {
            system_objects.erase(i_obj);
            return;
        }
    throw ObjectNotFoundException(obj->GetName());
}


#include "ObjectNotFoundException.hh"

ObjectNotFoundException::ObjectNotFoundException(const string& o_name)
{ message = "Object " + o_name + " couldn't be found"; }

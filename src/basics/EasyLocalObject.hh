// A -*- C++ -*- header, needed for Emacs editing environments.

/**
@file $URL$
 @brief The declaration of the more abstract object of the EasyLocal++ system.
 
 @author Andrea Schaerf (schaerf@uniud.it), Luca Di Gaspero (l.digaspero@uniud.it)
 @revision $Revision$
 @date $Date$
 */

#ifndef EASYLOCALOBJECT_HH_
#define EASYLOCALOBJECT_HH_

#include "../EasyLocal.hh"
#include <iostream>
#include <string>

/** This class is the root of all classes in the EasyLocal++ system.
It provides basic handling functions such as the Print method to
output the state of the object. */
class EasyLocalObject
{
  friend class EasyLocalSystemObjects;
public:
  virtual void Print(std::ostream& os = std::cout) const;
  const std::string& GetName() const;
  void SetName(const std::string& n);
protected:
	EasyLocalObject();
  EasyLocalObject(const std::string& name);
  virtual ~EasyLocalObject();
  std::string name;
};

#endif /*EASYLOCALOBJECT_HH_*/

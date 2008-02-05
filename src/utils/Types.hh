#ifndef TYPES_HH_
#define TYPES_HH_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

template <typename CFtype>
bool IsZero(CFtype value);

template <typename CFtype>
bool EqualTo(CFtype value1, CFtype value2);

template <typename CFtype>
bool LessThan(CFtype value1, CFtype value2);

template <typename CFtype>
bool LessOrEqualThan(CFtype value1, CFtype value2);

template <typename CFtype>
bool GreaterThan(CFtype value1, CFtype value2);

template <typename CFtype>
bool GreaterOrEqualThan(CFtype value1, CFtype value2);

#endif /*TYPES_HH_*/

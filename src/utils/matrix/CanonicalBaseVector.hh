/*
 *  CanonicalBaseVector.h
 *  CWM
 *
 *  Created by Luca Di Gaspero on 11/11/06.
 *  Copyright 2006 Luca Di Gaspero. All rights reserved.
 *
 */

#ifndef _CANONICALBASEVECTOR_HH
#define _CANONICALBASEVECTOR_HH

#include "Vector.hh"

template <typename T>
class CanonicalBaseVector : public Vector<T>
{
public:
	CanonicalBaseVector(unsigned int i, unsigned int n);
	inline void reset(unsigned int i);
private:
	unsigned int e;
};

template <typename T>
CanonicalBaseVector<T>::CanonicalBaseVector(unsigned int i, unsigned int n)
: Vector<T>((T)0, n), e(i)
{ (*this)[e] = (T)1; }

template <typename T>
inline void CanonicalBaseVector<T>::reset(unsigned int i)
{ 
	(*this)[e] = (T)0; 
	e = i; 
	(*this)[e] = (T)1;
}

#endif

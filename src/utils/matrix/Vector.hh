/*
 *  Vector.h
 *  CWM
 *
 *  Created by Luca Di Gaspero on 10/11/06.
 *  Copyright 2006 Luca Di Gaspero. All rights reserved.
 *
 */

#ifndef _VECTOR_HH
#define _VECTOR_HH

#include <stdexcept>
#include <set>
#include <iostream>
#include <iomanip>
#include <cmath>

template <typename T>
class Vector
{
public: 
	Vector(); 
	Vector(const unsigned int n);  
	Vector(const T& a, const unsigned int n); //initialize to constant value 
	Vector(const T* a, const unsigned int n); // Initialize to array 
	Vector(const Vector &rhs); // copy constructor 
  ~Vector(); // destructor
	
	inline void set(const T* a, const unsigned int n);
	Vector<T> extract(const std::set<unsigned int>& indexes) const;
	inline T& operator[](const unsigned int& i); //i-th element 
	inline const T& operator[](const unsigned int& i) const; 
	
	inline unsigned int size() const;
	inline void resize(const unsigned int n);
	inline void resize(const T& a, const unsigned int n);
	
	Vector<T>& operator=(const Vector<T>& rhs); //assignment 
	Vector<T>& operator=(const T& a); //assign a to every element 
	inline Vector<T>& operator+=(const Vector<T>& rhs);
	inline Vector<T>& operator-=(const Vector<T>& rhs);
	inline Vector<T>& operator*=(const Vector<T>& rhs);
	inline Vector<T>& operator/=(const Vector<T>& rhs);
	inline Vector<T>& operator^=(const Vector<T>& rhs);
	inline Vector<T>& operator+=(const T& a);
	inline Vector<T>& operator-=(const T& a);
	inline Vector<T>& operator*=(const T& a);
	inline Vector<T>& operator/=(const T& a);
	inline Vector<T>& operator^=(const T& a);
private: 
	unsigned int n; // size of array. upper index is n-1 
	T* v; // storage for data
}; 

template <typename T> 
Vector<T>::Vector() 
  : n(0), v(0) 
{} 

template <typename T> 
Vector<T>::Vector(const unsigned int n) 
  : v(new T[n]) 
{
	this->n = n;
} 

template <typename T> 
Vector<T>::Vector(const T& a, const unsigned int n) 
 : v(new T[n])
{ 
	 this->n = n;
	 for (unsigned int i = 0; i < n; i++) 
		 v[i] = a; 
} 

template <typename T> 
Vector<T>::Vector(const T* a, const unsigned int n) 
  : v(new T[n])
{ 
		this->n = n;
		for (unsigned int i = 0; i < n; i++) 
			v[i] = *a++; 
} 

template <typename T> 
Vector<T>::Vector(const Vector<T>& rhs) 
  : v(new T[rhs.n])
{ 
		this->n = rhs.n;
		for (unsigned int	i = 0; i < n; i++) 
			v[i] = rhs[i]; 
} 

template <typename T> 
Vector<T>::~Vector() 
{ 
	if (v != 0) 
		delete[] (v); 
} 

template <typename T> 
void Vector<T>::resize(const unsigned int n) 
{
	if (n == this->n)
		return;
	if (v != 0) 
		delete[] (v); 
	v = new T[n];
  this->n = n;
} 

template <typename T> 
void Vector<T>::resize(const T& a, const unsigned int n) 
{
  resize(n);
	for (unsigned int i = 0; i < n; i++)
		v[i] = a;
} 


template <typename T> 
inline Vector<T>& Vector<T>::operator=(const Vector<T>& rhs) 
// postcondition: normal assignment via copying has been performed; 
// if vector and rhs were different sizes, vector 
// has been resized to match the size of rhs 
{ 
	if (this != &rhs) 
	{ 
		resize(rhs.n);
		for (unsigned int i = 0; i < n; i++) 
			v[i] = rhs[i]; 
	} 
	return *this; 
} 

template <typename T> 
inline Vector<T> & Vector<T>::operator=(const T& a) //assign a to every element 
{ 
	for (unsigned int i = 0; i < n; i++) 
		v[i] = a; 
	return *this; 
} 

template <typename T> 
inline T & Vector<T>::operator[](const unsigned int& i) //subscripting 
{ 
	return v[i]; 
}

template <typename T>
inline const T& Vector<T>::operator[](const unsigned int& i) const //subscripting 
{ 
	return v[i]; 
} 

template <typename T> 
inline unsigned int Vector<T>::size() const 
{ 
	return n; 
}

template <typename T> 
inline void Vector<T>::set(const T* a, unsigned int n) 
{ 
  resize(n);
	for (unsigned int i = 0; i < n; i++) 
		v[i] = a[i]; 
} 

template <typename T> 
inline Vector<T> Vector<T>::extract(const std::set<unsigned int>& indexes) const
{
	Vector<T> tmp(indexes.size());
	unsigned int i = 0;
	
	for (std::set<unsigned int>::const_iterator el = indexes.begin(); el != indexes.end(); el++)
	{
		if (*el >= n)
			throw std::runtime_error("Error extracting subvector: the indexes are out of vector bounds");
		tmp[i++] = v[*el];
	}
	
	return tmp;
}

template <typename T> 
inline Vector<T>& Vector<T>::operator+=(const Vector<T>& rhs)
{
	if (this->size() != rhs.size())
		throw std::runtime_error("Operator+=: vectors have different sizes");
	for (unsigned int i = 0; i < n; i++)
		v[i] += rhs[i];
	
	return *this;
}


template <typename T> 
inline Vector<T>& Vector<T>::operator+=(const T& a)
{
	for (unsigned int i = 0; i < n; i++)
		v[i] += a;
	
	return *this;
}

template <typename T>
inline Vector<T> operator+(const Vector<T>& rhs)
{
	return rhs;
}

template <typename T>
inline Vector<T> operator+(const Vector<T>& lhs, const Vector<T>& rhs)
{
	if (lhs.size() != rhs.size())
		throw std::runtime_error("Operator+: vectors have different sizes");
	Vector<T> tmp(lhs.size());
	for (unsigned int i = 0; i < lhs.size(); i++)
		tmp[i] = lhs[i] + rhs[i];
	
	return tmp;
}

template <typename T>
inline Vector<T> operator+(const Vector<T>& lhs, const T& a)
{
	Vector<T> tmp(lhs.size());
	for (unsigned int i = 0; i < lhs.size(); i++)
		tmp[i] = lhs[i] + a;
		
	return tmp;
}

template <typename T>
inline Vector<T> operator+(const T& a, const Vector<T>& rhs)
{
	Vector<T> tmp(rhs.size());
	for (unsigned int i = 0; i < rhs.size(); i++)
		tmp[i] = a + rhs[i];
		
	return tmp;
}

template <typename T> 
inline Vector<T>& Vector<T>::operator-=(const Vector<T>& rhs)
{
	if (this->size() != rhs.size())
		throw std::runtime_error("Operator-=: vectors have different sizes");
	for (unsigned int i = 0; i < n; i++)
		v[i] -= rhs[i];
	
	return *this;
}


template <typename T> 
inline Vector<T>& Vector<T>::operator-=(const T& a)
{
	for (unsigned int i = 0; i < n; i++)
		v[i] -= a;
	
	return *this;
}

template <typename T>
inline Vector<T> operator-(const Vector<T>& rhs)
{
	return (T)(-1) * rhs;
}

template <typename T>
inline Vector<T> operator-(const Vector<T>& lhs, const Vector<T>& rhs)
{
	if (lhs.size() != rhs.size())
		throw std::runtime_error("Operator-: vectors have different sizes");
	Vector<T> tmp(lhs.size());
	for (unsigned int i = 0; i < lhs.size(); i++)
		tmp[i] = lhs[i] - rhs[i];
	
	return tmp;
}

template <typename T>
inline Vector<T> operator-(const Vector<T>& lhs, const T& a)
{
	Vector<T> tmp(lhs.size());
	for (unsigned int i = 0; i < lhs.size(); i++)
		tmp[i] = lhs[i] - a;
		
	return tmp;
}

template <typename T>
inline Vector<T> operator-(const T& a, const Vector<T>& rhs)
{
	Vector<T> tmp(rhs.size());
	for (unsigned int i = 0; i < rhs.size(); i++)
		tmp[i] = a - rhs[i];
		
	return tmp;
}

template <typename T> 
inline Vector<T>& Vector<T>::operator*=(const Vector<T>& rhs)
{
	if (this->size() != rhs.size())
		throw std::runtime_error("Operator*=: vectors have different sizes");
	for (unsigned int i = 0; i < n; i++)
		v[i] *= rhs[i];
	
	return *this;
}


template <typename T> 
inline Vector<T>& Vector<T>::operator*=(const T& a)
{
	for (unsigned int i = 0; i < n; i++)
		v[i] *= a;
	
	return *this;
}

template <typename T>
inline Vector<T> operator*(const Vector<T>& lhs, const Vector<T>& rhs)
{
	if (lhs.size() != rhs.size())
		throw std::runtime_error("Operator*: vectors have different sizes");
	Vector<T> tmp(lhs.size());
	for (unsigned int i = 0; i < lhs.size(); i++)
		tmp[i] = lhs[i] * rhs[i];
	
	return tmp;
}

template <typename T>
inline Vector<T> operator*(const Vector<T>& lhs, const T& a)
{
	Vector<T> tmp(lhs.size());
	for (unsigned int i = 0; i < lhs.size(); i++)
		tmp[i] = lhs[i] * a;
		
	return tmp;
}

template <typename T>
inline Vector<T> operator*(const T& a, const Vector<T>& rhs)
{
	Vector<T> tmp(rhs.size());
	for (unsigned int i = 0; i < rhs.size(); i++)
		tmp[i] = a * rhs[i];
		
	return tmp;
}

template <typename T> 
inline Vector<T>& Vector<T>::operator/=(const Vector<T>& rhs)
{
	if (this->size() != rhs.size())
		throw std::runtime_error("Operator/=: vectors have different sizes");
	for (unsigned int i = 0; i < n; i++)
		v[i] /= rhs[i];
	
	return *this;
}


template <typename T> 
inline Vector<T>& Vector<T>::operator/=(const T& a)
{
	for (unsigned int i = 0; i < n; i++)
		v[i] /= a;
	
	return *this;
}

template <typename T>
inline Vector<T> operator/(const Vector<T>& lhs, const Vector<T>& rhs)
{
	if (lhs.size() != rhs.size())
		throw std::runtime_error("Operator/: vectors have different sizes");
	Vector<T> tmp(lhs.size());
	for (unsigned int i = 0; i < lhs.size(); i++)
		tmp[i] = lhs[i] / rhs[i];
	
	return tmp;
}

template <typename T>
inline Vector<T> operator/(const Vector<T>& lhs, const T& a)
{
	Vector<T> tmp(lhs.size());
	for (unsigned int i = 0; i < lhs.size(); i++)
		tmp[i] = lhs[i] / a;
		
	return tmp;
}

template <typename T>
inline Vector<T> operator/(const T& a, const Vector<T>& rhs)
{
	Vector<T> tmp(rhs.size());
	for (unsigned int i = 0; i < rhs.size(); i++)
		tmp[i] = a / rhs[i];
		
	return tmp;
}

template <typename T>
inline Vector<T> operator^(const Vector<T>& lhs, const Vector<T>& rhs)
{
	if (lhs.size() != rhs.size())
		throw std::runtime_error("Operator^: vectors have different sizes");
	Vector<T> tmp(lhs.size());
	for (unsigned int i = 0; i < lhs.size(); i++)
		tmp[i] = pow(lhs[i], rhs[i]);
	
	return tmp;
}

template <typename T>
inline Vector<T> operator^(const Vector<T>& lhs, const T& a)
{
	Vector<T> tmp(lhs.size());
	for (unsigned int i = 0; i < lhs.size(); i++)
		tmp[i] = pow(lhs[i], a);
		
	return tmp;
}

template <typename T>
inline Vector<T> operator^(const T& a, const Vector<T>& rhs)
{
	Vector<T> tmp(rhs.size());
	for (unsigned int i = 0; i < rhs.size(); i++)
		tmp[i] = pow(a, rhs[i]);
		
	return tmp;
}

template <typename T>
inline Vector<T>& Vector<T>::operator^=(const Vector<T>& rhs)
{
	if (this->size() != rhs.size())
		throw std::runtime_error("Operator^=: vectors have different sizes");
	for (unsigned int i = 0; i < n; i++)
		v[i] = pow(v[i], rhs[i]);
		
	return *this;
}

template <typename T>
inline Vector<T>& Vector<T>::operator^=(const T& a)
{
	for (unsigned int i = 0; i < n; i++)
		v[i] = pow(v[i], a);
		
	return *this;
}

template <typename T>
inline bool operator==(const Vector<T>& v, const Vector<T>& w)
{
	if (v.size() != w.size())
		throw std::runtime_error("Vectors of different size are not confrontable");
	for (unsigned i = 0; i < v.size(); i++)
		if (v[i] != w[i])
			return false;
	return true;
}

template <typename T>
inline bool operator!=(const Vector<T>& v, const Vector<T>& w)
{
	if (v.size() != w.size())
		throw std::runtime_error("Vectors of different size are not confrontable");
	for (unsigned i = 0; i < v.size(); i++)
		if (v[i] != w[i])
			return true;
	return false;
}

template <typename T>
inline bool operator<(const Vector<T>& v, const Vector<T>& w)
{
	if (v.size() != w.size())
		throw std::runtime_error("Vectors of different size are not confrontable");
	for (unsigned i = 0; i < v.size(); i++)
		if (v[i] >= w[i])
			return false;
	return true;
}

template <typename T>
inline bool operator<=(const Vector<T>& v, const Vector<T>& w)
{
	if (v.size() != w.size())
		throw std::runtime_error("Vectors of different size are not confrontable");
	for (unsigned i = 0; i < v.size(); i++)
		if (v[i] > w[i])
			return false;
	return true;
}

template <typename T>
inline bool operator>(const Vector<T>& v, const Vector<T>& w)
{
	if (v.size() != w.size())
		throw std::runtime_error("Vectors of different size are not confrontable");
	for (unsigned i = 0; i < v.size(); i++)
		if (v[i] <= w[i])
			return false;
	return true;
}

template <typename T>
inline bool operator>=(const Vector<T>& v, const Vector<T>& w)
{
	if (v.size() != w.size())
		throw std::runtime_error("Vectors of different size are not confrontable");
	for (unsigned i = 0; i < v.size(); i++)
		if (v[i] < w[i])
			return false;
	return true;
}

/**
  Input/Output 
 */
template <typename T>
inline std::ostream& operator<<(std::ostream& os, const Vector<T>& v)
{
	os << std::endl << v.size() << std::endl;
	for (unsigned int i = 0; i < v.size() - 1; i++)
		os << std::setw(20) << std::setprecision(16) << v[i] << ", ";
  os << std::setw(20) << std::setprecision(16) << v[v.size() - 1] << std::endl;
	
	return os;
}

template <typename T>
std::istream& operator>>(std::istream& is, Vector<T>& v)
{
  int elements;
  char comma;
	is >> elements;
  v.resize(elements);
	for (unsigned int i = 0; i < elements; i++)
    is >> v[i] >> comma;
	
	return is;
}

/**
Index utilities
 */

std::set<unsigned int> seq(unsigned int s, unsigned int e);

std::set<unsigned int> singleton(unsigned int i);

#endif

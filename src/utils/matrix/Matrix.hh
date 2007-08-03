/*
 *  Matrix.h
 *  CWM
 *
 *  Created by Luca Di Gaspero on 10/11/06.
 *  Copyright 2006 Luca Di Gaspero. All rights reserved.
 *
 */

#ifndef _MATRIX_HH
#define _MATRIX_HH

#include "Vector.hh"
#include <set>
#include <cmath>
#include <iomanip>
#include <stdexcept>

enum MType { DIAG };

template <typename T>
class Matrix 
{
public:
	Matrix(); // Default constructor
	Matrix(const unsigned int n, const unsigned int m); // Construct a n x m matrix
	Matrix(const T& a, const unsigned int n, const unsigned int m); // Initialize the content to constant a
	Matrix(MType t, const T& a, const T& o, const unsigned int n, const unsigned int m);
	Matrix(MType t, const Vector<T>& v, const T& o, const unsigned int n, const unsigned int m);
	Matrix(const T* a, const unsigned int n, const unsigned int m); // Initialize to array 
	Matrix(const Matrix<T>& rhs); // Copy constructor
 	~Matrix(); // destructor
	
	inline T* operator[](const unsigned int& i) { return v[i]; } // Subscripting: row i
	inline const T* operator[](const unsigned int& i) const { return v[i]; }; // const subsctipting
	
	inline void resize(const unsigned int n, const unsigned int m);
	inline void resize(const T& a, const unsigned int n, const unsigned int m);
	
	
	inline Vector<T> extractRow(const unsigned int i) const; 
	inline Vector<T> extractColumn(const unsigned int j) const;
	inline Vector<T> extractDiag() const;
	inline Matrix<T> extractRows(const std::set<unsigned int>& indexes) const;
	inline Matrix<T> extractColumns(const std::set<unsigned int>& indexes) const;
	inline Matrix<T> extract(const std::set<unsigned int>& r_indexes, const std::set<unsigned int>& c_indexes) const;
	
	inline void set(const T* a, unsigned int n, unsigned int m);
	inline void set(const std::set<unsigned int>& r_indexes, const std::set<unsigned int>& c_indexes, const Matrix<T>& m);
	inline void setRow(const unsigned int index, const Vector<T>& v);
	inline void setRow(const unsigned int index, const Matrix<T>& v);
	inline void setRows(const std::set<unsigned int>& indexes, const Matrix<T>& m);
	inline void setColumn(const unsigned int index, const Vector<T>& v);
	inline void setColumn(const unsigned int index, const Matrix<T>& v);
	inline void setColumns(const std::set<unsigned int>& indexes, const Matrix<T>& m);
	
	
	inline unsigned int nrows() const { return n; } // number of rows
	inline unsigned int ncols() const { return m; } // number of columns
	
	inline Matrix<T>& operator=(const Matrix<T>& rhs); // Assignment operator
	inline Matrix<T>& operator=(const T& a); // Assign to every element value a
	inline Matrix<T>& operator+=(const Matrix<T>& rhs);
	inline Matrix<T>& operator-=(const Matrix<T>& rhs);
	inline Matrix<T>& operator*=(const Matrix<T>& rhs);
	inline Matrix<T>& operator/=(const Matrix<T>& rhs);
	inline Matrix<T>& operator^=(const Matrix<T>& rhs);
	inline Matrix<T>& operator+=(const T& a);
	inline Matrix<T>& operator-=(const T& a);
	inline Matrix<T>& operator*=(const T& a);
	inline Matrix<T>& operator/=(const T& a);
	inline Matrix<T>& operator^=(const T& a);
	inline operator Vector<T>();
private:
	unsigned int n; // number of rows
	unsigned int m; // number of columns
	T **v; // storage for data
};

template <typename T>
Matrix<T>::Matrix() 
	: n(0), m(0), v(0)
{}

template <typename T>
Matrix<T>::Matrix(unsigned int n, unsigned int m)
	: v(new T*[n])
{
  register unsigned int i;
	this->n = n; this->m = m;
	v[0] = new T[m * n];
	for (i = 1; i < n; i++)
		v[i] = v[i - 1] + m;
}

template <typename T>
Matrix<T>::Matrix(const T& a, unsigned int n, unsigned int m)
  : v(new T*[n])
{
    register unsigned int i, j;
		this->n = n; this->m = m;
		v[0] = new T[m * n];
		for (i = 1; i < n; i++)
			v[i] = v[i - 1] + m;
		for (i = 0; i < n; i++)
			for (j = 0; j < m; j++)
				v[i][j] = a;
}

template <class T> 
Matrix<T>::Matrix(const T* a, unsigned int n, unsigned int m) 
  : v(new T*[n])
{ 
  register unsigned int i, j;
	this->n = n; this->m = m;
	v[0] = new T[m * n]; 
	for (i = 1; i < n; i++) 
		v[i] = v[i - 1] + m; 
	for (i = 0; i < n; i++) 
		for (j = 0; j < m; j++) 
			v[i][j] = *a++; 
} 

template <class T> 
Matrix<T>::Matrix(MType t, const T& a, const T& o, unsigned int n, unsigned int m) 
: v(new T*[n])
{ 
  register unsigned int i, j;
	this->n = n; this->m = m;
	v[0] = new T[m * n]; 
	for (i = 1; i < n; i++) 
		v[i] = v[i - 1] + m; 
	switch (t)
	{
		case DIAG:
			for (i = 0; i < n; i++) 
				for (j = 0; j < m; j++) 
						if (i != j)
							v[i][j] = o; 
						else
							v[i][j] = a;
			break;
		default:
			throw std::runtime_error("Matrix type not supported");
	}
} 

template <class T> 
Matrix<T>::Matrix(MType t, const Vector<T>& a, const T& o, unsigned int n, unsigned int m) 
: v(new T*[n])
{ 
  register unsigned int i, j;
	this->n = n; this->m = m;
	v[0] = new T[m * n]; 
	for (i = 1; i < n; i++) 
		v[i] = v[i - 1] + m; 
	switch (t)
	{
		case DIAG:
			for (i = 0; i < n; i++) 
				for (j = 0; j < m; j++) 
					if (i != j)
						v[i][j] = o; 
					else
						v[i][j] = a[i];
			break;
		default:
			throw std::runtime_error("Matrix type not supported");
	}
} 

template <typename T>
Matrix<T>::Matrix(const Matrix<T>& rhs)
  : v(new T*[rhs.n])
{
    register unsigned int i, j;
		n = rhs.n; m = rhs.m;
		v[0] = new T[m * n]; 
		for (i = 1; i < n; i++) 
			v[i] = v[i - 1] + m;
		for (i = 0; i < n; i++)
			for (j = 0; j < m; j++)
				v[i][j] = rhs[i][j];
}

template <typename T> 
Matrix<T>::~Matrix() 
{ 
	if (v != 0) { 
		delete[] (v[0]); 
		delete[] (v); 
	} 
}
				
template <typename T> 
inline Matrix<T>& Matrix<T>::operator=(const Matrix<T> &rhs) 
// postcondition: normal assignment via copying has been performed; 
// if matrix and rhs were different sizes, matrix 
// has been resized to match the size of rhs 
{ 
  register unsigned int i, j;
	if (this != &rhs) 
  {
    resize(rhs.n, rhs.m);
		for (i = 0; i < n; i++) 
			for (j = 0; j < m; j++) 
				v[i][j] = rhs[i][j]; 
	} 
	return *this; 
} 

template <typename T> 
inline Matrix<T>& Matrix<T>::operator=(const T& a) // assign a to every element 
{ 
  register unsigned int i, j;
	for (i = 0; i < n; i++) 
		for (j = 0; j < m; j++) 
			v[i][j] = a; 
	return *this; 
} 


template <typename T> 
inline void Matrix<T>::resize(const unsigned int n, const unsigned int m) 
{
  register unsigned int i;
	if (n == this->n && m == this->m)
		return;
	if (v != 0) 
	{ 
		delete[] (v[0]); 
		delete[] (v); 
	} 
	this->n = n; this->m = m;
	v = new T*[n]; 
	v[0] = new T[m * n];  
	for (i = 1; i < n; i++)
		v[i] = v[i - 1] + m;
} 

template <typename T> 
inline void Matrix<T>::resize(const T& a, const unsigned int n, const unsigned int m) 
{
  register unsigned int i, j;
  resize(n, m);
	for (i = 0; i < n; i++)
		for (j = 0; j < m; j++)
			v[i][j] = a;
} 



template <typename T> 
inline Vector<T> Matrix<T>::extractRow(const unsigned int i) const
{
	if (i >= n)
		throw std::runtime_error("Error in extractRow: trying to extract a row out of matrix bounds");
	Vector<T> tmp(v[i], m);
	
	return tmp;
}

template <typename T> 
inline Vector<T> Matrix<T>::extractColumn(const unsigned int j) const
{
  register unsigned int i;
	if (j >= m)
		throw std::runtime_error("Error in extractRow: trying to extract a row out of matrix bounds");
	Vector<T> tmp(n);
	
	for (i = 0; i < n; i++)
		tmp[i] = v[i][j];
	
	return tmp;
}

template <typename T>
inline Vector<T> Matrix<T>::extractDiag() const
{
	register unsigned int d = std::min(n, m), i;
  
	Vector<T> tmp(d);
	
	for (i = 0; i < d; i++)
		tmp[i] = v[i][i];
	
	return tmp;
	
}

template <typename T> 
inline Matrix<T> Matrix<T>::extractRows(const std::set<unsigned int>& indexes) const
{
	Matrix<T> tmp(indexes.size(), m);
	register unsigned int i = 0, j;
	
	for (std::set<unsigned int>::const_iterator el = indexes.begin(); el != indexes.end(); el++)
	{
		for (j = 0; j < m; j++)
		{
			if (*el >= n)
				throw std::runtime_error("Error extracting rows: the indexes are out of matrix bounds");
			tmp[i][j] = v[*el][j];
		}
		i++;
	}
	
	return tmp;
}

template <typename T> 
inline Matrix<T> Matrix<T>::extractColumns(const std::set<unsigned int>& indexes) const
{
	Matrix<T> tmp(n, indexes.size());
	register unsigned int i, j = 0;
	
	for (std::set<unsigned int>::const_iterator el = indexes.begin(); el != indexes.end(); el++)
	{
		for (i = 0; i < n; i++)
		{
			if (*el >= m)
				throw std::runtime_error("Error extracting columns: the indexes are out of matrix bounds");
			tmp[i][j] = v[i][*el];
		}
		j++;
	}
	
	return tmp;
}

template <typename T> 
inline Matrix<T> Matrix<T>::extract(const std::set<unsigned int>& r_indexes, const std::set<unsigned int>& c_indexes) const
{
	Matrix<T> tmp(r_indexes.size(), c_indexes.size());
	register unsigned int i = 0, j;
	
	for (std::set<unsigned int>::const_iterator r_el = r_indexes.begin(); r_el != r_indexes.end(); r_el++)
	{
		if (*r_el >= n)
			throw std::runtime_error("Error extracting submatrix: the indexes are out of matrix bounds");
		j = 0;
		for (std::set<unsigned int>::const_iterator c_el = c_indexes.begin(); c_el != c_indexes.end(); c_el++)
		{
			if (*c_el >= m)
				throw std::runtime_error("Error extracting rows: the indexes are out of matrix bounds");
			tmp[i][j] = v[*r_el][*c_el];
			j++;
		}
		i++;
	}
	
	return tmp;
}

template <typename T> 
inline void Matrix<T>::setRow(unsigned int i, const Vector<T>& a)
{	
	if (i >= n)
		throw std::runtime_error("Error in setRow: trying to set a row out of matrix bounds");
	if (this->m != a.size())
		throw std::runtime_error("Error setting matrix row: ranges are not compatible");
	for (unsigned int j = 0; j < ncols(); j++)
		v[i][j] = a[j];
}

template <typename T> 
inline void Matrix<T>::setRow(unsigned int i, const Matrix<T>& a)
{	
	if (i >= n)
		throw std::runtime_error("Error in setRow: trying to set a row out of matrix bounds");
	if (this->m != a.ncols())
		throw std::runtime_error("Error setting matrix column: ranges are not compatible");
	if (a.nrows() != 1)
		throw std::runtime_error("Error setting matrix column with a non-row matrix");
	for (unsigned int j = 0; j < ncols(); j++)
		v[i][j] = a[0][j];
}

template <typename T> 
inline void Matrix<T>::setRows(const std::set<unsigned int>& indexes, const Matrix<T>& m)
{
	unsigned int i = 0;
	
	if (indexes.size() != m.nrows() || this->m != m.ncols())
		throw std::runtime_error("Error setting matrix rows: ranges are not compatible");
	for (std::set<unsigned int>::const_iterator el = indexes.begin(); el != indexes.end(); el++)
	{
		for (unsigned int j = 0; j < ncols(); j++)
		{
			if (*el >= n)
				throw std::runtime_error("Error in setRows: trying to set a row out of matrix bounds");
			v[*el][j] = m[i][j];
		}
		i++;
	}
}

template <typename T> 
inline void Matrix<T>::setColumn(unsigned int j, const Vector<T>& a)
{	
	if (j >= m)
		throw std::runtime_error("Error in setColumn: trying to set a column out of matrix bounds");
	if (this->n != a.size())
		throw std::runtime_error("Error setting matrix column: ranges are not compatible");
	for (unsigned int i = 0; i < nrows(); i++)
		v[i][j] = a[i];
}

template <typename T> 
inline void Matrix<T>::setColumn(unsigned int j, const Matrix<T>& a)
{	
	if (j >= m)
		throw std::runtime_error("Error in setColumn: trying to set a column out of matrix bounds");
	if (this->n != a.nrows())
		throw std::runtime_error("Error setting matrix column: ranges are not compatible");
	if (a.ncols() != 1)
		throw std::runtime_error("Error setting matrix column with a non-column matrix");
	for (unsigned int i = 0; i < nrows(); i++)
		v[i][j] = a[i][0];
}


template <typename T> 
inline void Matrix<T>::setColumns(const std::set<unsigned int>& indexes, const Matrix<T>& a)
{
	unsigned int j = 0;
	
	if (indexes.size() != a.ncols() || this->n != a.nrows())
		throw std::runtime_error("Error setting matrix columns: ranges are not compatible");
	for (std::set<unsigned int>::const_iterator el = indexes.begin(); el != indexes.end(); el++)
	{
		for (unsigned int i = 0; i < nrows(); i++)
		{
			if (*el >= m)
				throw std::runtime_error("Error in setColumns: trying to set a column out of matrix bounds");
			v[i][*el] = a[i][j];
		}
		j++;
	}
}

template <typename T> 
inline void Matrix<T>::set(const std::set<unsigned int>& r_indexes, const std::set<unsigned int>& c_indexes, const Matrix<T>& a)
{
	unsigned int i = 0, j;
	if (c_indexes.size() != a.ncols() || r_indexes.size() != a.nrows())
		throw std::runtime_error("Error setting matrix elements: ranges are not compatible");
	
	for (std::set<unsigned int>::const_iterator r_el = r_indexes.begin(); r_el != r_indexes.end(); r_el++)
	{
		if (*r_el >= n)
			throw std::runtime_error("Error in set: trying to set a row out of matrix bounds");
		j = 0;
		for (std::set<unsigned int>::const_iterator c_el = c_indexes.begin(); c_el != c_indexes.end(); c_el++)
		{
			if (*c_el >= m)
				throw std::runtime_error("Error in set: trying to set a column out of matrix bounds");
			v[*r_el][*c_el] = a[i][j];
			j++;
		}
		i++;
	}
}

template <typename T> 
inline void Matrix<T>::set(const T* a, unsigned int n, unsigned int m)
{
	if (this->n != n || this->m != m)
		resize(n, m);
	unsigned int k = 0;
	for (unsigned int i = 0; i < n; i++)
		for (unsigned int j = 0; j < m; j++)
			v[i][j] = a[k++];
}


template <typename T>
Matrix<T> operator+(const Matrix<T>& rhs)
{
	return rhs;
}

template <typename T>
Matrix<T> operator+(const Matrix<T>& lhs, const Matrix<T>& rhs)
{
	if (lhs.ncols() != rhs.ncols() || lhs.nrows() != rhs.nrows())
		throw std::runtime_error("Operator+: matrices have different sizes");
	Matrix<T> tmp(lhs.nrows(), lhs.ncols());
	for (unsigned int i = 0; i < lhs.nrows(); i++)
		for (unsigned int j = 0; j < lhs.ncols(); j++)
			tmp[i][j] = lhs[i][j] + rhs[i][j];
	
	return tmp;
}

template <typename T>
Matrix<T> operator+(const Matrix<T>& lhs, const T& a)
{
	Matrix<T> tmp(lhs.nrows(), lhs.ncols());
	for (unsigned int i = 0; i < lhs.nrows(); i++)
		for (unsigned int j = 0; j < lhs.ncols(); j++)
			tmp[i][j] = lhs[i][j] + a;
	
	return tmp;
}

template <typename T>
Matrix<T> operator+(const T& a, const Matrix<T>& rhs)
{
	Matrix<T> tmp(rhs.nrows(), rhs.ncols());
	for (unsigned int i = 0; i < rhs.nrows(); i++)
		for (unsigned int j = 0; j < rhs.ncols(); j++)
			tmp[i][j] = a + rhs[i][j];
	
	return tmp;
}

template <typename T>
inline Matrix<T>& Matrix<T>::operator+=(const Matrix<T>& rhs)
{
	if (m != rhs.ncols() || n != rhs.nrows())
		throw std::runtime_error("Operator+=: matrices have different sizes");
	for (unsigned int i = 0; i < n; i++)
		for (unsigned int j = 0; j < m; j++)
			v[i][j] += rhs[i][j];
	
	return *this;
}

template <typename T>
inline Matrix<T>& Matrix<T>::operator+=(const T& a)
{
	for (unsigned int i = 0; i < n; i++)
		for (unsigned int j = 0; j < m; j++)
			v[i][j] += a;
	
	return *this;
}

template <typename T>
Matrix<T> operator-(const Matrix<T>& rhs)
{	
	return (T)(-1) * rhs;
}

template <typename T>
Matrix<T> operator-(const Matrix<T>& lhs, const Matrix<T>& rhs)
{
	if (lhs.ncols() != rhs.ncols() || lhs.nrows() != rhs.nrows())
		throw std::runtime_error("Operator-: matrices have different sizes");
	Matrix<T> tmp(lhs.nrows(), lhs.ncols());
	for (unsigned int i = 0; i < lhs.nrows(); i++)
		for (unsigned int j = 0; j < lhs.ncols(); j++)
			tmp[i][j] = lhs[i][j] - rhs[i][j];
	
	return tmp;
}

template <typename T>
Matrix<T> operator-(const Matrix<T>& lhs, const T& a)
{
	Matrix<T> tmp(lhs.nrows(), lhs.ncols());
	for (unsigned int i = 0; i < lhs.nrows(); i++)
		for (unsigned int j = 0; j < lhs.ncols(); j++)
			tmp[i][j] = lhs[i][j] - a;
	
	return tmp;
}

template <typename T>
Matrix<T> operator-(const T& a, const Matrix<T>& rhs)
{
	Matrix<T> tmp(rhs.nrows(), rhs.ncols());
	for (unsigned int i = 0; i < rhs.nrows(); i++)
		for (unsigned int j = 0; j < rhs.ncols(); j++)
			tmp[i][j] = a - rhs[i][j];
	
	return tmp;
}

template <typename T>
inline Matrix<T>& Matrix<T>::operator-=(const Matrix<T>& rhs)
{
	if (m != rhs.ncols() || n != rhs.nrows())
		throw std::runtime_error("Operator-=: matrices have different sizes");
	for (unsigned int i = 0; i < n; i++)
		for (unsigned int j = 0; j < m; j++)
			v[i][j] -= rhs[i][j];
	
	return *this;
}

template <typename T>
inline Matrix<T>& Matrix<T>::operator-=(const T& a)
{
	for (unsigned int i = 0; i < n; i++)
		for (unsigned int j = 0; j < m; j++)
			v[i][j] -= a;
	
	return *this;
}

template <typename T>
Matrix<T> operator*(const Matrix<T>& lhs, const Matrix<T>& rhs)
{
	if (lhs.ncols() != rhs.ncols() || lhs.nrows() != rhs.nrows())
		throw std::runtime_error("Operator*: matrices have different sizes");
	Matrix<T> tmp(lhs.nrows(), lhs.ncols());
	for (unsigned int i = 0; i < lhs.nrows(); i++)
		for (unsigned int j = 0; j < lhs.ncols(); j++)
			tmp[i][j] = lhs[i][j] * rhs[i][j];
	
	return tmp;
}

template <typename T>
Matrix<T> operator*(const Matrix<T>& lhs, const T& a)
{
	Matrix<T> tmp(lhs.nrows(), lhs.ncols());
	for (unsigned int i = 0; i < lhs.nrows(); i++)
		for (unsigned int j = 0; j < lhs.ncols(); j++)
			tmp[i][j] = lhs[i][j] * a;
	
	return tmp;
}

template <typename T>
Matrix<T> operator*(const T& a, const Matrix<T>& rhs)
{
	Matrix<T> tmp(rhs.nrows(), rhs.ncols());
	for (unsigned int i = 0; i < rhs.nrows(); i++)
		for (unsigned int j = 0; j < rhs.ncols(); j++)
			tmp[i][j] = a * rhs[i][j];
	
	return tmp;
}

template <typename T>
inline Matrix<T>& Matrix<T>::operator*=(const Matrix<T>& rhs)
{
	if (m != rhs.ncols() || n != rhs.nrows())
		throw std::runtime_error("Operator*=: matrices have different sizes");
	for (unsigned int i = 0; i < n; i++)
		for (unsigned int j = 0; j < m; j++)
			v[i][j] *= rhs[i][j];
	
	return *this;
}

template <typename T>
inline Matrix<T>& Matrix<T>::operator*=(const T& a)
{
	for (unsigned int i = 0; i < n; i++)
		for (unsigned int j = 0; j < m; j++)
			v[i][j] *= a;
	
	return *this;
}

template <typename T>
Matrix<T> operator/(const Matrix<T>& lhs, const Matrix<T>& rhs)
{
	if (lhs.ncols() != rhs.ncols() || lhs.nrows() != rhs.nrows())
		throw std::runtime_error("Operator+: matrices have different sizes");
	Matrix<T> tmp(lhs.nrows(), lhs.ncols());
	for (unsigned int i = 0; i < lhs.nrows(); i++)
		for (unsigned int j = 0; j < lhs.ncols(); j++)
			tmp[i][j] = lhs[i][j] / rhs[i][j];
	
	return tmp;
}

template <typename T>
Matrix<T> operator/(const Matrix<T>& lhs, const T& a)
{
	Matrix<T> tmp(lhs.nrows(), lhs.ncols());
	for (unsigned int i = 0; i < lhs.nrows(); i++)
		for (unsigned int j = 0; j < lhs.ncols(); j++)
			tmp[i][j] = lhs[i][j] / a;
	
	return tmp;
}

template <typename T>
Matrix<T> operator/(const T& a, const Matrix<T>& rhs)
{
	Matrix<T> tmp(rhs.nrows(), rhs.ncols());
	for (unsigned int i = 0; i < rhs.nrows(); i++)
		for (unsigned int j = 0; j < rhs.ncols(); j++)
			tmp[i][j] = a / rhs[i][j];
	
	return tmp;
}

template <typename T>
inline Matrix<T>& Matrix<T>::operator/=(const Matrix<T>& rhs)
{
	if (m != rhs.ncols() || n != rhs.nrows())
		throw std::runtime_error("Operator+=: matrices have different sizes");
	for (unsigned int i = 0; i < n; i++)
		for (unsigned int j = 0; j < m; j++)
			v[i][j] /= rhs[i][j];
	
	return *this;
}

template <typename T>
inline Matrix<T>& Matrix<T>::operator/=(const T& a)
{
	for (unsigned int i = 0; i < n; i++)
		for (unsigned int j = 0; j < m; j++)
			v[i][j] /= a;
	
	return *this;
}

template <typename T>
Matrix<T> operator^(const Matrix<T>& lhs, const T& a)
{
	Matrix<T> tmp(lhs.nrows(), lhs.ncols());
	for (unsigned int i = 0; i < lhs.nrows(); i++)
		for (unsigned int j = 0; j < lhs.ncols(); j++)
			tmp[i][j] = pow(lhs[i][j], a);
	
	return tmp;
}

template <typename T>
inline Matrix<T>& Matrix<T>::operator^=(const Matrix<T>& rhs)
{
	if (m != rhs.ncols() || n != rhs.nrows())
		throw std::runtime_error("Operator^=: matrices have different sizes");
	for (unsigned int i = 0; i < n; i++)
		for (unsigned int j = 0; j < m; j++)
			v[i][j] = pow(v[i][j], rhs[i][j]);
	
	return *this;
}


template <typename T>
inline Matrix<T>& Matrix<T>::operator^=(const T& a)
{
	for (unsigned int i = 0; i < n; i++)
		for (unsigned int j = 0; j < m; j++)
			v[i][j] = pow(v[i][j], a);
	
	return *this;
}

template <typename T>
inline Matrix<T>::operator Vector<T>()
{
	if (n > 1 && m > 1)
		throw std::runtime_error("Error matrix cast to vector: trying to cast a multi-dimensional matrix");
	if (n == 1)
		return extractRow(0);
	else
		return extractColumn(0);
}

template <typename T>
inline bool operator==(const Matrix<T>& a, const Matrix<T>& b)
{
	if (a.nrows() != b.nrows() || a.ncols() != b.ncols())
		throw std::runtime_error("Matrices of different size are not confrontable");
	for (unsigned i = 0; i < a.nrows(); i++)
		for (unsigned j = 0; j < a.ncols(); j++)
			if (a[i][j] != b[i][j])
				return false;
	return true;
}

template <typename T>
inline bool operator!=(const Matrix<T>& a, const Matrix<T>& b)
{
	if (a.nrows() != b.nrows() || a.ncols() != b.ncols())
		throw std::runtime_error("Matrices of different size are not confrontable");
	for (unsigned i = 0; i < a.nrows(); i++)
		for (unsigned j = 0; j < a.ncols(); j++)
			if (a[i][j] != b[i][j])
				return true;
	return false;
}



/**
Input/Output 
 */
template <typename T>
std::ostream& operator<<(std::ostream& os, const Matrix<T>& m)
{
	os << std::endl << m.nrows() << " " << m.ncols() << std::endl;
	for (unsigned int i = 0; i < m.nrows(); i++)
	{
		for (unsigned int j = 0; j < m.ncols() - 1; j++)
			os << std::setw(20) << std::setprecision(16) << m[i][j] << ", ";
    os << std::setw(20) << std::setprecision(16) << m[i][m.ncols() - 1] << std::endl;
	}
	
	return os;
}

template <typename T>
std::istream& operator>>(std::istream& is, Matrix<T>& m)
{
  int rows, cols;
  char comma;
	is >> rows >> cols;
  m.resize(rows, cols);
	for (unsigned int i = 0; i < rows; i++)
		for (unsigned int j = 0; j < cols; j++)
      is >> m[i][j] >> comma;
	
	return is;
}


#endif // _MATRIX_H


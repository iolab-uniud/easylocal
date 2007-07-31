/*
 *  MatrixFunctions.h
 *  CWM
 *
 *  Created by Luca Di Gaspero on 12/11/06.
 *  Copyright 2006 Luca Di Gaspero. All rights reserved.
 *
 */

#include "Matrix.hh"
#include "Vector.hh"

#ifndef _MATRIXFUNCTIONS_HH
#define _MATRIXFUNCTIONS_HH

/* Random */

template <typename T>
void random(Matrix<T>& m)
{
	for (unsigned int i = 0; i < m.nrows(); i++)
		for (unsigned int j = 0; j < m.ncols(); j++)
			m[i][j] = (T)(rand() / double(RAND_MAX));
}

/**
Aggregate functions
 */

template <typename T>
Vector<T> sum(const Matrix<T>& m)
{
	Vector<T> tmp((T)0, m.ncols());
	for (unsigned int j = 0; j < m.ncols(); j++)
		for (unsigned int i = 0; i < m.nrows(); i++)
			tmp[j] += m[i][j];
	return tmp;
}

template <typename T>
Vector<T> r_sum(const Matrix<T>& m)
{
	Vector<T> tmp((T)0, m.nrows());
	for (unsigned int i = 0; i < m.nrows(); i++)
		for (unsigned int j = 0; j < m.ncols(); j++)
			tmp[i] += m[i][j];
	return tmp;
}

template <typename T>
T all_sum(const Matrix<T>& m)
{
  T tmp = (T)0;
  for (unsigned int i = 0; i < m.nrows(); i++)
		for (unsigned int j = 0; j < m.ncols(); j++)
			tmp += m[i][j];
  return tmp;
}

template <typename T>
Vector<T> prod(const Matrix<T>& m)
{
	Vector<T> tmp((T)1, m.ncols());
	for (unsigned int j = 0; j < m.ncols(); j++)
		for (unsigned int i = 0; i < m.nrows(); i++)
			tmp[j] *= m[i][j];
	return tmp;
}

template <typename T>
Vector<T> r_prod(const Matrix<T>& m)
{
	Vector<T> tmp((T)1, m.nrows());
	for (unsigned int i = 0; i < m.nrows(); i++)
		for (unsigned int j = 0; j < m.ncols(); j++)
			tmp[i] *= m[i][j];
	return tmp;
}

template <typename T>
T all_prod(const Matrix<T>& m)
{
  T tmp = (T)1;
  for (unsigned int i = 0; i < m.nrows(); i++)
		for (unsigned int j = 0; j < m.ncols(); j++)
			tmp *= m[i][j];
  return tmp;
}

template <typename T>
Vector<T> mean(const Matrix<T>& m)
{
	Vector<T> res((T)0, m.ncols());
	for (unsigned int j = 0; j < m.ncols(); j++)
	{
		for (unsigned int i = 0; i < m.nrows(); i++)
			res[j] += m[i][j];
		res[j] /= m.nrows();
	}
	
  return res;
}

template <typename T>
Vector<T> r_mean(const Matrix<T>& m)
{
	Vector<T> res((T)0, m.rows());
	for (unsigned int i = 0; i < m.nrows(); i++)
	{
		for (unsigned int j = 0; j < m.ncols(); j++)
			res[i] += m[i][j];
		res[i] /= m.nrows();
	}
	
  return res;
}

template <typename T>
T all_mean(const Matrix<T>& m)
{
  T tmp = (T)0;
  for (unsigned int i = 0; i < m.nrows(); i++)
		for (unsigned int j = 0; j < m.ncols(); j++)
			tmp += m[i][j];
  return tmp / (m.nrows() * m.ncols());
}

template <typename T>
Vector<T> var(const Matrix<T>& m, bool sample_correction = false)
{
	Vector<T> res((T)0, m.ncols());
	unsigned int n = m.nrows();
	double sum, ssum;
	for (unsigned int j = 0; j < m.ncols(); j++)
	{	
		sum = (T)0.0; ssum = (T)0.0;
		for (unsigned int i = 0; i < m.nrows(); i++)
		{
			sum += m[i][j];
			ssum += (m[i][j] * m[i][j]);
		}
		if (!sample_correction)
			res[j] = (ssum / n) - (sum / n) * (sum / n);
		else
			res[j] = n * ((ssum / n) - (sum / n) * (sum / n)) / (n - 1);		 
	}
	
	return res;
}

template <typename T>
Vector<T> stdev(const Matrix<T>& m, bool sample_correction = false)
{
	return sqrt(var(m, sample_correction));
}

template <typename T>
Vector<T> r_var(const Matrix<T>& m, bool sample_correction = false)
{
	Vector<T> res((T)0, m.nrows());
	double sum, ssum;
	unsigned int n = m.ncols();
	for (unsigned int i = 0; i < m.nrows(); i++)
	{	
		sum = 0.0; ssum = 0.0;
		for (unsigned int j = 0; j < m.ncols(); j++)
		{
			sum += m[i][j];
			ssum += (m[i][j] * m[i][j]);
		}
		if (!sample_correction)
			res[i] = (ssum / n) - (sum / n) * (sum / n);
		else
			res[i] = n * ((ssum / n) - (sum / n) * (sum / n)) / (n - 1);
	}
	
	return res;
}

template <typename T>
Vector<T> r_stdev(const Matrix<T>& m, bool sample_correction = false)
{
	return sqrt(r_var(m, sample_correction));
}

template <typename T>
Vector<T> max(const Matrix<T>& m)
{
	Vector<T> res(m.ncols());
	double value;
	for (unsigned int j = 0; j < m.ncols(); j++)
	{
		value = m[0][j];
		for (unsigned int i = 1; i < m.nrows(); i++)
			value = std::max(m[i][j], value);
		res[j] = value;
	}
	
  return res;
}

template <typename T>
Vector<T> r_max(const Matrix<T>& m)
{
	Vector<T> res(m.nrows());
	double value;
	for (unsigned int i = 0; i < m.nrows(); i++)
	{
		value = m[i][0];
		for (unsigned int j = 1; j < m.ncols(); j++)
			value = std::max(m[i][j], value);
		res[i] = value;
	}
	
  return res;
}

template <typename T>
Vector<T> min(const Matrix<T>& m)
{
	Vector<T> res(m.ncols());
	double value;
	for (unsigned int j = 0; j < m.ncols(); j++)
	{
		value = m[0][j];
		for (unsigned int i = 1; i < m.nrows(); i++)
			value = std::min(m[i][j], value);
		res[j] = value;
	}
	
  return res;
}

template <typename T>
Vector<T> r_min(const Matrix<T>& m)
{
	Vector<T> res(m.nrows());
	double value;
	for (unsigned int i = 0; i < m.nrows(); i++)
	{
		value = m[i][0];
		for (unsigned int j = 1; j < m.ncols(); j++)
			value = std::min(m[i][j], value);
		res[i] = value;
	}
	
  return res;
}



/**
Single element mathematical functions
 */

template <typename T>
Matrix<T> exp(const Matrix<T>&m)
{
	Matrix<T> tmp(m.nrows(), m.ncols());
	
	for (unsigned int i = 0; i < m.nrows(); i++)
		for (unsigned int j = 0; j < m.ncols(); j++)
			tmp[i][j] = exp(m[i][j]);
	
	return tmp;
}

template <typename T>
Matrix<T> sqrt(const Matrix<T>&m)
{
	Matrix<T> tmp(m.nrows(), m.ncols());
	
	for (unsigned int i = 0; i < m.nrows(); i++)
		for (unsigned int j = 0; j < m.ncols(); j++)
			tmp[i][j] = sqrt(m[i][j]);
	
	return tmp;
}

/**
Matrix operators
 */

template <typename T>
Matrix<T> kron(const Vector<T>& b, const Vector<T>& a)
{
	Matrix<T> tmp(b.size(), a.size());
	for (unsigned int i = 0; i < b.size(); i++)
		for (unsigned int j = 0; j < a.size(); j++)
			tmp[i][j] = a[j] * b[i];
	
	return tmp;
}

template <typename T>
Matrix<T> t(const Matrix<T>& a)
{
	Matrix<T> tmp(a.ncols(), a.nrows());
	for (unsigned int i = 0; i < a.nrows(); i++)
		for (unsigned int j = 0; j < a.ncols(); j++)
			tmp[j][i] = a[i][j];
	
	return tmp;
}

template <typename T>
Matrix<T> dot_prod(const Matrix<T>& a, const Matrix<T>& b)
{
	if (a.ncols() != b.nrows())
		throw std::runtime_error("Error matrix dot product: dimensions of the matrices are not compatible");
	Matrix<T> tmp(a.nrows(), b.ncols());
	for (unsigned int i = 0; i < tmp.nrows(); i++)
		for (unsigned int j = 0; j < tmp.ncols(); j++)
		{
			tmp[i][j] = (T)0;
			for (unsigned int k = 0; k < a.ncols(); k++)
				tmp[i][j] += a[i][k] * b[k][j];
		}
			
			return tmp;
}

template <typename T>
Matrix<T> dot_prod(const Matrix<T>& a, const Vector<T>& b)
{
	if (a.ncols() != b.size())
		throw std::runtime_error("Error matrix dot product: dimensions of the matrix and the vector are not compatible");
	Matrix<T> tmp(a.nrows(), 1);
	for (unsigned int i = 0; i < tmp.nrows(); i++)
	{
		tmp[i][0] = (T)0;
		for (unsigned int k = 0; k < a.ncols(); k++)
			tmp[i][0] += a[i][k] * b[k];
	}
		
	return tmp;
}

template <typename T>
Matrix<T> dot_prod(const Vector<T>& a, const Matrix<T>& b)
{
	if (a.size() != b.ncols())
		throw std::runtime_error("Error matrix dot product: dimensions of the vector and matrix are not compatible");
	Matrix<T> tmp(1, b.ncols());
	for (unsigned int j = 0; j < tmp.ncols(); j++)
	{
		tmp[0][j] = (T)0;
		for (unsigned int k = 0; k < a.size(); k++)
			tmp[0][j] += a[k] * b[k][j];
	}
		
	return tmp;
}

template <typename T>
inline Matrix<double> rank(const Matrix<T> m)
{
  Matrix<double> tmp(m.nrows(), m.ncols());
  for (unsigned int j = 0; j < m.ncols(); j++)
    tmp.setColumn(j, rank<T>(m.extractColumn(j)));
  
  return tmp;                  
}

template <typename T>
inline Matrix<double> r_rank(const Matrix<T> m)
{
  Matrix<double> tmp(m.nrows(), m.ncols());
  for (unsigned int i = 0; i < m.nrows(); i++)
    tmp.setRow(i, rank<T>(m.extractRow(i)));
  
  return tmp;                  
}

#endif

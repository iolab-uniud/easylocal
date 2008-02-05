/*
 *  VectorFunctions.h
 *  CWM
 *
 *  Created by Luca Di Gaspero on 12/11/06.
 *  Copyright 2006 Luca Di Gaspero. All rights reserved.
 *
 */

#ifndef _VECTORFUNCTIONS_HH
#define _VECTORFUNCTIONS_HH

#include "Vector.hh"
#include <stdexcept>

template <typename T>
inline T sum(const Vector<T>& v)
{
	T tmp = (T)0;
	for (unsigned int i = 0; i < v.size(); i++)
		tmp += v[i];
	
	return tmp;
}

template <typename T>
inline T prod(const Vector<T>& v)
{
	T tmp = (T)1;
	for (unsigned int i = 0; i < v.size(); i++)
		tmp *= v[i];
	
	return tmp;
}

template <typename T>
inline T mean(const Vector<T>& v)
{
	T sum = (T)0;
	for (unsigned int i = 0; i < v.size(); i++)
		sum += v[i];
	return sum / v.size();
}

template <typename T>
inline T median(const Vector<T>& v)
{
  Vector<T> tmp = sort(v);
  if (v.size() % 2 == 1) // it is an odd-sized vector
    return tmp[v.size() / 2];
  else
    return 0.5 * (tmp[v.size() / 2 - 1] + tmp[v.size() / 2]);
}

template <typename T>
inline T stdev(const Vector<T>& v, bool sample_correction = false)
{
	return sqrt(var(v, sample_correction));
}

template <typename T>
inline T var(const Vector<T>& v, bool sample_correction = false)
{
	T sum = (T)0, ssum = (T)0;
	unsigned int n = v.size();
	for (unsigned int i = 0; i < n; i++)
	{	
		sum += v[i];
		ssum += (v[i] * v[i]);
	}
	if (!sample_correction)
		return (ssum / n) - (sum / n) * (sum / n);
	else
		return n * ((ssum / n) - (sum / n) * (sum / n)) / (n - 1);
}

template <typename T>
inline T max(const Vector<T>& v)
{
	T value = v[0];
	for (unsigned int i = 1; i < v.size(); i++)
		value = std::max(v[i], value);
	
	return value;
}

template <typename T>
inline T min(const Vector<T>& v)
{
	T value = v[0];
	for (unsigned int i = 1; i < v.size(); i++)
		value = std::min(v[i], value);
	
	return value;
}

template <typename T>
inline unsigned int index_max(const Vector<T>& v)
{
	unsigned int max = 0;
	for (unsigned int i = 1; i < v.size(); i++)
		if (v[i] > v[max])
			max = i;
	
	return max;
}

template <typename T>
inline unsigned int index_min(const Vector<T>& v)
{
	unsigned int min = 0;
	for (unsigned int i = 1; i < v.size(); i++)
		if (v[i] < v[min])
			min = i;
	
	return min;
}


template <typename T>
inline T dot_prod(const Vector<T>& a, const Vector<T>& b)
{
	T sum = (T)0;
	if (a.size() != b.size())
		throw std::logic_error("Dotprod error: the vectors are not the same size");
	for (unsigned int i = 0; i < a.size(); i++)
		sum += a[i] * b[i];
	
	return sum;
}

/**
Single element mathematical functions
 */

template <typename T>
inline Vector<T> exp(const Vector<T>& v)
{
	Vector<T> tmp(v.size());
	for (unsigned int i = 0; i < v.size(); i++)
		tmp[i] = exp(v[i]);
	
	return tmp;
}

template <typename T>
inline Vector<T> log(const Vector<T>& v)
{
	Vector<T> tmp(v.size());
	for (unsigned int i = 0; i < v.size(); i++)
		tmp[i] = log(v[i]);
	
	return tmp;
}

template <typename T>
inline Vector<T> sqrt(const Vector<T>& v)
{
	Vector<T> tmp(v.size());
	for (unsigned int i = 0; i < v.size(); i++)
		tmp[i] = sqrt(v[i]);
	
	return tmp;
}

template <typename T>
inline Vector<T> pow(const Vector<T>& v, double a)
{
	Vector<T> tmp(v.size());
	for (unsigned int i = 0; i < v.size(); i++)
		tmp[i] = pow(v[i], a);
	
	return tmp;
}

template <typename T>
inline Vector<T> abs(const Vector<T>& v)
{
	Vector<T> tmp(v.size());
	for (unsigned int i = 0; i < v.size(); i++)
		tmp[i] = (T)fabs(v[i]);
	
	return tmp;
}

template <typename T>
inline Vector<T> sign(const Vector<T>& v)
{
	Vector<T> tmp(v.size());
	for (unsigned int i = 0; i < v.size(); i++)
		tmp[i] = v[i] > 0 ? +1 : v[i] == 0 ? 0 : -1;
	
	return tmp;
}

template <typename T>
inline unsigned int partition(Vector<T>& v, unsigned int begin, unsigned int end)
{
	unsigned int i = begin + 1, j = begin + 1;
	T pivot = v[begin];
	while (j <= end) 
	{
		if (v[j] < pivot) {
			std::swap(v[i], v[j]);
			i++;
		}
		j++;
	}
	v[begin] = v[i - 1];
	v[i - 1] = pivot;
	return i - 2;
}
	

template <typename T>
inline void quicksort(Vector<T>& v, unsigned int begin, unsigned int end)
{
  if (end > begin)
  {
	  unsigned int index = partition(v, begin, end);
		quicksort(v, begin, index);
		quicksort(v, index + 2, end);
	}
}

template <typename T>
inline Vector<T> sort(const Vector<T>& v)
{
  Vector<T> tmp(v);
  
  quicksort<T>(tmp, 0, tmp.size() - 1);
  
  return tmp;
}

template <typename T>
inline Vector<double> rank(const Vector<T>& v)
{
  Vector<T> tmp(v);
  Vector<double> tmp_rank(0.0, v.size());	
	
	for (unsigned int i = 0; i < tmp.size(); i++)
	{
		unsigned int smaller = 0, equal = 0;
    for (unsigned int j = 0; j < tmp.size(); j++)
			if (i == j)
				continue;
			else
				if (tmp[j] < tmp[i])
					smaller++;
				else if (tmp[j] == tmp[i])
					equal++;
		tmp_rank[i] = smaller + 1;
		if (equal > 0)
		{
			for (unsigned int j = 1; j <= equal; j++)
				tmp_rank[i] += smaller + 1 + j;
			tmp_rank[i] /= (double)(equal + 1);
		}
	}
	
  return tmp_rank;
}


#endif

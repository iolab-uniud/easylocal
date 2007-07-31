/*
 *  MatrixAlgorithms.h
 *  CWM
 *
 *  Created by Luca Di Gaspero on 11/11/06.
 *  Copyright 2006 Luca Di Gaspero. All rights reserved.
 *
 */

#ifndef _MATRIXALGORITHMS_HH
#define _MATRIXALGORITHMS_HH

#include "CanonicalBaseVector.hh"

template <typename T>
T sign(const T& v)
{
	if (v >= (T)0.0)
		return (T)1.0;
	else
		return (T)-1.0;
}

template <typename T>
T dist(const T& a, const T& b)
{
	T abs_a = (T)fabs(a), abs_b = (T)fabs(b);
	if (abs_a > abs_b)
		return abs_a * sqrt((T)1.0 + (abs_b / abs_a) * (abs_b / abs_a));
	else
		return (abs_b == (T)0.0 ? (T)0.0 : abs_b * sqrt((T)1.0 + (abs_a / abs_b) * (abs_a / abs_b)));
}

template <typename T>
void svd(const Matrix<T>& A, Matrix<T>& U, Vector<T>& W, Matrix<T>& V)
{
	int m = A.nrows(), n = A.ncols(), i, j, k, l, jj, nm;
	const unsigned int max_its = 30;
	bool flag;
	Vector<T> rv1(n);
	U = A;
	W.resize(n);
	V.resize(n, n);
	T anorm, c, f, g, h, s, scale, x, y, z;
	g = scale = anorm = (T)0.0;
	
	// Householder reduction to bidiagonal form
	for (i = 0; i < n; i++)
	{
		l = i + 1;
		rv1[i] = scale * g;
		g = s = scale = (T)0.0;
		if (i < m)
		{
			for (k = i; k < m; k++)
				scale += fabs(U[k][i]);
			if (scale != (T)0.0)
			{
				for (k = i; k < m; k++)
				{
					U[k][i] /= scale;
					s += U[k][i] * U[k][i];
				}
				f = U[i][i];
				g = -sign(f) * sqrt(s);
				h = f * g - s;
				U[i][i] = f - g;
				for (j = l; j < n; j++)
				{
					s = (T)0.0;
					for (k = i; k < m; k++)
						s += U[k][i] * U[k][j];
					f = s / h;
					for (k = i; k < m; k++)
						U[k][j] += f * U[k][i];
				}
				for (k = i; k < m; k++)
					U[k][i] *= scale;
			}
		}
		W[i] = scale * g;
		g = s = scale = (T)0.0;
		if (i < m && i != n - 1)
		{
			for (k = l; k < n; k++)
				scale += fabs(U[i][k]);
			if (scale != (T)0.0)
			{
				for (k = l; k < n; k++)
				{
					U[i][k] /= scale;
					s += U[i][k] * U[i][k];					
				}
				f = U[i][l];
				g = -sign(f) * sqrt(s);
				h = f * g - s;
				U[i][l] = f - g;
				for (k = l; k <n; k++)
					rv1[k] = U[i][k] / h;
				for (j = l; j < m; j++)
				{
					s = (T)0.0;
					for (k = l; k < n; k++)
						s += U[j][k] * U[i][k];
					for (k = l; k < n; k++)
						U[j][k] += s * rv1[k];
				}
				for (k = l; k < n; k++)
					U[i][k] *= scale;
			}
		}
		anorm = std::max(anorm, fabs(W[i]) + fabs(rv1[i]));
	}
	// Accumulation of right-hand transformations
	for (i = n - 1; i >= 0; i--)
	{
		if (i < n - 1) 
		{
			if (g != (T)0.0)
			{
				for (j = l; j < n; j++)
					V[j][i] = (U[i][j] / U[i][l]) / g;
				for (j = l; j < n; j++)
				{
					s = (T)0.0;
					for (k = l; k < n; k++)
						s += U[i][k] * V[k][j];
					for (k = l; k < n; k++)
						V[k][j] += s * V[k][i];
				}
			}
			for (j = l; j < n; j++)
				V[i][j] = V[j][i] = (T)0.0;
		}
		V[i][i] = (T)1.0;
		g = rv1[i];
		l = i;
	}
	// Accumulation of left-hand transformations
	for (i = std::min(m, n) - 1; i >= 0; i--)
	{
		l = i + 1;
		g = W[i];
		for (j = l; j < n; j++)
			U[i][j] = (T)0.0;
		if (g != (T)0.0)
		{
			g = (T)1.0 / g;
			for (j = l; j < n; j++)
			{
				s = (T)0.0;
				for (k = l; k < m; k++)
					s += U[k][i] * U[k][j];
				f = (s / U[i][i]) * g;
				for (k = i; k < m; k++)
					U[k][j] += f * U[k][i];
			}
			for (j = i; j < m; j++)
				U[j][i] *= g;
		}
		else
			for (j = i; j < m; j++)
				U[j][i] = (T)0.0;
		U[i][i]++;
	}
	// Diagonalization of the bidiagonal form: loop over singular values, and over allowed iterations.
	for (k = n - 1; k >= 0; k--)
	{
		for (unsigned int its = 0; its < max_its; its++)
		{
			flag = true;
			for (l = k; l >= 0; l--) // FIXME: in NR it was l >= 1 but there subscripts start from one
			{ // Test for splitting
				nm = l - 1; // Note that rV[0] is always zero
				if ((T)(fabs(rv1[l]) + anorm) == anorm)
				{
					flag = false;
					break;
				}
				if ((T)(fabs(W[nm]) + anorm) == anorm)
					break;
			}
			if (flag)
			{
				// Cancellation of rv1[l], if l > 0 FIXME: it was l > 1 in NR
				c = (T)0.0;
				s = (T)1.0;
				for (i = l; i <= k; i++)
				{
					f = s * rv1[i];
					rv1[i] *= c;
					if ((T)(fabs(f) + anorm) == anorm)
						break;
					g = W[i];
					h = dist(f, g);
					W[i] = h;
					h = (T)1.0 / h;
					c = g * h;
					s = -f * h;
					for (j = 0; j < m; j++)
					{
						y = U[j][nm];
						z = U[j][i];
						U[j][nm] = y * c + z * s;
						U[j][i] = z * c - y * s;
					}
				}
			}
			z = W[k];
			if (l == k)
			{  // Convergence
				if (z < (T)0.0)
				{ // Singular value is made nonnegative
					W[k] = -z;
					for (j = 0; j < n; j++)
						V[j][k] = -V[j][k];
				}
				break;
			}
			if (its == max_its)
				throw std::runtime_error("Error svd: no convergence in the maximum number of iterations");
			x = W[l];
			nm = k - 1;
			y = W[nm];
			g = rv1[nm];
			h = rv1[k];
			f = ((y - z) * (y + z) + (g - h) * (g + h)) / (2.0 * h * y);
			g = dist(f, (T)1.0);
			f = ((x - z) * (x + z) + h * ((y / (f + sign(f)*fabs(g))) - h)) / x;
			c = s = (T)1.0; // Next QR transformation
			for (j = l; j <= nm; j++)
			{
				i = j + 1;
				g = rv1[i];
				y = W[i];
				h = s * g;
				g *= c;
				z = dist(f, h);
				rv1[j] = z;
				c = f / z;
				s = h / z;
				f = x * c + g * s;
				g = g * c - x * s;
				h = y * s;
				y *= c;
				for (jj = 0; jj < n; jj++)
				{
					x = V[jj][j];
					z = V[jj][i];
					V[jj][j] = x * c + z * s;
					V[jj][i] = z * c - x * s;
				}
				z = dist(f, h);
				W[j] = z; 
				if (z != 0) // Rotation can be arbitrary if z = 0
				{
					z = (T)1.0 / z;
					c = f * z;
					s = h * z;
				}
				f = c * g + s * y;
				x = c * y - s * g;
				for (jj = 0; jj < m; jj++)
				{
					y = U[jj][j];
					z = U[jj][i];
					U[jj][j] = y * c + z * s;
					U[jj][i] = z * c - y * s;
				}
			}
			rv1[l] = (T)0.0;
			rv1[k] = f;
			W[k] = x;
		}
	}	
}

template <typename T>
Matrix<T> pinv(const Matrix<T>& A)
{
	Matrix<T> U, V, x, tmp(A.ncols(), A.nrows());
	Vector<T> W;
	CanonicalBaseVector<T> e(0, A.nrows());
	svd(A, U, W, V);
	for (unsigned int i = 0; i < A.nrows(); i++)
	{
		e.reset(i);
		tmp.setColumn(i, dot_prod(dot_prod(dot_prod(V, Matrix<double>(DIAG, 1.0 / W, 0.0, W.size(), W.size())), t(U)), e));
	}
		
	return tmp;
}

template <typename T>
int lu(const Matrix<T>& A, Matrix<T>& LU, Vector<unsigned int>& index)
{
	if (A.ncols() != A.nrows())
		throw std::runtime_error("Error in LU decomposition: matrix must be squared");
	int i, p, j, k, n = A.ncols(), ex;
	T val, tmp;
	Vector<T> d(n);
	LU = A;
	index.resize(n);
	
	ex = 1;
	for (i = 0; i < n; i++)
	{
		index[i] = i;
		val = (T)0.0;
		for (j = 0; j < n; j++)
			val = std::max(val, (T)fabs(LU[i][j]));
		if (val == (T)0.0)
			std::runtime_error("Error in LU decomposition: matrix was singular");
		d[i] = val;
	}

	for (k = 0; k < n - 1; k++)
	{
		p = k;
		val = fabs(LU[k][k]) / d[k];
		for (i = k + 1; i < n; i++)
		{
			tmp = fabs(LU[i][k]) / d[i];
			if (tmp > val)
			{
				val = tmp;
				p = i;
			}
		}
		if (val == (T)0.0)
			std::runtime_error("Error in LU decomposition: matrix was singular");
		if (p > k)
		{
			ex = -ex;
			std::swap(index[k], index[p]);
			std::swap(d[k], d[p]);
			for (j = 0; j < n; j++)
				std::swap(LU[k][j], LU[p][j]);
		}
		
		for (i = k + 1; i < n; i++)
		{
			LU[i][k] /= LU[k][k];
			for (j = k + 1; j < n; j++)
				LU[i][j] -= LU[i][k] * LU[k][j];
		}
	}
	if (LU[n - 1][n - 1] == (T)0.0)
		std::runtime_error("Error in LU decomposition: matrix was singular");
		
	return ex;
}

template <typename T>
Vector<T> lu_solve(const Matrix<T>& LU, const Vector<T>& b, Vector<unsigned int>& index)
{
	if (LU.ncols() != LU.nrows())
		throw std::runtime_error("Error in LU solve: LU matrix should be squared");
	unsigned int n = LU.ncols();
	if (b.size() != n)
		throw std::runtime_error("Error in LU solve: b vector must be of the same dimensions of LU matrix");
	Vector<T> x((T)0.0, n);
	int i, j, p;
	T sum;
	
	p = index[0];
	x[0] = b[p];
	
	for (i = 1; i < n; i++)
	{
		sum = (T)0.0;
		for (j = 0; j < i; j++)
			sum += LU[i][j] * x[j];
		p = index[i];
		x[i] = b[p] - sum;
	}
	x[n - 1] /= LU[n - 1][n - 1];
	for (i = n - 2; i >= 0; i--)
	{
		sum = (T)0.0;
		for (j = i + 1; j < n; j++)
			sum += LU[i][j] * x[j];
		x[i] = (x[i] - sum) / LU[i][i];
	}
	return x;
}

template <typename T>
void lu_solve(const Matrix<T>& LU, Vector<T>& x, const Vector<T>& b, Vector<unsigned int>& index)
{
	x = lu_solve(LU, b, index);
}

template <typename T>
Matrix<T> lu_inverse(const Matrix<T>& A)
{
	if (A.ncols() != A.nrows())
		throw std::runtime_error("Error in LU invert: matrix must be squared");	
	unsigned int n = A.ncols();
	Matrix<T> A1(n, n), LU;
	Vector<unsigned int> index;
	
	lu(A, LU, index);
	CanonicalBaseVector<T> e(0, n);
	for (unsigned i = 0; i < n; i++)
	{
		e.reset(i);
		A1.setColumn(i, lu_solve(LU, e, index));
	}
	
	return A1;
}

template <typename T>
T lu_det(const Matrix<T>& A)
{
	if (A.ncols() != A.nrows())
		throw std::runtime_error("Error in LU determinant: matrix must be squared");	
	unsigned int d;
	Matrix<T> LU;
	Vector<unsigned int> index;
	
	d = lu(A, LU, index);
	
	return d * prod(LU.extractDiag());
}

template <typename T>
void cholesky(const Matrix<T> A, Matrix<T>& LL) 
{
	if (A.ncols() != A.nrows())
		throw std::runtime_error("Error in Cholesky decomposition: matrix must be squared");
  register int i, j, k, n = A.ncols();
  register double sum;
	LL = A;
	
  for (i = 0; i < n; i++)
  {
    for (j = i; j < n; j++)
    {
      sum = LL[i][j];
      for (k = i - 1; k >= 0; k--)
        sum -= LL[i][k] * LL[j][k];
      if (i == j) 
      {
        if (sum <= 0.0)
          throw std::runtime_error("Error in Cholesky decomposition: matrix is not postive definite");
        LL[i][i] = sqrt(sum);
      }
      else
        LL[j][i] = sum / LL[i][i];
    }
    for (k = i + 1; k < n; k++)
      LL[i][k] = LL[k][i];
  } 
}

template <typename T>
Matrix<T> cholesky(const Matrix<T> A) 
{
	Matrix<T> LL;
	cholesky(A, LL);
	
	return LL;
}

template <typename T>
Vector<T> cholesky_solve(const Matrix<T>& LL, const Vector<T>& b)
{
	if (LL.ncols() != LL.nrows())
		throw std::runtime_error("Error in Cholesky solve: matrix must be squared");
	unsigned int n = LL.ncols();
	if (b.size() != n)
		throw std::runtime_error("Error in Cholesky decomposition: b vector must be of the same dimensions of LU matrix");
  Vector<T> x, y;
	
	/* Solve L * y = b */
	forward_elimination(LL, y, b);
  /* Solve L^T * x = y */
	backward_elimination(LL, x, y);
	
	return x;
}

template <typename T>
void cholesky_solve(const Matrix<T>& LL, Vector<T>& x, const Vector<T>& b)
{
	x = cholesky_solve(LL, b);
}

template <typename T>
void forward_elimination(const Matrix<T>& L, Vector<T>& y, const Vector<T> b)
{
	if (L.ncols() != L.nrows())
		throw std::runtime_error("Error in Forward elimination: matrix must be squared (lower triangular)");
	if (b.size() != L.nrows())
		throw std::runtime_error("Error in Forward elimination: b vector must be of the same dimensions of L matrix");
	register int i, j, n = b.size();
	y.resize(n);
	
	y[0] = b[0] / L[0][0];
	for (i = 1; i < n; i++)
	{
		y[i] = b[i];
		for (j = 0; j < i; j++)
			y[i] -= L[i][j] * y[j];
		y[i] = y[i] / L[i][i];
	}
}

template <typename T>
Vector<T> forward_elimination(const Matrix<T>& L, const Vector<T> b)
{
	Vector<T> y;
	forward_elimination(L, y, b);
	
	return y;
}

template <typename T>
void backward_elimination(const Matrix<T>& U, Vector<T>& x, const Vector<T>& y)
{
	if (U.ncols() != U.nrows())
		throw std::runtime_error("Error in Backward elimination: matrix must be squared (upper triangular)");
	if (y.size() != U.nrows())
		throw std::runtime_error("Error in Backward elimination: b vector must be of the same dimensions of U matrix");
	register int i, j, n = y.size();
	x.resize(n);
	
	x[n - 1] = y[n - 1] / U[n - 1][n - 1];
	for (i = n - 2; i >= 0; i--)
	{
		x[i] = y[i];
		for (j = i + 1; j < n; j++)
			x[i] -= U[i][j] * x[j];
		x[i] = x[i] / U[i][i];
	}
}

template <typename T>
Vector<T> backward_elimination(const Matrix<T>& U, const Vector<T> y)
{
	Vector<T> x;
	forward_elimination(U, x, y);
	
	return x;
}

/* Setting default linear systems machinery */

#define det lu_det
#define inverse lu_inverse
#define solve lu_solve

#endif

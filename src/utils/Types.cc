// $Id$
// This file is part of EasyLocalpp: a C++ Object-Oriented framework
// aimed at easing the development of Local Search algorithms.
// Copyright (C) 2001--2008 Andrea Schaerf, Luca Di Gaspero. 
//
// EasyLocalpp is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// EasyLocalpp is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with EasyLocalpp. If not, see <http://www.gnu.org/licenses/>.

#include <utils/Types.hh>

#include <limits>
#include <cmath>
#include <iostream>

template <>
bool IsZero<int>(int value)
{ return value == 0; }

template <>
bool EqualTo<int>(int value1, int value2)
{ return value1 == value2; }

template <>
bool LessThan<int>(int value1, int value2)
{ return value1 < value2; }

template <>
bool LessOrEqualThan<int>(int value1, int value2)
{ return value1 <= value2; }

template <>
bool GreaterThan<int>(int value1, int value2)
{ return value1 > value2; }

template <>
bool GreaterOrEqualThan<int>(int value1, int value2)
{ return value1 >= value2; }


template <>
bool IsZero<long int>(long int value)
{ return value == 0; }

template <>
bool EqualTo<long int>(long int value1, long int value2)
{ return value1 == value2; }

template <>
bool LessThan<long int>(long int value1, long int value2)
{ return value1 < value2; }

template <>
bool LessOrEqualThan<long int>(long int value1, long int value2)
{ return value1 <= value2; }

template <>
bool GreaterThan<long int>(long int value1, long int value2)
{ return value1 > value2; }

template <>
bool GreaterOrEqualThan<long int>(long int value1, long int value2)
{ return value1 >= value2; }

template <>
bool IsZero<float>(float value)
{ return fabsf(value) <= std::numeric_limits<float>::epsilon(); }

template <>
bool EqualTo<float>(float value1, float value2)
{ return fabsf(value1 - value2) <= std::numeric_limits<float>::epsilon(); }

template <>
bool LessThan<float>(float value1, float value2)
{ return value1 + std::numeric_limits<float>::epsilon() < value2; }

template <>
bool LessOrEqualThan<float>(float value1, float value2)
{ return value1  <= value2; }

template <>
bool GreaterThan<float>(float value1, float value2)
{ return value1 + std::numeric_limits<float>::epsilon() > value2; }

template <>
bool GreaterOrEqualThan<float>(float value1, float value2)
{ return value1 >= value2; }

template <>
bool IsZero<double>(double value)
{ return fabs(value) <= std::numeric_limits<double>::epsilon(); }

template <>
bool EqualTo<double>(double value1, double value2)
{ return fabs(value1 - value2) <= std::numeric_limits<double>::epsilon(); }

template <>
bool LessThan<double>(double value1, double value2)
{ return value1 + std::numeric_limits<double>::epsilon() < value2; }

template <>
bool LessOrEqualThan<double>(double value1, double value2)
{ return value1 <= value2; }

template <>
bool GreaterThan<double>(double value1, double value2)
{ return value1 - std::numeric_limits<double>::epsilon() > value2; }

template <>
bool GreaterOrEqualThan<double>(double value1, double value2)
{ return value1  >= value2; }


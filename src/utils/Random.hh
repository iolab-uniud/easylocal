// $Id$
// This file is part of EasyLocalpp: a C++ Object-Oriented framework
// aimed at easing the development of Local Search algorithms.
// Copyright (C) 2001--2011 Andrea Schaerf, Luca Di Gaspero. 
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

#if !defined(_RANDOM_HH_)
#define _RANDOM_HH_

#include <EasyLocal.conf.hh>
#if defined(HAVE_CONFIG_H)
#include <config.hh>
#endif
#include <vector>


/** A class that encapsulates a random number generator.  It
     generates pseudo-random integer values in some range [a, b],
     using the Mersenne Twister algorithm by Makoto Matsumoto 
     and Takuji Nishimura.
     Moreover, it generates a random perturbation to be applied to
     a real number x.
     The class compels with the Singleton Design pattern, therefore at any
     time in the system exists only a instance of this class that is
     obtainable through the Instance() static function.
 */
class Random
{
public:
    // the Random interface
    static void Seed(const unsigned long seed);
    static void T_Seed();
    static unsigned long GetSeed();
    static bool Bool();
    static bool Bool(const double p);
    static unsigned int Int(const unsigned int max);
    static int Int(const int min, const int max);
    static unsigned long Long(const unsigned long max);
    static long Long(const long min, const long max);
		static std::vector<unsigned int> Permutation(const unsigned int length);
    static double Double();
    static double Double(const double max);
    static double Double(const double min, const double max);
    static double Double_1();
    static double Double_Unit_Uniform();
    static double Double_Gaussian(const double mean, const double stddev);
    static double Double_Unit_Gaussian();
    static double Double_Cauchy();
    static double Double_Exponential();
    static double Perturb(double x);
    static float Float();
    static float Float(const float max);
    static float Float(const float min, const float max);
    static float Float_1();
    static float Float_Unit_Uniform();
    static float Float_Gaussian(const float mean, const float stddev);
    static float Float_Unit_Gaussian();
    static float Float_Cauchy();
    static float Float_Exponential();
    static bool Test();
protected:
    static unsigned long Rand();
    static void NextState();
    static void Init();
    static bool initialized;
    static unsigned long state[]; /* the array for the state vector  */
    static int left;
    static int initf;
    static unsigned long *next;
    static unsigned long initial_seed;
};

#endif // define _RANDOM_HH_

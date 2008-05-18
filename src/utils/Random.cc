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

#include <utils/Random.hh>

#include <climits>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <cfloat>

#if defined(_MSC_VER)
const double M_PI = atan(1.0);
#endif

/*
   For the random number generator Random::Rand() adapted from:
     
   A C-program for MT19937, with initialization improved 2002/1/26.
   Coded by Takuji Nishimura and Makoto Matsumoto.

   Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
   All rights reserved.                          

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     1. Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.

     2. Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

     3. The names of its contributors may not be used to endorse or promote 
        products derived from this software without specific prior written 
        permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


   Any feedback is very welcome.
   http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt.html
   email: m-mat @ math.sci.hiroshima-u.ac.jp (remove space)
*/

// Constant declaration

bool Random::initialized = false;

/* Period parameters */
  #define RANDOM_N 624
  #define RANDOM_M 397
  #define RANDOM_MATRIX_A 0x9908b0dfUL   /* constant vector a */
  #define RANDOM_UMASK 0x80000000UL /* most significant w-r bits */
  #define RANDOM_LMASK 0x7fffffffUL /* least significant r bits */
  #define RANDOM_MIXBITS(u,v) ( ((u) & RANDOM_UMASK) | ((v) & RANDOM_LMASK) )
  #define RANDOM_TWIST(u,v) ((RANDOM_MIXBITS(u,v) >> 1) ^ ((v)&1UL ? RANDOM_MATRIX_A : 0UL))
  #define RANDOM_RAND_MAX 0xffffffffUL 
unsigned long Random::state[RANDOM_N]; /* the array for the state vector  */
int Random::left = 1;
int Random::initf = 0;
unsigned long *Random::next;
unsigned long Random::initial_seed = 0;

/**
    Sets seed for pseudo random number generator.
    Uses the linear congruential algorithm to fill the state array.
    @param seed the seed value.
*/
void Random::Seed(const unsigned long seed)
{
    int j;
    state[0]= seed & 0xffffffffUL;
    for (j = 1; j < RANDOM_N; j++) {
        state[j] = (1812433253UL * (state[j-1] ^ (state[j-1] >> 30)) + j);
        /* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
        /* In the previous versions, MSBs of the seed affect   */
        /* only MSBs of the array state[].                        */
        /* 2002/01/09 modified by Makoto Matsumoto             */
        state[j] &= 0xffffffffUL;  /* for >32 bit machines */
    }
    left = 1; initf = 1;

    initialized = true;
    Random::initial_seed = seed;
}

/**
   Sets the seed for pseudo random number generator from the system time.
*/
void Random::T_Seed()
{
    Seed((unsigned long) (time(NULL) % RANDOM_RAND_MAX));
}

unsigned long Random::GetSeed()
{
  if (!initialized)
    T_Seed();
  return initial_seed;    
}

void Random::NextState()
{
    unsigned long *p = state;
    int j;

    /* if init_genrand() has not been called, */
    /* a default initial seed is used         */
    //if (initf == 0) Seed(5489UL);

    left = RANDOM_N;
    next = state;

    for (j = RANDOM_N - RANDOM_M + 1; --j; p++)
        *p = p[RANDOM_M] ^ RANDOM_TWIST(p[0], p[1]);

    for (j = RANDOM_M; --j; p++)
        *p = p[RANDOM_M-RANDOM_N] ^ RANDOM_TWIST(p[0], p[1]);

    *p = p[RANDOM_M-RANDOM_N] ^ RANDOM_TWIST(p[0], state[0]);
}

/**
   Returns a new pseudo-random value from the sequence, in
   the range 0 to RANDOM_RAND_MAX inclusive, and updates
   global state for next call. Size should be non-zero,
   and state should be initialized.
   @return a random value in the range [0, RANDOM_RAND_MAX]
*/
unsigned long Random::Rand()
{
    if (!initialized)
    {
        T_Seed();
    }

    unsigned long y;

    if (--left == 0) NextState();
    y = *next++;

    /* Tempering */
    y ^= (y >> 11);
    y ^= (y << 7) & 0x9d2c5680UL;
    y ^= (y << 15) & 0xefc60000UL;
    y ^= (y >> 18);

    return y;
}


/**
   Returns a uniformly sampled boolean value, i.e. the value @b true
   with probability 0.5.
   @return a uniform random boolean value
*/
bool Random::Bool()
{
    return (Rand() <= RANDOM_RAND_MAX/2);
}

/**
   Returns a not uniformly sampled boolean value, i.e. the value @b
   true with probability p and the value @b false with probability (1-p).
   @param p the probability of @b true value
   @return a non-uniform random boolean value
*/
bool Random::Bool(const double p)
{
    return (Rand() <= (unsigned long)(p*(double)RANDOM_RAND_MAX));
}

/**
   Returns a uniformly sampled random integer between 0 and max
   inclusive.  
   @param max the upper bound of the range 
   @return a random
   integer value in the range [0, max]
*/
unsigned int Random::Int(const unsigned int max)
{
    return (unsigned int)Rand() % (max + 1);
}

/**
   Returns a uniformly sampled random integer between min and max
   inclusive.
   @param min the lower bound of the range
   @param max the upper bound of the range
   @return a random integer value in the range [min, max]
*/
int Random::Int(const int min, const int max)
{
    return min + ((unsigned int)Rand() % (max - min + 1));
}

/**
   Returns a uniformly sampled random long integer between min and max
   inclusive.
   @param min the lower bound of the range
   @param max the upper bound of the range
   @return a random long integer value in the range [min, max]
*/
long Random::Long(const long min, const long max)
{
    return min + (Rand() % (max - min + 1));
}

/**
  Returns a uniformly sampled random long integer between 0 and max
  inclusive.  
  @param max the upper bound of the range 
  @return a random long integer value in the range [0, max]
*/
unsigned long Random::Long(const unsigned long max)
{
    return Rand() % (max + 1);
}

/**
Returns a random permutation of unsigned integers between 0 and length
 (exclusive).  
 @param length the length of the permutation 
 @return a random vector of unsigned integer values without repetitions
 */
std::vector<unsigned int> Random::Permutation(const unsigned int length)
{
	std::vector<unsigned int> tmp(length);
	unsigned int times = Int(0, length / 2);
	
	for (unsigned int i = 0; i < length; i++)
		tmp[i] = i;
	
	if (length == 0)
		return tmp;
	
	for (unsigned int t = 0; t < times; t++)
  {
		unsigned i = Int(0, length - 1), j;
		do 
			j = Int(0, length - 1);
		while (j == i);
		std::swap(tmp[i], tmp[j]);
	}
	
	return tmp;
}


/**
   Returns a random double within the allowed range.
   @return a random double in the range [DBL_MIN, DBL_MAX]
*/
double Random::Double()
{
    return (((double)Rand() / (double)RANDOM_RAND_MAX) * (DBL_MAX - DBL_MIN) + DBL_MIN);
}

/**
   Perturbs a double value within the allowed range.
   @return a perturbated double value
*/
double Random::Perturb(double x)
{
    unsigned int int_range = 100000;
    double fraction = 0.3;
    unsigned int n = Int(int_range + 1);
    return x * (1.0 + fraction * double(n)/double(int_range) - fraction / 2.0);
}

/**
   Returns a random double value within the range [0,max] or [-|max|,0] 
   depending on the sign of max.
   @return a random double value.
*/
double Random::Double(const double max)
{
    return ((((double)Rand() * max) / (double)RANDOM_RAND_MAX));
}


/**
   Returns a random double within the range [min, max].
   @param min the lower bound of the range.
   @param max the upper bound of the range.
   @return a random double value.
*/
double Random::Double(const double min, const double max)
{
    return ((((double)Rand() * (max - min)) / (double)RANDOM_RAND_MAX) + min );
}


/**
   Return a random double within the range [-1.0, 1.0]
   @return a random double value.
*/
double Random::Double_1()
{
    return ((((double)Rand() *2.0) / (double)RANDOM_RAND_MAX) - 1.0);
}

/**
   Return a pseudo-random number with a uniform distribution in the
   range [0.0, 1.0].
   @return a random unit double value.
*/
double Random::Double_Unit_Uniform()
{
    return ((((double)Rand()) / (double)RANDOM_RAND_MAX));
}

/**
   Returns a pseudo-random number with a normal distribution with a
   given mean and standard deviation.  
   @param mean the mean of the normal distribution
   @param stddev the standard deviation of the normal distribution
   @return a double random value.
   @note based on:
   ALGORITHM 712, COLLECTED ALGORITHMS FROM ACM.
   THIS WORK PUBLISHED IN TRANSACTIONS ON MATHEMATICAL SOFTWARE,
   VOL. 18, NO. 4, DECEMBER, 1992, PP. 434-435.
   The algorithm uses the ratio of uniforms method of A.J. Kinderman
   and J.F. Monahan augmented with quadratic bounding curves.
*/

double Random::Double_Gaussian(const double mean, const double stddev)
{
    double  q,u,v,x,y;

    /*
      Generate P = (u,v) uniform in rect. enclosing acceptance region 
      Make sure that any random numbers <= 0 are rejected, since
      the method requires uniforms > 0, but Dobule_Uniform() delivers >= 0.
    */
    do
    {
        u = Double_Unit_Uniform();
        v = Double_Unit_Uniform();
        if (u <= 0.0 || v <= 0.0)
        {
            u = 1.0;
            v = 1.0;
        }
        v = 1.7156 * (v - 0.5);

        /*  Evaluate the quadratic form */
        x = u - 0.449871;
        y = fabs(v) + 0.386595;
        q = x * x + y * (0.19600 * y - 0.25472 * x);

        /* Accept P if inside inner ellipse */
        if (q < 0.27597)
            break;

        /*  Reject P if outside outer ellipse, or outside acceptance region */
    }
    while ((q > 0.27846) || (v * v > -4.0 * log(u) * u * u));

    /*  Return ratio of P's coordinates as the normal deviate */
    return (mean + stddev * v / u);
}

/**
   Random number generator with normal distribution, average 0.0 and
   standard deviation 1.0.
   @return a double random value.
   @note From Numerical Recipes.
*/
double Random::Double_Unit_Gaussian()
{
    double r, u, v, fac;
    static bool set = false;
    static double dset;

    if (set)
    {
        set = false;
        return dset;
    }

    do
    {
        u = 2.0 * Double_Unit_Uniform() - 1.0;
        v = 2.0 * Double_Unit_Uniform() - 1.0;
        r = u*u + v*v;
    } while (r >= 1.0);

    fac = sqrt(-2.0 * log(r) / r);
    dset = v*fac;

    return u*fac;
}

/**
   Random number with a Cauchy/Lorentzian distribution.
   @return a double random value.
*/
double Random::Double_Cauchy()
{
    return tan(Double(-M_PI/2,M_PI/2));
}


/**
   Random number with an exponential distribution, with the mean of 1.0.
   @return a double random value.
*/
double Random::Double_Exponential(void)
{
    return -log(Double_Unit_Uniform());
}

/**
   Returns a random float within the allowed range.
   @return a random float in the range [FLT_MIN, FLT_MAX]
*/
float Random::Float()
{
    return (((float)Rand() / (float)RANDOM_RAND_MAX) * (FLT_MAX - FLT_MIN) + FLT_MIN);
}

/**
   Returns a random float value within the range [0,max] or [-|max|,0] 
   depending on the sign of max.
   @return a random float value.
*/
float Random::Float(const float max)
{
    return ((((float)Rand() * max) / (float)RANDOM_RAND_MAX));
}


/**
   Returns a random float within the range [min, max].
   @param min the lower bound of the range.
   @param max the upper bound of the range.
   @return a random float value.
*/
float Random::Float(const float min, const float max)
{
    return ((((float)Rand() * (max - min)) / (float)RANDOM_RAND_MAX) + min );
}


/**
   Return a random float within the range [-1.0, 1.0]
   @return a random float value.
*/
float Random::Float_1()
{
    return ((((float)Rand() * 2.0f) / (float)RANDOM_RAND_MAX) - 1.0f);
}

/**
   Return a pseudo-random number with a uniform distribution in the
   range [0.0, 1.0].
   @return a random unit float value.
*/
float Random::Float_Unit_Uniform()
{
    return ((((float)Rand()) / (float)RANDOM_RAND_MAX));
}

/**
   Returns a pseudo-random number with a normal distribution with a
   given mean and standard deviation.  
   @param mean the mean of the normal distribution
   @param stddev the standard deviation of the normal distribution
   @return a float random value.
   @note based on:
   ALGORITHM 712, COLLECTED ALGORITHMS FROM ACM.
   THIS WORK PUBLISHED IN TRANSACTIONS ON MATHEMATICAL SOFTWARE,
   VOL. 18, NO. 4, DECEMBER, 1992, PP. 434-435.
   The algorithm uses the ratio of uniforms method of A.J. Kinderman
   and J.F. Monahan augmented with quadratic bounding curves.
*/

float Random::Float_Gaussian(const float mean, const float stddev)
{
    float q,u,v,x,y;


    // Generate P = (u,v) uniform in rectangular acceptance region.
    do
    {
        u = 1.0f - Float_Unit_Uniform();	           // draw u in the range [0.0, 1.0]
        v = 1.7156f * (0.5f - Float_Unit_Uniform()); // and v  in the range [-0.8578,0.8578]

        // Evaluate the quadratic form
        x = (float)(u - 0.449871f);
        y = (float)(fabs(v) + 0.386595);
        q = x * x + y * (0.19600f * y - 0.25472f * x);
        // Accept P if inside inner ellipse.
        // Reject P if outside outer ellipse, or outside acceptance region.
    } while ((q >= 0.27597f) && ((q > 0.27846f) || (v * v > -4.0f * log(u) * u * u)));

    // Return ratio of P's coordinates as the normal deviate.
    return (mean + 2.0f * stddev * v / u);
}

/**
   Random number generator with normal distribution, average 0.0 and
   standard deviation 1.0.
   @return a float random value.
   @note From Numerical Recipes.
*/
float Random::Float_Unit_Gaussian()
{
    float r, u, v, fac;
    static bool set = false;
    static float dset;

    if (set)
    {
        set = false;
        return dset;
    }

    do
    {
        u = 2.0f * Float_Unit_Uniform() - 1.0f;
        v = 2.0f * Float_Unit_Uniform() - 1.0f;
        r = u*u + v*v;
    } while (r >= 1.0f);

    fac = sqrtf(-2.0f * log(r) / r);
    dset = v*fac;

    return u*fac;
}

/**
   Random number with a Cauchy/Lorentzian distribution.
   @return a float random value.
*/
float Random::Float_Cauchy()
{
    return tan(Float((float)-M_PI/2.0f,(float)M_PI/2.0f));
}


/**
   Random number with an exponential distribution, with the mean of 1.0.
   @return a float random value.
*/
float Random::Float_Exponential()
{
    return -log(Float_Unit_Uniform());
}

/**
   Tests the various random generators
   @return @b true if all tests successful, @b false otherwise
*/

#define NUM_BINS 400
#define NUM_SAMPLES 1000000
#define NUM_CHISQ 20
#define SQU(x) (x) * (x)

bool Random::Test()
{
    double r;			// Pseudo-random number
    long bins[NUM_BINS];	// Bins
    double sum, sumsq;		// Stats
    std::cout << "Testing random numbers." << std::endl;

    // Uniform Distribution.
    std::cout << "Uniform distribution.  Mean should be about 0.5." << std::endl;

    for (unsigned int i = 0; i < NUM_BINS; i++)
        bins[i] = 0;

    sum = 0;
    sumsq = 0;
    for (unsigned int i = 0; i < NUM_SAMPLES; i++)
    {
        r = Double_Unit_Uniform();
        if (r >= 0.0 && r < 1.0)
        {
            bins[(int)(r*NUM_BINS)]++;
            sum += r;
            sumsq += SQU(r);
        }
        else
	  std::cout << "Number generated out of range [0.0, 1.0]." << std::endl;
    }
    std::cout << "Mean = " <<  sum / NUM_SAMPLES << std::endl;
    std::cout << "Standard deviation = "
    << (sumsq - sum * sum / NUM_SAMPLES) / NUM_SAMPLES << std::endl;

    for (unsigned int i = 0; i < NUM_BINS; i++)
        printf("%5.3f %ld\n", i/(double)NUM_BINS, bins[i]);

    // Gaussian Distribution.
    std::cout << "Gaussian distribution.  Mean should be about 0.45.  Standard deviation should be about 0.05." << std::endl;

    sum = 0;
    sumsq = 0;

    for (unsigned int i = 0; i < NUM_BINS; i++)
        bins[i] = 0;

    for (unsigned int i = 0; i < NUM_SAMPLES; i++)
    {
        r = Double_Gaussian(0.45, 0.05);
        if (r >= 0.0 && r < 1.0)
        {
            bins[(int)(r*NUM_BINS)]++;
            sum += r;
            sumsq += SQU(r);
        }
        else
            std::cout << "Number generated out of range [0.0, 1.0]." << std::endl;
    }
    std::cout << "Mean = " <<  sum / NUM_SAMPLES << std::endl;
    std::cout << "Standard deviation = "
    << (sumsq - sum * sum / NUM_SAMPLES) / NUM_SAMPLES  << std::endl;
    std::cout << sumsq  << " " << sum << " " << NUM_SAMPLES << std::endl;

    for (unsigned int i = 0; i < NUM_BINS; i++)
        printf("%5.3f %ld\n", i/(double)NUM_BINS, bins[i]);

    // Unit Gaussian Distribution.
    std::cout << "Gaussian distribution.  Mean should be about 0.0.  Standard deviation should be about 1.0." << std::endl;

    sum = 0;
    sumsq = 0;

    for (unsigned int i = 0; i < NUM_BINS; i++)
        bins[i] = 0;

    for (unsigned int i = 0; i < NUM_SAMPLES; i++)
    {
        r = Double_Unit_Gaussian();
        if (r >= -5.0 && r < 5.0)
        {
            bins[(int)((r+5.0)*NUM_BINS)/10]++;
            sum += r;
            sumsq += SQU(r);
        }
        else
            std::cout << "Number generated out of range [-5.0, 5.0]." << std::endl;
    }
    std::cout << "Mean = " <<  sum / NUM_SAMPLES << std::endl;
    std::cout << "Standard deviation = "
    << (sumsq - sum * sum / NUM_SAMPLES) / NUM_SAMPLES << std::endl;

    for (unsigned int i = 0; i < NUM_BINS; i++)
        printf("%5.3f %ld\n", -5.0+10*i/(double)NUM_BINS, bins[i]);

    // Random bool
    std::cout << "Random Booleans.  Two counts should be approximately equal." << std::endl;

    unsigned int numtrue = 0, numfalse = 0;
    for (unsigned int i = 0; i < NUM_SAMPLES; i++)
    {
        if (Bool())
            numtrue++;
        else
            numfalse++;
    }
    std::cout << "true/false = " << numtrue << '/' << numfalse << std::endl;

    // Random int
    std::cout << "Random Integers.  The distribution should be approximately uniform." << std::endl;

    for (unsigned int i = 0; i < NUM_BINS; i++)
        bins[i] = 0;

    for (unsigned i = 0; i < NUM_SAMPLES; i++)
        bins[Int(NUM_BINS)]++;

    for (unsigned int i = 0; i < NUM_BINS;i++)
        printf("%u %ld\n", i, bins[i]);

    /*
     * Chi squared test.  This is the standard basic test for randomness of a PRNG.
     * We would expect any method to fail about about one out of ten runs.
     * The error is r*t/N - N and should be within 2*sqrt(r) of r.
     */

    unsigned int rchi=100;	// Number of bins in chisq test
    unsigned int nchi=100000;	// Number of samples in chisq test
    double chisq;		// Chisq error
    double elimit = 2*sqrt((double)rchi);      // Chisq error limit
    double nchi_rchi = (double)nchi / (double)rchi; // Speed calculation

    printf("Chi Squared Test of Random Integers.  We would expect a couple of failures.\n");

    if (rchi > NUM_BINS)
    {
        printf("Internal error: too few bins.");
        exit(-1);
    }

    for (unsigned int j = 0; j < NUM_CHISQ; j++)
    {
        printf("Run %u. chisq should be within %f of %u.\n", j, elimit, rchi);
        for(unsigned int k = 0; k < 10; k++)
        {
            memset(bins, 0, rchi*sizeof(long));
            chisq = 0.0;

            for(unsigned int i = 0; i < nchi; i++)
                bins[Int(rchi)]++;

            for(unsigned int i = 0; i < rchi; i++)
                chisq += SQU((double)bins[i] - nchi_rchi);

            chisq /= nchi_rchi;

            printf("chisq = %f - %s\n", chisq, fabs(chisq - rchi)>elimit?"FAILED":"PASSED");
        }
    }

    //     printf("Creating file (\"randtest.dat\") of 5000 random integer numbers for external analysis.\n");

    //     FILE rfile = fopen("randtest.dat", "w");

    //     for (unsigned int i = 0; i < 5000; i++)
    //       {
    // 	r = (double) Rand();
    // 	fprintf(rfile, "%f %f\n",
    // 		/*i, r,*/
    // 		(double)i/(double)5000, r/(double)RANDOM_RAND_MAX);
    //       }

    //     fclose(rfile);

    return true;
}


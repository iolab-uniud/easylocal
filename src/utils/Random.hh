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

#endif // _RANDOM_HH_

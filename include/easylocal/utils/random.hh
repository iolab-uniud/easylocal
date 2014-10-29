#if !defined(_RANDOM_HH_)
#define _RANDOM_HH_

#include <random>
#include <iostream>

namespace EasyLocal {

  namespace Core {
  
    /** Utility static class to generate pseudo-random values according to distributions. 
    In order to make experiments repeatable, each solver must include:
    
      Random::Seed(Random::Int());
    */
    class Random
    {
    public:
      static std::mt19937 g;
      static std::random_device dev;
  
      /** Generates an uniform random integer in [a, b]. 
      @param a lower bound
      @param b upper bound
      */
      static int Int(int a, int b) 
      {
        std::uniform_int_distribution<> d(a, b);
        return d(g);
      }

      /** Generates random int without bounds, useful to generate a random seed. */
      static int Int() 
      {
        std::uniform_int_distribution<> d;
        return d(g);
      }
      /** Generates an uniform random double in [a, b]
      @param a lower bound
      @param b upper bound
      @remarks generates an uniform random double in [0, 1] if called without arguments
      */
      static double Double(double a = 0, double b = 1)
      {
        std::uniform_real_distribution<> d(a, b);
        return d(g);
      }
  
      /** Sets a new seed for the random engine. */
      static int Seed(int seed)
      {
        g.seed(seed);
        return Random::seed = seed;
      }

      static int seed;
    };
  }
}

#endif // _RANDOM_HH_

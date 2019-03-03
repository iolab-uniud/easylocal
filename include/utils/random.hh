#pragma once

#include <random>
#include <iostream>

namespace EasyLocal
{
  
  namespace Core
  {
    
    // TODO: Create a singleton for the Random class
    
    /** Utility static class to generate pseudo-random values according to distributions.
     In order to make experiments repeatable, each solver must include:
     
     Random::Seed(value);
     */
    class Random
    {
    public:
      /** Generates an uniform random integer in [a, b].
       @param a lower bound
       @param b upper bound
       */
      template <typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
      static T Uniform(T a, T b)
      {
        std::uniform_int_distribution<T> d(a, b);
        return d(GetInstance().g);
      }
      
      /** Generates an uniform random float in [a, b].
       @param a lower bound
       @param b upper bound
       */
      template <typename T, typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
      static T Uniform(T a, T b)
      {
        std::uniform_real_distribution<T> d(a, b);
        return d(GetInstance().g);
      }
      
      /** Sets a new seed for the random engine. */
      static unsigned int SetSeed(unsigned int seed)
      {
        Random& r = GetInstance();
        r.g.seed(seed);
        return r.seed = seed;
      }
      
      static unsigned int GetSeed()
      {
        return GetInstance().seed;
      }
      
      
      static std::mt19937& GetGenerator()
      {
        return GetInstance().g;
      }
      
    private:
      static Random& GetInstance() {
        static Random instance;
        return instance;
      }
      
      Random()
      {
        std::random_device dev;
        seed = dev();
        g.seed(seed);
      }
      
      std::mt19937 g;
      
      unsigned int seed;
    };
  } // namespace Core
} // namespace EasyLocal

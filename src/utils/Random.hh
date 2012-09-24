#if !defined(_RANDOM_HH_)
#define _RANDOM_HH_

#include <EasyLocal.conf.hh>
#if defined(HAVE_CONFIG_H)
#include <config.hh>
#endif

#include <random>

class Random
{
public:
  static std::mt19937 g;
  static std::random_device dev;
  
  static int Int(int a, int b) 
  {
    std::uniform_int_distribution<> d(a,b);
    return d(g);
  }
  
  static int Double(double a = 0, double b = 1) 
  {
    std::uniform_real_distribution<> d(a,b);
    return d(g);
  }
  
  static void Seed(int seed)
  {
    g.seed(seed);
  }
};

#endif // _RANDOM_HH_

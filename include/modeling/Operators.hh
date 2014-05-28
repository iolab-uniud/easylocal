#if !defined(_OPERATORS_HH_)
#define _OPERATORS_HH_

#include "modeling/AST.hh"

namespace EasyLocal {
    
  namespace Modeling {
    
    template <typename T>
    class Exp;
    
    template <typename T>
    static Exp<T>& operator+=(Exp<T>& e1, const Exp<T>& e2)
    {
      e1 = Exp<T>(std::make_shared<Sum<T>>(e1, e2));
      e1.simplify();
      return e1;
    }
    
    template <typename T>
    static Exp<T>& operator+=(Exp<T>& e1, const T& v)
    {
      e1 = Exp<T>(std::make_shared<Sum<T>>(Exp<T>(v), e1));
      e1.simplify();
      return e1;
    }
    
    template <typename T>
    static Exp<T> operator+(const Exp<T>& e1, const Exp<T>& e2)
    {
      Exp<T> r(std::make_shared<Sum<T>>(e1, e2));
      r.simplify();
      return r;
    }
    
    template <typename T>
    static Exp<T> operator+(const T& v, const Exp<T>& e)
    {
      if (v != 0)
        return Exp<T>(v) + e;
      else
        return e;
    }
    
    template <typename T>
    static Exp<T> operator+(const Exp<T>& e, const T& v)
    {
      if (v != 0)
        return Exp<T>(v) + e;
      else
        return e;
    }
    
    template <typename T>
    static Exp<T>& operator*=(Exp<T>& e1, const Exp<T>& e2)
    {
      e1 = Exp<T>(std::make_shared<Mul<T>>(e1, e2));
      e1.simplify();
      return e1;
    }
    
    template <typename T>
    static Exp<T>& operator*=(Exp<T>& e1, const T& v)
    {
      if (v == 0)
        e1 = Exp<T>(0);
      if (v != 1)
        e1 = Exp<T>(std::make_shared<Mul<T>>(Exp<T>(v), e1));
      e1.simplify();
      return e1;
    }
    
    template <typename T>
    static Exp<T> operator*(const Exp<T>& e1, const Exp<T>& e2)
    {
      Exp<T> r(std::make_shared<Mul<T>>(e1, e2));
      r.simplify();
      return r;
    }
    
    template <typename T>
    static Exp<T> operator*(const T& v, const Exp<T>& e)
    {
      if (v == 0)
        return Exp<T>(0);
      if (v != 1)
        return Exp<T>(v) * e;
      else
        return e;
    }
    
    template <typename T>
    static Exp<T> operator*(const Exp<T>& e, const T& v)
    {
      if (v == 0)
        return Exp<T>(0);
      if (v != 1)
        return Exp<T>(v) * e;
      else
        return e;
    }
    
    template <typename T>
    static Exp<T> operator==(Exp<T>& e1, Exp<T>& e2)
    {
      Exp<T> r(std::make_shared<Eq<T>>(e1, e2));
      r.simplify();
      return r;
    }
    
    template <typename T>
    static Exp<T> operator==(Exp<T>& e, const T& v)
    {
      return Exp<T>(v) == e;
    }
    
    template <typename T>
    static Exp<T> operator==(const T& v, Exp<T>& e)
    {
      return Exp<T>(v) == e;
    }
  }
}

#endif // _OPERATORS_HH_
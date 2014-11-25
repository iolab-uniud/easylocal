#ifndef _OPERATORS_HH
#define _OPERATORS_HH

#include "ast.hh"

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
    static Exp<T>& operator+=(Exp<T>& e1, T v)
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
    static Exp<T> operator+(T v, const Exp<T>& e)
    {
      if (v != 0)
        return Exp<T>(v) + e;
      else
        return e;
    }
    
    template <typename T>
    static Exp<T> operator+(const Exp<T>& e, T v)
    {
      if (v != 0)
        return Exp<T>(v) + e;
      else
        return e;
    }
    
    template <typename T>
    static Exp<T>& operator-=(Exp<T>& e1, const Exp<T>& e2)
    {
      Exp<T> m_e2(std::make_shared<Mul<T>>(e2, -1));
      e1 = Exp<T>(std::make_shared<Sum<T>>(e1, m_e2));
      e1.simplify();
      return e1;
    }
    
    template <typename T>
    static Exp<T>& operator-=(Exp<T>& e1, T v)
    {
      e1 = Exp<T>(std::make_shared<Sum<T>>(Exp<T>(-v), e1));
      e1.simplify();
      return e1;
    }
    
    template <typename T>
    static Exp<T> operator-(const Exp<T>& e1, const Exp<T>& e2)
    {
      Exp<T> m_e2(std::make_shared<Mul<T>>(e2, -1));
      Exp<T> r(std::make_shared<Sum<T>>(e1, m_e2));
      r.simplify();
      return r;
    }
    
    template <typename T>
    static Exp<T> operator-(T v, const Exp<T>& e)
    {
      if (v != 0)
        return Exp<T>(v) - e;
      else
        return e;
    }
    
    template <typename T>
    static Exp<T> operator-(const Exp<T>& e, T v)
    {
      if (v != 0)
        return e - Exp<T>(v);
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
    static Exp<T>& operator*=(Exp<T>& e1, T v)
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
    static Exp<T> operator*(T v, const Exp<T>& e)
    {
      if (v == 0)
        return Exp<T>(0);
      if (v != 1)
        return Exp<T>(v) * e;
      else
        return e;
    }
    
    template <typename T>
    static Exp<T> operator*(const Exp<T>& e, T v)
    {
      if (v == 0)
        return Exp<T>(0);
      if (v != 1)
        return Exp<T>(v) * e;
      else
        return e;
    }
    
    template <typename T>
    static Exp<T> operator==(const Exp<T>& e1, const Exp<T>& e2)
    {
      Exp<T> r(std::make_shared<Eq<T>>(e1, e2));
      r.simplify();
      return r;
    }
    
    template <typename T>
    static Exp<T> operator==(const Exp<T>& e, T v)
    {
      return Exp<T>(v) == e;
    }
    
    template <typename T>
    static Exp<T> operator==(T v, const Exp<T>& e)
    {
      return Exp<T>(v) == e;
    }
    
    template <typename T>
    static Exp<T> operator!=(const Exp<T>& e1, const Exp<T>& e2)
    {
      Exp<T> r(std::make_shared<Ne<T>>(e1, e2));
      r.simplify();
      return r;
    }
    
    template <typename T>
    static Exp<T> operator!=(const Exp<T>& e, T v)
    {
      return Exp<T>(v) != e;
    }
    
    template <typename T>
    static Exp<T> operator!=(T v, const Exp<T>& e)
    {
      return Exp<T>(v) != e;
    }
    
    template <typename T>
    static Exp<T> operator<=(const Exp<T>& e1, const Exp<T>& e2)
    {
      Exp<T> r(std::make_shared<Le<T>>(e1, e2));
      r.simplify();
      return r;
    }
    
    template <typename T>
    static Exp<T> operator<=(const Exp<T>& e, T v)
    {
      return e <= Exp<T>(v);
    }
    
    template <typename T>
    static Exp<T> operator<=(T v, const Exp<T>& e)
    {
      return Exp<T>(v) <= e;
    }
    
    template <typename T>
    static Exp<T> operator<(const Exp<T>& e1, const Exp<T>& e2)
    {
      Exp<T> r(std::make_shared<Lt<T>>(e1, e2));
      r.simplify();
      return r;
    }
    
    template <typename T>
    static Exp<T> operator<(const Exp<T>& e, T v)
    {
      return e < Exp<T>(v);
    }
    
    template <typename T>
    static Exp<T> operator<(T v, const Exp<T>& e)
    {
      return Exp<T>(v) < e;
    }
    
    template <typename T>
    static Exp<T> operator>=(const Exp<T>& e1, const Exp<T>& e2)
    {
      return e2 <= e1;
    }
    
    template <typename T>
    static Exp<T> operator>=(const Exp<T>& e, T v)
    {
      return Exp<T>(v) <= e;
    }
    
    template <typename T>
    static Exp<T> operator>=(T v, const Exp<T>& e)
    {
      return e <= Exp<T>(v);
    }
    
    template <typename T>
    static Exp<T> operator>(const Exp<T>& e1, const Exp<T>& e2)
    {
      return e2 < e1;
    }
    
    template <typename T>
    static Exp<T> operator>(const Exp<T>& e, T v)
    {
      return Exp<T>(v) < e;
    }
    
    template <typename T>
    static Exp<T> operator>(T v, const Exp<T>& e)
    {
      return e < Exp<T>(v);
    }
         
    template <typename T>
    static Exp<T> alldifferent(const std::vector<Exp<T>>& v)
    {
      Exp<T> e = Exp<T>(std::make_shared<AllDiff<T>>(v));
      e.simplify();
      return e;
    }
    
    template <typename T>
    static Exp<T> abs(const Exp<T>& e)
    {
      Exp<T> t = Exp<T>(std::make_shared<Abs<T>>(e));
      t.simplify();
      return t;
    }
  }
}

#endif
#if !defined(_COST_STRUCTURE_HH_)
#define _COST_STRUCTURE_HH_

#include "easylocal/utils/types.hh"

namespace EasyLocal {
  
  namespace Core {
    
    template <typename CFtype>
    struct DefaultCostStructure
    {
      DefaultCostStructure() : total(0), violations(0), objective(0), all_components(0), weighted(0.0), is_weighted(false) {}
      DefaultCostStructure(CFtype total, CFtype violations, CFtype objective, const std::vector<CFtype>& all_components) : total(total), violations(violations), objective(objective), all_components(all_components), weighted(total), is_weighted(false) {}
      DefaultCostStructure(CFtype total, double weighted, CFtype violations, CFtype objective, const std::vector<CFtype>& all_components) : total(total), violations(violations), objective(objective), all_components(all_components), weighted(weighted), is_weighted(true) {}
      
      
      CFtype total, violations, objective;
      std::vector<CFtype> all_components;
      double weighted;
      
      bool is_weighted;
      
      DefaultCostStructure& operator+=(const DefaultCostStructure& other)
      {
        this->total += other.total;
        this->violations += other.violations;
        this->objective += other.objective;
        if (this->all_components.size() < other.all_components.size())
          this->all_components.resize(other.all_components.size(), 0);
        for (size_t i = 0; i < other.all_components.size(); i++)
          this->all_components[i] += other.all_components[i];
        return *this;
      }
      
      DefaultCostStructure& operator-=(const DefaultCostStructure& other)
      {
        this->total -= other.total;
        this->violations -= other.violations;
        this->objective -= other.objective;
        if (this->all_components.size() < other.all_components.size())
          this->all_components.resize(other.all_components.size(), 0);
        for (size_t i = 0; i < other.all_components.size(); i++)
          this->all_components[i] -= other.all_components[i];
        return *this;
      }
      
      explicit operator double() const
      {
        if (is_weighted)
          return weighted;
        else
          return (double)total;
      }
    };
    
    template <typename CFtype>
    DefaultCostStructure<CFtype> operator+(const DefaultCostStructure<CFtype>& cs1, const DefaultCostStructure<CFtype>& cs2)
    {
      DefaultCostStructure<CFtype> res = cs1;
      res += cs2;
      return res;
    }
    
    template <typename CFtype>
    DefaultCostStructure<CFtype> operator-(const DefaultCostStructure<CFtype>& cs1, const DefaultCostStructure<CFtype>& cs2)
    {
      DefaultCostStructure<CFtype> res = cs1;
      res -= cs2;
      return res;
    }
    
    template <class CFtype>
    bool operator<(const DefaultCostStructure<CFtype>& cs1, const DefaultCostStructure<CFtype>& cs2)
    {
      if (cs1.is_weighted && cs2.is_weighted)
        return LessThan(cs1.weighted, cs2.weighted);
      return LessThan(cs1.total, cs2.total);
    }
    
    template <class CFtype>
    bool operator<(CFtype c1, const DefaultCostStructure<CFtype>& cs2)
    {
      if (cs2.is_weighted)
        return LessThan((double)c1, cs2.weighted);
      return LessThan(c1, cs2.total);
    }
    
    template <class CFtype>
    bool operator<(const DefaultCostStructure<CFtype>& cs1, CFtype c2)
    {
      if (cs1.is_weighted)
        return LessThan(cs1.weighted, (double)c2);
      return LessThan(cs1.total, c2);
    }
    
    template <class CFtype, typename OtherType>
    bool operator<(OtherType c1, const DefaultCostStructure<CFtype>& cs2)
    {
      if (cs2.is_weighted)
        return LessThan((double)c1, cs2.weighted);
      return LessThan((CFtype)c1, cs2.total);
    }
    
    template <class CFtype, typename OtherType>
    bool operator<(const DefaultCostStructure<CFtype>& cs1, OtherType c2)
    {
      if (cs1.is_weighted)
        return LessThan(cs1.weighted, (double)c2);
      return LessThan(cs1.total, (CFtype)c2);
    }
    
    template <class CFtype>
    bool operator<=(const DefaultCostStructure<CFtype>& cs1, const DefaultCostStructure<CFtype>& cs2)
    {
      if (cs1.is_weighted && cs2.is_weighted)
        return LessThanOrEqualTo(cs1.weighted, cs2.weighted);
      return LessThanOrEqualTo(cs1.total, cs2.total);
    }
    
    template <class CFtype>
    bool operator<=(CFtype c1, const DefaultCostStructure<CFtype>& cs2)
    {
      if (cs2.is_weighted)
        return LessThanOrEqualTo((double)c1, cs2.weighted);
      return LessThanOrEqualTo(c1, cs2.total);
    }
    
    template <class CFtype>
    bool operator<=(const DefaultCostStructure<CFtype>& cs1, CFtype c2)
    {
      if (cs1.is_weighted)
        return LessThanOrEqualTo(cs1.weighted, (double)c2);
      return LessThanOrEqualTo(cs1.total, c2);
    }
    
    template <class CFtype, typename OtherType>
    bool operator<=(OtherType c1, const DefaultCostStructure<CFtype>& cs2)
    {
      if (cs2.is_weighted)
        return LessThanOrEqualTo((double)c1, cs2.weighted);
      return LessThanOrEqualTo((CFtype)c1, cs2.total);
    }
    
    template <class CFtype, typename OtherType>
    bool operator<=(const DefaultCostStructure<CFtype>& cs1, OtherType c2)
    {
      if (cs1.is_weighted)
        return LessThanOrEqualTo(cs1.weighted, (double)c2);
      return LessThanOrEqualTo(cs1.total, (CFtype)c2);
    }
    
    template <class CFtype>
    bool operator==(const DefaultCostStructure<CFtype>& cs1, const DefaultCostStructure<CFtype>& cs2)
    {
      if (cs1.is_weighted && cs2.is_weighted)
        return EqualTo(cs1.weighted, cs2.weighted);
      return EqualTo(cs1.total, cs2.total);
    }
    
    template <class CFtype>
    bool operator==(CFtype c1, const DefaultCostStructure<CFtype>& cs2)
    {
      if (cs2.is_weighted)
        return EqualTo((double)c1, cs2.weighted);
      return EqualTo(c1, cs2.total);
    }
    
    template <class CFtype>
    bool operator==(const DefaultCostStructure<CFtype>& cs1, CFtype c2)
    {
      if (cs1.is_weighted)
        return EqualTo(cs1.weighted, (double)c2);
      return EqualTo(cs1.total, c2);
    }
    
    template <class CFtype, typename OtherType>
    bool operator==(OtherType c1, const DefaultCostStructure<CFtype>& cs2)
    {
      if (cs2.is_weighted)
        return EqualTo((double)c1, cs2.weighted);
      return EqualTo((CFtype)c1, cs2.total);
    }
    
    template <class CFtype, typename OtherType>
    bool operator==(const DefaultCostStructure<CFtype>& cs1, OtherType c2)
    {
      if (cs1.is_weighted)
        return EqualTo(cs1.weighted, (double)c2);
      return EqualTo(cs1.total, (CFtype)c2);
    }
    
    template <class CFtype>
    bool operator>=(const DefaultCostStructure<CFtype>& cs1, const DefaultCostStructure<CFtype>& cs2)
    {
      if (cs1.is_weighted && cs2.is_weighted)
        return GreaterThanOrEqualTo(cs1.weighted, cs2.weighted);
      return GreaterThanOrEqualTo(cs1.total, cs2.total);
    }
    
    template <class CFtype>
    bool operator>=(CFtype c1, const DefaultCostStructure<CFtype>& cs2)
    {
      if (cs2.is_weighted)
        return GreaterThanOrEqualTo((double)c1, cs2.weighted);
      return GreaterThanOrEqualTo(c1, cs2.total);
    }
    
    template <class CFtype>
    bool operator>=(const DefaultCostStructure<CFtype>& cs1, CFtype c2)
    {
      if (cs1.is_weighted)
        return GreaterThanOrEqualTo(cs1.weighted, (double)c2);
      return GreaterThanOrEqualTo(cs1.total, c2);
    }
    
    template <class CFtype, typename OtherType>
    bool operator>=(OtherType c1, const DefaultCostStructure<CFtype>& cs2)
    {
      if (cs2.is_weighted)
        return GreaterThanOrEqualTo((double)c1, cs2.weighted);
      return GreaterThanOrEqualTo((CFtype)c1, cs2.total);
    }
    
    template <class CFtype, typename OtherType>
    bool operator>=(const DefaultCostStructure<CFtype>& cs1, OtherType c2)
    {
      if (cs1.is_weighted)
        return GreaterThanOrEqualTo(cs1.weighted, (double)c2);
      return GreaterThanOrEqualTo(cs1.total, (CFtype)c2);
    }
    
    template <class CFtype>
    bool operator>(const DefaultCostStructure<CFtype>& cs1, const DefaultCostStructure<CFtype>& cs2)
    {
      if (cs1.is_weighted && cs2.is_weighted)
        return GreaterThan(cs1.weighted, cs2.weighted);
      return GreaterThan(cs1.total, cs2.total);
    }
    
    template <class CFtype>
    bool operator>(CFtype c1, const DefaultCostStructure<CFtype>& cs2)
    {
      if (cs2.is_weighted)
        return GreaterThan((double)c1, cs2.weighted);
      return GreaterThan(c1, cs2.total);
    }
    
    template <class CFtype>
    bool operator>(const DefaultCostStructure<CFtype>& cs1, CFtype c2)
    {
      if (cs1.is_weighted)
        return GreaterThan(cs1.weighted, (double)c2);
      return GreaterThan(cs1.total, c2);
    }
    
    template <class CFtype, typename OtherType>
    bool operator>(OtherType c1, const DefaultCostStructure<CFtype>& cs2)
    {
      if (cs2.is_weighted)
        return GreaterThan((double)c1, cs2.weighted);
      return GreaterThan((CFtype)c1, cs2.total);
    }
    
    template <class CFtype, typename OtherType>
    bool operator>(const DefaultCostStructure<CFtype>& cs1, OtherType c2)
    {
      if (cs1.is_weighted)
        return GreaterThan(cs1.weighted, (double)c2);
      return GreaterThan(cs1.total, (CFtype)c2);
    }
    
    template <class CFtype>
    bool operator!=(const DefaultCostStructure<CFtype>& cs1, const DefaultCostStructure<CFtype>& cs2)
    {
      if (cs1.is_weighted && cs2.is_weighted)
        return !EqualTo(cs1.weighted, cs2.weighted);
      return !EqualTo(cs1.total, cs2.total);
    }
    
    template <class CFtype>
    bool operator!=(CFtype c1, const DefaultCostStructure<CFtype>& cs2)
    {
      if (cs2.is_weighted)
        return !EqualTo((double)c1, cs2.weighted);
      return !EqualTo(c1, cs2.total);
    }
    
    template <class CFtype>
    bool operator!=(const DefaultCostStructure<CFtype>& cs1, CFtype c2)
    {
      if (cs1.is_weighted)
        return !EqualTo(cs1.weighted, (double)c2);
      return !EqualTo(cs1.total, c2);
    }
    
    template <class CFtype, typename OtherType>
    bool operator!=(OtherType c1, const DefaultCostStructure<CFtype>& cs2)
    {
      if (cs2.is_weighted)
        return !EqualTo((double)c1, cs2.weighted);
      return !EqualTo((CFtype)c1, cs2.total);
    }
    
    template <class CFtype, typename OtherType>
    bool operator!=(const DefaultCostStructure<CFtype>& cs1, OtherType c2)
    {
      if (cs1.is_weighted)
        return !EqualTo(cs1.weighted, (double)c2);
      return !EqualTo(cs1.total, (CFtype)c2);
    }
    
    template <typename CFtype>
    std::ostream& operator<<(std::ostream& os, const DefaultCostStructure<CFtype>& cc)
    {
      os << cc.total << " (viol: " << cc.violations << ", obj: " << cc.objective << ", comps: {";
      for (size_t i = 0; i < cc.all_components.size(); i++)
      {
        if (i > 0)
          os << ", ";
        os << cc.all_components[i];
      }
      os << "})";
      
      return os;
    }
    
  }
}

#endif

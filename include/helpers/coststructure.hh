#pragma once

#include "utils/types.hh"

namespace EasyLocal
{

namespace Core
{

// FIXME: it is likely that the semantics of equality and inequality in case of hybrid comparison (i.e., CostStructure against scalar) is not meaningful. Probably it can be safely removed.

template <typename T>
struct DefaultCostStructure
{
  typedef T CFtype;

  DefaultCostStructure() : total(0), violations(0), objective(0), all_components(0), weighted(0.0), is_weighted(false) {}
  DefaultCostStructure(CFtype total, CFtype violations, CFtype objective, const std::vector<CFtype> &all_components) : total(total), violations(violations), objective(objective), all_components(all_components), weighted(total), is_weighted(false) {}
  DefaultCostStructure(CFtype total, double weighted, CFtype violations, CFtype objective, const std::vector<CFtype> &all_components) : total(total), violations(violations), objective(objective), all_components(all_components), weighted(weighted), is_weighted(true) {}

  CFtype total, violations, objective;
  std::vector<CFtype> all_components;
  double weighted;

  bool is_weighted;

  DefaultCostStructure &operator+=(const DefaultCostStructure &other)
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

  DefaultCostStructure &operator-=(const DefaultCostStructure &other)
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

  const CFtype &operator[](size_t i) const
  {
    return all_components[i];
  }

  size_t size() const
  {
    return all_components.size();
  }
};

template <typename CFtype>
DefaultCostStructure<CFtype> operator+(const DefaultCostStructure<CFtype> &cs1, const DefaultCostStructure<CFtype> &cs2)
{
  DefaultCostStructure<CFtype> res = cs1;
  res += cs2;
  return res;
}

template <typename CFtype>
DefaultCostStructure<CFtype> operator-(const DefaultCostStructure<CFtype> &cs1, const DefaultCostStructure<CFtype> &cs2)
{
  DefaultCostStructure<CFtype> res = cs1;
  res -= cs2;
  return res;
}

template <class CFtype>
bool operator<(const DefaultCostStructure<CFtype> &cs1, const DefaultCostStructure<CFtype> &cs2)
{
  if (cs1.is_weighted && cs2.is_weighted)
    return LessThan(cs1.weighted, cs2.weighted);
  return LessThan(cs1.total, cs2.total);
}

template <class CFtype, typename OtherType>
bool operator<(OtherType c1, const DefaultCostStructure<CFtype> &cs2)
{
  if (cs2.is_weighted)
    return LessThan((double)c1, cs2.weighted);
  return LessThan(static_cast<CFtype>(c1), cs2.total);
}

template <class CFtype>
bool operator<(double c1, const DefaultCostStructure<CFtype> &cs2)
{
  if (cs2.is_weighted)
    return LessThan(c1, cs2.weighted);
  return LessThan(c1, static_cast<double>(cs2.total));
}

template <class CFtype, typename OtherType>
bool operator<(const DefaultCostStructure<CFtype> &cs1, OtherType c2)
{
  if (cs1.is_weighted)
    return LessThan(cs1.weighted, (double)c2);
  return LessThan(cs1.total, static_cast<CFtype>(c2));
}

template <class CFtype>
bool operator<(const DefaultCostStructure<CFtype> &cs1, double c2)
{
  if (cs1.is_weighted)
    return LessThan(cs1.weighted, c2);
  return LessThan(static_cast<double>(cs1.total), c2);
}

template <class CFtype>
bool operator<=(const DefaultCostStructure<CFtype> &cs1, const DefaultCostStructure<CFtype> &cs2)
{
  if (cs1.is_weighted && cs2.is_weighted)
    return LessThanOrEqualTo(cs1.weighted, cs2.weighted);
  return LessThanOrEqualTo(cs1.total, cs2.total);
}

template <class CFtype, typename OtherType>
bool operator<=(OtherType c1, const DefaultCostStructure<CFtype> &cs2)
{
  if (cs2.is_weighted)
    return LessThanOrEqualTo((double)c1, cs2.weighted);
  return LessThanOrEqualTo(static_cast<CFtype>(c1), cs2.total);
}

template <class CFtype>
bool operator<=(double c1, const DefaultCostStructure<CFtype> &cs2)
{
  if (cs2.is_weighted)
    return LessThanOrEqualTo(c1, cs2.weighted);
  return LessThanOrEqualTo(c1, static_cast<double>(cs2.total));
}

template <class CFtype, typename OtherType>
bool operator<=(const DefaultCostStructure<CFtype> &cs1, OtherType c2)
{
  if (cs1.is_weighted)
    return LessThanOrEqualTo(cs1.weighted, (double)c2);
  return LessThanOrEqualTo(cs1.total, static_cast<CFtype>(c2));
}

template <class CFtype>
bool operator<=(const DefaultCostStructure<CFtype> &cs1, double c2)
{
  if (cs1.is_weighted)
    return LessThanOrEqualTo(cs1.weighted, c2);
  return LessThanOrEqualTo(static_cast<double>(cs1.total), c2);
}

template <class CFtype>
bool operator==(const DefaultCostStructure<CFtype> &cs1, const DefaultCostStructure<CFtype> &cs2)
{
  if (cs1.is_weighted && cs2.is_weighted)
    return EqualTo(cs1.weighted, cs2.weighted);
  return EqualTo(cs1.total, cs2.total);
}

template <class CFtype, typename OtherType>
bool operator==(OtherType c1, const DefaultCostStructure<CFtype> &cs2)
{
  if (cs2.is_weighted)
    return EqualTo((double)c1, cs2.weighted);
  return EqualTo(static_cast<CFtype>(c1), cs2.total);
}

template <class CFtype>
bool operator==(double c1, const DefaultCostStructure<CFtype> &cs2)
{
  if (cs2.is_weighted)
    return EqualTo(c1, cs2.weighted);
  return EqualTo(c1, static_cast<double>(cs2.total));
}

template <class CFtype, typename OtherType>
bool operator==(const DefaultCostStructure<CFtype> &cs1, OtherType c2)
{
  if (cs1.is_weighted)
    return EqualTo(cs1.weighted, (double)c2);
  return EqualTo(cs1.total, static_cast<CFtype>(c2));
}

template <class CFtype, typename OtherType>
bool operator==(const DefaultCostStructure<CFtype> &cs1, double c2)
{
  if (cs1.is_weighted)
    return EqualTo(cs1.weighted, c2);
  return EqualTo(static_cast<double>(cs1.total), c2);
}

template <class CFtype>
bool operator>=(const DefaultCostStructure<CFtype> &cs1, const DefaultCostStructure<CFtype> &cs2)
{
  return !(cs1 < cs2);
}

template <class CFtype, class OtherType>
bool operator>=(OtherType c1, const DefaultCostStructure<CFtype> &cs2)
{
  return !(c1 < cs2);
}

template <class CFtype, typename OtherType>
bool operator>=(const DefaultCostStructure<CFtype> &cs1, OtherType c2)
{
  return !(cs1 < c2);
}

template <class CFtype>
bool operator>(const DefaultCostStructure<CFtype> &cs1, const DefaultCostStructure<CFtype> &cs2)
{
  return !(cs1 <= cs2);
}

template <class CFtype, typename OtherType>
bool operator>(OtherType c1, const DefaultCostStructure<CFtype> &cs2)
{
  return !(c1 <= cs2);
}

template <class CFtype, typename OtherType>
bool operator>(const DefaultCostStructure<CFtype> &cs1, OtherType c2)
{
  return !(cs1 <= c2);
}

template <class CFtype>
bool operator!=(const DefaultCostStructure<CFtype> &cs1, const DefaultCostStructure<CFtype> &cs2)
{
  return !(cs1 == cs2);
}

template <class CFtype, typename OtherType>
bool operator!=(OtherType c1, const DefaultCostStructure<CFtype> &cs2)
{
  return !(c1 == cs2);
}

template <class CFtype, typename OtherType>
bool operator!=(const DefaultCostStructure<CFtype> &cs1, OtherType c2)
{
  return !(cs1 == c2);
}

template <typename CFtype>
std::ostream &operator<<(std::ostream &os, const DefaultCostStructure<CFtype> &cc)
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

template <typename T>
struct HierarchicalCostStructure
{
  typedef T CFtype;

  HierarchicalCostStructure() : total(0), violations(0), objective(0), all_components(0), weighted(0.0), is_weighted(false) {}
  HierarchicalCostStructure(CFtype total, CFtype violations, CFtype objective, const std::vector<CFtype> &all_components) : total(total), violations(violations), objective(objective), all_components(all_components), weighted(total), is_weighted(false) {}
  HierarchicalCostStructure(CFtype total, double weighted, CFtype violations, CFtype objective, const std::vector<CFtype> &all_components) : total(total), violations(violations), objective(objective), all_components(all_components), weighted(weighted), is_weighted(true) {}

  CFtype total, violations, objective;
  std::vector<CFtype> all_components;
  double weighted;

  bool is_weighted;

  HierarchicalCostStructure &operator+=(const HierarchicalCostStructure &other)
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

  HierarchicalCostStructure &operator-=(const HierarchicalCostStructure &other)
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

  const CFtype &operator[](size_t i) const
  {
    return all_components[i];
  }

  size_t size() const
  {
    return all_components.size();
  }
};

template <typename CFtype>
HierarchicalCostStructure<CFtype> operator+(const HierarchicalCostStructure<CFtype> &cs1, const HierarchicalCostStructure<CFtype> &cs2)
{
  HierarchicalCostStructure<CFtype> res = cs1;
  res += cs2;
  return res;
}

template <typename CFtype>
HierarchicalCostStructure<CFtype> operator-(const HierarchicalCostStructure<CFtype> &cs1, const HierarchicalCostStructure<CFtype> &cs2)
{
  HierarchicalCostStructure<CFtype> res = cs1;
  res -= cs2;
  return res;
}

template <class CFtype>
bool operator<(const HierarchicalCostStructure<CFtype> &cs1, const HierarchicalCostStructure<CFtype> &cs2)
{
  // TODO: consider also the weighted case
  for (size_t i = 0; i < cs1.size(); i++)
    if (LessThan(cs1[i], cs2[i]))
      return true;
    else if (GreaterThan(cs1[i], cs2[i]))
      return false;
  return false;
}

template <class CFtype, typename OtherType>
bool operator<(OtherType c1, const HierarchicalCostStructure<CFtype> &cs2)
{
  // TODO: consider also the weighted case
  for (size_t i = 0; i < cs2.size(); i++)
    if (LessThan(static_cast<CFtype>(c1), cs2[i]))
      return true;
    else if (GreaterThan(static_cast<CFtype>(c1), cs2[i]))
      return false;
  return false;
}

template <class CFtype>
bool operator<(double c1, const HierarchicalCostStructure<CFtype> &cs2)
{
  // TODO: consider also the weighted case
  for (size_t i = 0; i < cs2.size(); i++)
    if (LessThan(c1, static_cast<double>(cs2[i])))
      return true;
    else if (GreaterThan(c1, static_cast<double>(cs2[i])))
      return false;
  return false;
}

template <class CFtype, typename OtherType>
bool operator<(const HierarchicalCostStructure<CFtype> &cs1, OtherType c2)
{
  // TODO: consider also the weighted case
  for (size_t i = 0; i < cs1.size(); i++)
    if (LessThan(cs1[i], static_cast<CFtype>(c2)))
      return true;
    else if (GreaterThan(cs1[i], static_cast<CFtype>(c2)))
      return false;
  return false;
}

template <class CFtype>
bool operator<(const HierarchicalCostStructure<CFtype> &cs1, double c2)
{
  // TODO: consider also the weighted case
  for (size_t i = 0; i < cs1.size(); i++)
    if (LessThan(static_cast<double>(cs1[i]), c2))
      return true;
    else if (GreaterThan(static_cast<double>(cs1[i]), c2))
      return false;
  return false;
}

template <class CFtype>
bool operator<=(const HierarchicalCostStructure<CFtype> &cs1, const HierarchicalCostStructure<CFtype> &cs2)
{
  // TODO: consider also the weighted case
  for (size_t i = 0; i < cs1.size(); i++)
    if (LessThan(cs1[i], cs2[i]))
      return true;
    else if (GreaterThan(cs1[i], cs2[i]))
      return false;
  return true;
}

template <class CFtype, typename OtherType>
bool operator<=(OtherType c1, const HierarchicalCostStructure<CFtype> &cs2)
{
  // TODO: consider also the weighted case
  for (size_t i = 0; i < cs2.size(); i++)
    if (LessThan(static_cast<CFtype>(c1), cs2[i]))
      return true;
    else if (GreaterThan(static_cast<CFtype>(c1), cs2[i]))
      return false;
  return true;
}

template <class CFtype>
bool operator<=(double c1, const HierarchicalCostStructure<CFtype> &cs2)
{
  // TODO: consider also the weighted case
  for (size_t i = 0; i < cs2.size(); i++)
    if (LessThan(c1, static_cast<double>(cs2[i])))
      return true;
    else if (GreaterThan(c1, static_cast<double>(cs2[i])))
      return false;
  return true;
}

template <class CFtype, typename OtherType>
bool operator<=(const HierarchicalCostStructure<CFtype> &cs1, OtherType c2)
{
  // TODO: consider also the weighted case
  for (size_t i = 0; i < cs1.size(); i++)
    if (LessThan(cs1[i], static_cast<CFtype>(c2)))
      return true;
    else if (GreaterThan(cs1[i], static_cast<CFtype>(c2)))
      return false;
  return true;
}

template <class CFtype>
bool operator<=(const HierarchicalCostStructure<CFtype> &cs1, double c2)
{
  // TODO: consider also the weighted case
  for (size_t i = 0; i < cs1.size(); i++)
    if (LessThan(static_cast<double>(cs1[i]), c2))
      return true;
    else if (GreaterThan(static_cast<double>(cs1[i]), c2))
      return false;
  return true;
}

template <class CFtype>
bool operator==(const HierarchicalCostStructure<CFtype> &cs1, const HierarchicalCostStructure<CFtype> &cs2)
{
  // TODO: consider also the weighted case
  for (size_t i = 0; i < cs1.size(); i++)
    if (!EqualTo(cs1[i], cs2[i]))
      return false;
  return true;
}

template <class CFtype, typename OtherType>
bool operator==(OtherType c1, const HierarchicalCostStructure<CFtype> &cs2)
{
  // TODO: consider also the weighted case
  for (size_t i = 0; i < cs2.size(); i++)
    if (!EqualTo(static_cast<CFtype>(c1), cs2[i]))
      return false;
  return true;
}

template <class CFtype>
bool operator==(double c1, const HierarchicalCostStructure<CFtype> &cs2)
{
  // TODO: consider also the weighted case
  for (size_t i = 0; i < cs2.size(); i++)
    if (!EqualTo(c1, static_cast<double>(cs2[i])))
      return false;
  return true;
}

template <class CFtype, typename OtherType>
bool operator==(const HierarchicalCostStructure<CFtype> &cs1, OtherType c2)
{
  // TODO: consider also the weighted case
  for (size_t i = 0; i < cs1.size(); i++)
    if (!EqualTo(cs1[i], static_cast<CFtype>(c2)))
      return false;
  return true;
}

template <class CFtype>
bool operator==(const HierarchicalCostStructure<CFtype> &cs1, double c2)
{
  // TODO: consider also the weighted case
  for (size_t i = 0; i < cs1.size(); i++)
    if (!EqualTo(static_cast<double>(cs1[i]), c2))
      return false;
  return true;
}

template <class CFtype>
bool operator>=(const HierarchicalCostStructure<CFtype> &cs1, const HierarchicalCostStructure<CFtype> &cs2)
{
  return !(cs1 < cs2);
}

template <class CFtype, class OtherType>
bool operator>=(OtherType c1, const HierarchicalCostStructure<CFtype> &cs2)
{
  return !(c1 < cs2);
}

template <class CFtype, typename OtherType>
bool operator>=(const HierarchicalCostStructure<CFtype> &cs1, OtherType c2)
{
  return !(cs1 < c2);
}

template <class CFtype>
bool operator>(const HierarchicalCostStructure<CFtype> &cs1, const HierarchicalCostStructure<CFtype> &cs2)
{
  return !(cs1 <= cs2);
}

template <class CFtype, typename OtherType>
bool operator>(OtherType c1, const HierarchicalCostStructure<CFtype> &cs2)
{
  return !(c1 <= cs2);
}

template <class CFtype, typename OtherType>
bool operator>(const HierarchicalCostStructure<CFtype> &cs1, OtherType c2)
{
  return !(cs1 <= c2);
}

template <class CFtype>
bool operator!=(const HierarchicalCostStructure<CFtype> &cs1, const HierarchicalCostStructure<CFtype> &cs2)
{
  return !(cs1 == cs2);
}

template <class CFtype, typename OtherType>
bool operator!=(OtherType c1, const HierarchicalCostStructure<CFtype> &cs2)
{
  return !(c1 == cs2);
}

template <class CFtype, typename OtherType>
bool operator!=(const HierarchicalCostStructure<CFtype> &cs1, OtherType c2)
{
  return !(cs1 == c2);
}

template <typename CFtype>
std::ostream &operator<<(std::ostream &os, const HierarchicalCostStructure<CFtype> &cc)
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
} // namespace Core
} // namespace EasyLocal

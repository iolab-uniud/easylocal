#if !defined(_COST_COMPONENT_HH_)
#define _COST_COMPONENT_HH_

#include <iostream>

/** 
 @brief The class CostComponent manages one single component of the
 cost, either hard or soft.
 
 @ingroup Helpers      
*/

template <class Input, class State, typename CFtype = int>
class CostComponent
{
public:
  void Print(std::ostream& os = std::cout) const;
  virtual CFtype ComputeCost(const State& st) const = 0;
  CFtype Cost(const State& st) const { return weight * ComputeCost(st); }
  virtual void PrintViolations(const State& st, std::ostream& os = std::cout) const = 0;
  CFtype Weight() const { return weight; }
  void SetWeight(const CFtype& w) { weight = w; }
  void SetHard() { is_hard = true; }
  void SetSoft() { is_hard = false; }
  bool IsHard() const { return is_hard; }
  bool IsSoft() const { return !is_hard; }
  const std::string name;
protected:
  CostComponent(const Input& in, const CFtype& weight, bool hard, std::string name);
  virtual ~CostComponent() {}
  const Input& in;
  CFtype weight;
  bool is_hard;
};

/*************************************************************************
 * Implementation
 *************************************************************************/
 
template <class Input, class State, typename CFtype>
CostComponent<Input,State,CFtype>::CostComponent(const Input& i, const CFtype& w, bool hard, std::string e_name)
  : name(e_name), in(i), weight(w), is_hard(hard)
{}

template <class Input, class State, typename CFtype>
void CostComponent<Input,State,CFtype>::Print(std::ostream& os) const
{ os  << "Cost Component " << name << ": weight " << weight << (is_hard ? "*" : "") << std::endl; }

#endif // _COST_COMPONENT_HH_

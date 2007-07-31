#ifndef COSTCOMPONENT_HH_
#define COSTCOMPONENT_HH_

#include "basics/EasyLocalObject.hh"

/** The class CostComponent manages one single component of the
      cost, either hard or soft 
      @ingroup Helpers      
  */

template <class Input, class State, typename CFtype = int>
class CostComponent
            : public EasyLocalObject
{
public:
    void Print(std::ostream& os = std::cout) const;
    virtual CFtype ComputeCost(const State& st) const = 0;
    CFtype Cost(const State& st) const { return weight * ComputeCost(st); }
    virtual void PrintCost(const State& st, std::ostream& os = std::cout) const;
    CFtype Weight() const { return weight; }
    void SetWeight(const CFtype& w) { weight = w; }
    bool IsHard() const { return is_hard; }
    bool IsSoft() const { return !is_hard; }
protected:
    CostComponent(const Input& in, const CFtype& weight, bool hard, std::string name = "");
    const Input& in;
    CFtype weight;
    bool is_hard;
};

/*************************************************************************
 * Implementation
 *************************************************************************/
 
template <class Input, class State, typename CFtype>
CostComponent<Input,State,CFtype>::CostComponent(const Input& i, const CFtype& w, bool hard, std::string name)
        : EasyLocalObject(name), in(i), weight(w), is_hard(hard)
{}


template <class Input, class State, typename CFtype>
void CostComponent<Input,State,CFtype>::Print(std::ostream& os) const
    { os  << "Cost Component: " << GetName() << std::endl; }

template <class Input, class State, typename CFtype>
void CostComponent<Input,State,CFtype>::PrintCost(const State& st,
        std::ostream& os) const
    { os  << "Cost Component " << GetName() << ": " << Cost(st) << std::endl; }


#endif

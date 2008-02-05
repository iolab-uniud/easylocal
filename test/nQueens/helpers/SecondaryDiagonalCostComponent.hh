#ifndef SECONDARYDIAGONALCOSTCOMPONENT_HH_
#define SECONDARYDIAGONALCOSTCOMPONENT_HH_

#include <helpers/CostComponent.hh>
#include <vector>

class SecondaryDiagonalCostComponent : public CostComponent<unsigned,std::vector<unsigned> >
  {
  public:
    SecondaryDiagonalCostComponent(unsigned& in) : CostComponent<unsigned,std::vector<unsigned> >(in,1,true, "UpRight <--> DownLeft Violations")
      { }
    int ComputeCost(const std::vector<unsigned>& st) const;
    void PrintCost(const std::vector<unsigned>& st, std::ostream& os = std::cout) const
      { os << name << ": " << Cost(st); }
  };

#endif /*SECONDARYDIAGONALCOSTCOMPONENT_HH_*/

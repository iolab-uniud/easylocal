#ifndef PRIMARYDIAGONALCOSTCOMPONENT_HH_
#define PRIMARYDIAGONALCOSTCOMPONENT_HH_

#include <helpers/CostComponent.hh>
#include <vector>

  class PrimaryDiagonalCostComponent : public CostComponent<unsigned,std::vector<unsigned> >
  {
  public:
    PrimaryDiagonalCostComponent(unsigned& in) : CostComponent<unsigned,std::vector<unsigned> >(in, 1, false, "UpLeft <--> DownRight Violations") 
      {  }
    int ComputeCost(const std::vector<unsigned>& st) const;
    void PrintCost(const std::vector<unsigned>& st, std::ostream& os = std::cout) const
      { os << name << ": " << Cost(st); }
  };
  
  

#endif /*PRIMARYDIAGONALCOSTCOMPONENT_HH_*/

#ifndef URDLCOSTCOMPONENT_HH_
#define URDLCOSTCOMPONENT_HH_

#include <helpers/CostComponent.hh>
#include <vector>

class URDLCostComponent : public CostComponent<unsigned,std::vector<unsigned> >
  {
  public:
    URDLCostComponent(unsigned& in) : CostComponent<unsigned,std::vector<unsigned> >(in,1)
      { SetName("UpRight <--> DownLeft Violations"); }
    int ComputeCost(const std::vector<unsigned>& st) const;
    void PrintCost(const std::vector<unsigned>& st, std::ostream& os = std::cout) const
      { os << GetName() << ": " << Cost(st); }
  };

#endif /*URDLCOSTCOMPONENT_HH_*/

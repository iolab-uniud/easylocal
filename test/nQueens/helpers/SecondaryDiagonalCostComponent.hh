#ifndef SECONDARYDIAGONALCOSTCOMPONENT_HH_
#define SECONDARYDIAGONALCOSTCOMPONENT_HH_

#include <helpers/CostComponent.hh>
#include <vector>

class SecondaryDiagonalCostComponent : public CostComponent<int,std::vector<int> >
  {
  public:
    SecondaryDiagonalCostComponent(int& in) : CostComponent<int,std::vector<int> >(in,1,true, "SecondaryDiagonal")
      { }
		static bool Violation(int i, int j, int ai, int aj);
    int ComputeCost(const std::vector<int>& st) const;
    void PrintViolations(const std::vector<int>& st, std::ostream& os = std::cout) const;
  };

#endif /*SECONDARYDIAGONALCOSTCOMPONENT_HH_*/

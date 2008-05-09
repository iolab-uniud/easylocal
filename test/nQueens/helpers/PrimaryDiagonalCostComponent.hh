#ifndef PRIMARYDIAGONALCOSTCOMPONENT_HH_
#define PRIMARYDIAGONALCOSTCOMPONENT_HH_

#include <helpers/CostComponent.hh>
#include <vector>

  class PrimaryDiagonalCostComponent : public CostComponent<int, std::vector<int> >
  {
  public:
    PrimaryDiagonalCostComponent(const int& in) : CostComponent<int, std::vector<int> >(in, 1, false, "PrimaryDiagonal") 
      {  }
    int ComputeCost(const std::vector<int>& st) const;
		static bool Violation(int i, int j, int ai, int aj);
    void PrintViolations(const std::vector<int>& st, std::ostream& os = std::cout) const;
  };
  
  

#endif /*PRIMARYDIAGONALCOSTCOMPONENT_HH_*/

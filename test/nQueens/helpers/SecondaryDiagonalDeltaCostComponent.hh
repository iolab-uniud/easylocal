#ifndef SECONDARYDIAGONALDELTACOSTCOMPONENT_HH_
#define SECONDARYDIAGONALDELTACOSTCOMPONENT_HH_

#include <helpers/DeltaCostComponent.hh>
#include <vector>
#include "../data/Swap.hh"
#include "SecondaryDiagonalCostComponent.hh"

class SecondaryDiagonalDeltaCostComponent : public FilledDeltaCostComponent<int, std::vector<int>, Swap>
{
public:
  SecondaryDiagonalDeltaCostComponent(const int& in, SecondaryDiagonalCostComponent& cc) 
    : FilledDeltaCostComponent<int, std::vector<int>, Swap>(in,cc,"Secondary diagonal")
{ }
	int ComputeDeltaCost(const std::vector<int>& st, const Swap& sw) const;
};

#endif /*SECONDARYDIAGONALDELTACOSTCOMPONENT_HH_*/

#ifndef SECONDARYDIAGONALDELTACOSTCOMPONENT_HH_
#define SECONDARYDIAGONALDELTACOSTCOMPONENT_HH_

#include <helpers/DeltaCostComponent.hh>
#include <vector>
#include "../data/Swap.hh"
#include "SecondaryDiagonalCostComponent.hh"

class SecondaryDiagonalDeltaCostComponent : public FilledDeltaCostComponent<unsigned,std::vector<unsigned>,Swap>
{
public:
  SecondaryDiagonalDeltaCostComponent(const unsigned& in, SecondaryDiagonalCostComponent& cc, bool shifted = false) 
    : FilledDeltaCostComponent<unsigned,std::vector<unsigned>,Swap>(in,cc,"Secondary diagonal",shifted)
{ }
	int ComputeDeltaCost(const std::vector<unsigned>& st, const Swap& sw) const;
};

#endif /*SECONDARYDIAGONALDELTACOSTCOMPONENT_HH_*/

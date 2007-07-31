#ifndef PRIMARYDIAGONALDELTACOSTCOMPONENT_HH_
#define PRIMARYDIAGONALDELTACOSTCOMPONENT_HH_

#include <helpers/DeltaCostComponent.hh>
#include "../data/Swap.hh"
#include "PrimaryDiagonalCostComponent.hh"

class PrimaryDiagonalDeltaCostComponent
            : public FilledDeltaCostComponent<unsigned,std::vector<unsigned>,Swap>
{
public:
  PrimaryDiagonalDeltaCostComponent(const unsigned& in, PrimaryDiagonalCostComponent& cc, bool shifted = false)
    : FilledDeltaCostComponent<unsigned,std::vector<unsigned>,Swap>(in,cc,"ULDR",shifted)
    {}
    int ComputeDeltaCost(const std::vector<unsigned>& st, const Swap& sw) const;
};

#endif /*PRIMARYDIAGONALDELTACOSTCOMPONENT_HH_*/

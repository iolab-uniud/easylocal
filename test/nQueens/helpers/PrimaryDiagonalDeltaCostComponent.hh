#if !defined(_PRIMARYDIAGONALDELTACOSTCOMPONENT_HH_)
#define _PRIMARYDIAGONALDELTACOSTCOMPONENT_HH_

#include <helpers/DeltaCostComponent.hh>
#include "../data/Swap.hh"
#include "PrimaryDiagonalCostComponent.hh"

class PrimaryDiagonalDeltaCostComponent
            : public FilledDeltaCostComponent<int, std::vector<int>, Swap>
{
public:
  PrimaryDiagonalDeltaCostComponent(const int& in, PrimaryDiagonalCostComponent& cc)
    : FilledDeltaCostComponent<int, std::vector<int>, Swap>(in, cc, "PrimaryDiagonal")
    {}
    int ComputeDeltaCost(const std::vector<int>& st, const Swap& sw) const;
};

#endif /*_PRIMARYDIAGONALDELTACOSTCOMPONENT_HH_*/

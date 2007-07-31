#ifndef URDLDELTACOSTCOMPONENT_HH_
#define URDLDELTACOSTCOMPONENT_HH_

#include <helpers/DeltaCostComponent.hh>
#include <vector>
#include "../data/Swap.hh"
#include "URDLCostComponent.hh"

class URDLDeltaCostComponent : public DeltaCostComponent<unsigned,std::vector<unsigned>,Swap>
{
public:
  URDLDeltaCostComponent(const unsigned& in, URDLCostComponent& cc, bool shifted = false) 
    : DeltaCostComponent<unsigned,std::vector<unsigned>,Swap>(in,cc,"URDL",shifted)
{ }
	int DeltaCost(const std::vector<unsigned>& st, const Swap& sw) const;
};

#endif /*URDLDELTACOSTCOMPONENT_HH_*/

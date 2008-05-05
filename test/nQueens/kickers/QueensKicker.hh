#ifndef QUEENSKICKER_HH_
#define QUEENSKICKER_HH_

#include <kickers/SimpleKicker.hh>
#include <vector>
#include "../data/Swap.hh"

class QueensKicker : public SimpleKicker<unsigned,std::vector<unsigned>,Swap>
{
public:
	QueensKicker(const unsigned& bs, SwapNeighborhoodExplorer& qnhe, unsigned s = 2)
	: SimpleKicker<unsigned,std::vector<unsigned>,Swap>(bs, qnhe, s, "QueensKicker") 
{}
	bool RelatedMoves(const Swap&, const Swap&) const
{ return true; } 
};  	

#endif /*QUEENSKICKER_HH_*/

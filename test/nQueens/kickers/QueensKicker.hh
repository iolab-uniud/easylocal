#ifndef QUEENSKICKER_HH_
#define QUEENSKICKER_HH_

#include <kickers/SimpleKicker.hh>
#include <vector>
#include "../data/Swap.hh"

class QueensKicker : public SimpleKicker<int,std::vector<int>,Swap>
{
public:
	QueensKicker(const int& bs, SwapNeighborhoodExplorer& qnhe, int s = 2)
	: SimpleKicker<int,std::vector<int>,Swap>(bs, qnhe, s, "QueensKicker") 
{}
	bool RelatedMoves(const Swap&, const Swap&) const
{ return true; } 
};  	

#endif /*QUEENSKICKER_HH_*/

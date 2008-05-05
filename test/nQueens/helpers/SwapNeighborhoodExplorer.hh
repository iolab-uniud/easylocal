#ifndef SwapNeighborhoodExplorer_HH_
#define SwapNeighborhoodExplorer_HH_

#include <helpers/NeighborhoodExplorer.hh>
#include "../data/Swap.hh"
#include "QueensStateManager.hh"

/** This class implements a possible neighborhood explorartion strategy
for the Swap move. */
class SwapNeighborhoodExplorer
:  public NeighborhoodExplorer<unsigned,std::vector<unsigned>,Swap>
{
public:
	SwapNeighborhoodExplorer(const unsigned& in, QueensStateManager& qsm, std::string name = "")
	: NeighborhoodExplorer<unsigned,std::vector<unsigned>,Swap>(in, qsm, name)
  {}
	void RandomMove(const std::vector<unsigned> &a, Swap& sw);
	void MakeMove(std::vector<unsigned> &a, const Swap& sw);
	bool FeasibleMove(const std::vector<unsigned> &a, const Swap& sw);
protected:
    void NextMove(const std::vector<unsigned> &a, Swap& sw);
};

#endif /*SwapNeighborhoodExplorer_H_*/

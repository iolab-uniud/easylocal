#ifndef SwapNeighborhoodExplorer_HH_
#define SwapNeighborhoodExplorer_HH_

#include <helpers/NeighborhoodExplorer.hh>
#include "../data/Swap.hh"
#include "QueensStateManager.hh"

/** This class implements a possible neighborhood explorartion strategy
for the Swap move. */
class SwapNeighborhoodExplorer
:  public NeighborhoodExplorer<int,std::vector<int>,Swap>
{
public:
	SwapNeighborhoodExplorer(const int& in, QueensStateManager& qsm, std::string name = "")
	: NeighborhoodExplorer<int,std::vector<int>,Swap>(in, qsm, name)
  {}
	void RandomMove(const std::vector<int> &a, Swap& sw);
	void MakeMove(std::vector<int> &a, const Swap& sw);
	bool FeasibleMove(const std::vector<int> &a, const Swap& sw);
protected:
    void NextMove(const std::vector<int> &a, Swap& sw);
};

#endif /*SwapNeighborhoodExplorer_H_*/

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
	void MakeMove(std::vector<int> &a, const Swap& sw) const;
  void RandomMove(const std::vector<int> &a, Swap& sw) const;
  void FirstMove(const std::vector<int> &a, Swap& sw) const;
  bool NextMove(const std::vector<int> &a, Swap& sw) const;
};

#endif /*SwapNeighborhoodExplorer_H_*/

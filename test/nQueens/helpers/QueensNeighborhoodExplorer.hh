#ifndef QUEENSNEIGHBORHOODEXPLORER_HH_
#define QUEENSNEIGHBORHOODEXPLORER_HH_

#include <helpers/NeighborhoodExplorer.hh>
#include "../data/Swap.hh"
#include "QueensStateManager.hh"

/** This class implements a possible neighborhood explorartion strategy
     for the Swap move. */
class QueensNeighborhoodExplorer
            :  public NeighborhoodExplorer<unsigned,std::vector<unsigned>,Swap>
{
public:
    QueensNeighborhoodExplorer(const unsigned& in, QueensStateManager& qsm)
            : NeighborhoodExplorer<unsigned,std::vector<unsigned>,Swap>(in, qsm)
    {}
    void RandomMove(const std::vector<unsigned> &a, Swap& sw);
    void MakeMove(std::vector<unsigned> &a, const Swap& sw);
    bool FeasibleMove(const std::vector<unsigned> &a, const Swap& sw);
protected:
    void NextMove(const std::vector<unsigned> &a, Swap& sw);
};

#endif /*QUEENSNEIGHBORHOODEXPLORER_H_*/

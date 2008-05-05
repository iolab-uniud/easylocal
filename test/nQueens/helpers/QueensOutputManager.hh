#ifndef QUEENSOUTPUTMANAGER_HH_
#define QUEENSOUTPUTMANAGER_HH_

#include <helpers/OutputManager.hh>
#include "../data/ChessBoard.hh"
#include "QueensStateManager.hh"

/** This class handles the output objects, and translate among them
     and the corresponding state objects. */
class QueensOutputManager
            : public OutputManager<unsigned, ChessBoard, std::vector<unsigned> >
{
public:
  QueensOutputManager(const unsigned& bs)
  : OutputManager<unsigned,ChessBoard,std::vector<unsigned> >(bs, "QueensOutputManager") {}
    void OutputState(const std::vector<unsigned> &a, ChessBoard& cb) const;
    void InputState(std::vector<unsigned> &a, const ChessBoard& cb) const;
};

#endif /*QUEENSOUTPUTMANAGER_HH_*/

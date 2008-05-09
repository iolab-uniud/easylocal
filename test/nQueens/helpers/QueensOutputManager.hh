#ifndef QUEENSOUTPUTMANAGER_HH_
#define QUEENSOUTPUTMANAGER_HH_

#include <helpers/OutputManager.hh>
#include "../data/ChessBoard.hh"
#include "QueensStateManager.hh"

/** This class handles the output objects, and translate among them
     and the corresponding state objects. */
class QueensOutputManager
            : public OutputManager<int, ChessBoard, std::vector<int> >
{
public:
  QueensOutputManager(const int& bs)
  : OutputManager<int,ChessBoard,std::vector<int> >(bs, "QueensOutputManager") {}
    void OutputState(const std::vector<int> &a, ChessBoard& cb) const;
    void InputState(std::vector<int> &a, const ChessBoard& cb) const;
};

#endif /*QUEENSOUTPUTMANAGER_HH_*/

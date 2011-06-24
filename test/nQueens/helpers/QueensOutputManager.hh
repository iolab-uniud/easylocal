// $Id$
// This file is part of EasyLocalpp: a C++ Object-Oriented framework
// aimed at easing the development of Local Search algorithms.
// Copyright (C) 2001--2011 Andrea Schaerf, Luca Di Gaspero. 
//
// EasyLocalpp is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// EasyLocalpp is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with EasyLocalpp. If not, see <http://www.gnu.org/licenses/>.

#if !defined(_QUEENSOUTPUTMANAGER_HH_)
#define _QUEENSOUTPUTMANAGER_HH_

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

#endif /*_QUEENSOUTPUTMANAGER_HH_*/

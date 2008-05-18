// $Id$
// This file is part of EasyLocalpp: a C++ Object-Oriented framework
// aimed at easing the development of Local Search algorithms.
// Copyright (C) 2001--2008 Andrea Schaerf, Luca Di Gaspero. 
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

#if !defined(_CHESSBOARD_HH_)
#define _CHESSBOARD_HH_

#include <iostream>
#include <vector>

/** This class instantiates template Output.
     It handles a matrix of character which represent a chessboard.
     A queen is placed in position (i,j) if the cell (i,j) contains
     the character Q, otherwise the cell is free. 
     @ingroup basic
 */
class ChessBoard
{
    friend std::ostream& operator<<(std::ostream&, const ChessBoard &);
    friend std::istream& operator>>(std::istream&, ChessBoard &);
public:
    ChessBoard(const int& bs);
    char operator()(int i, int j) const;
    void SetSquare(int i, int j, char ch); 
		
    void Clean();

    // used for doublechecking the correctness of the solvers
    int CountAttacks();
    int CountSingleAttacks(int h, int k);

private:
		std::vector<std::vector<char> > cb;
};

#endif /*_CHESSBOARD_HH_*/

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

#include "ChessBoard.hh"

ChessBoard::ChessBoard(const int& bs)
  : cb(bs, std::vector<char>(bs, '-'))
{}

char ChessBoard::operator()(int i, int j) const 
{ return cb[i][j]; }

void ChessBoard::SetSquare(int i, int j, char ch) 
{ cb[i][j] = ch; }

void ChessBoard::Clean()
{
    for (int i = 0; i < (int)cb.size(); i++)
        for (int j = 0; j < (int)cb.size(); j++)
            cb[i][j] = '-';
}

std::ostream& operator<<(std::ostream& os, const ChessBoard& board)
{
    for (int i = 0; i < (int)board.cb.size(); i++)
    {
        for (int j = 0; j < (int)board.cb.size(); j++)
            os << board.cb[i][j];
        os << std::endl;
    }
    return os;
}

std::istream& operator>>(std::istream& is, ChessBoard& board)
{
    for (int i = 0; i < (int)board.cb.size(); i++)
        for (int j = 0; j < (int)board.cb.size(); j++)
            is >> board.cb[i][j];
    return is;
}

int ChessBoard::CountAttacks()
{
    int attacks = 0;
    for (int i = 0; i < (int)cb.size(); i++)
        for (int j = 0; j < (int)cb.size(); j++)
            if (cb[i][j] == 'Q')
                attacks += CountSingleAttacks(i,j);
    return attacks/2;
}

int ChessBoard::CountSingleAttacks(int h, int k)
{
	int attacks = 0, l;
    for (int i = 0; i < (int)cb.size(); i++)
        if (i != h && cb[i][k] == 'Q')
            attacks++;
    for (int j = 0; j < (int)cb.size(); j++)
        if (j != k && cb[h][j] == 'Q')
            attacks++;
    for (l = -(int)cb.size(); l < (int)cb.size(); l++)
        if (h + l >= 0 && h + l < (int)cb.size()
                && k + l >= 0 && k + l < (int)cb.size()
                && l != 0 && cb[h+l][k+l] == 'Q')
            attacks++;
    for (l = -(int)cb.size(); l < (int)cb.size(); l++)
        if (h + l >= 0 && h + l < (int)cb.size()
                && k - l >= 0 && k - l < (int)cb.size()
                && l != 0 && cb[h+l][k-l] == 'Q')
            attacks++;
    return attacks;
}

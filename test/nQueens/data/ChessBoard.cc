#include "ChessBoard.hh"

ChessBoard::ChessBoard(const unsigned&bs)
  : cb(bs, std::vector<char>(bs, '-'))
{}

char ChessBoard::operator()(int i, int j) const 
{ return cb[i][j]; }

void ChessBoard::SetSquare(int i, int j, char ch) 
{ cb[i][j] = ch; }

void ChessBoard::Clean()
{
    for (int i = 0; i < cb.size(); i++)
        for (int j = 0; j < cb.size(); j++)
            cb[i][j] = '-';
}

std::ostream& operator<<(std::ostream& os, const ChessBoard& board)
{
    for (int i = 0; i < board.cb.size(); i++)
    {
        for (int j = 0; j < board.cb.size(); j++)
            os << board.cb[i][j];
        os << std::endl;
    }
    return os;
}

std::istream& operator>>(std::istream& is, ChessBoard& board)
{
    for (int i = 0; i < board.cb.size(); i++)
        for (int j = 0; j < board.cb.size(); j++)
            is >> board.cb[i][j];
    return is;
}

int ChessBoard::CountAttacks()
{
    int attacks = 0;
    for (int i = 0; i < cb.size(); i++)
        for (int j = 0; j < cb.size(); j++)
            if (cb[i][j] == 'Q')
                attacks += CountSingleAttacks(i,j);
    return attacks/2;
}

int ChessBoard::CountSingleAttacks(int h, int k)
{
    int attacks = 0, l;
    for (int i = 0; i < cb.size(); i++)
        if (i != h && cb[i][k] == 'Q')
            attacks++;
    for (int j = 0; j < cb.size(); j++)
        if (j != k && cb[h][j] == 'Q')
            attacks++;
    for (l = -cb.size(); l < cb.size(); l++)
        if (h + l >= 0 && h + l < cb.size()
                && k + l >= 0 && k + l < cb.size()
                && l != 0 && cb[h+l][k+l] == 'Q')
            attacks++;
    for (l = -cb.size(); l < cb.size(); l++)
        if (h + l >= 0 && h + l < cb.size()
                && k - l >= 0 && k - l < cb.size()
                && l != 0 && cb[h+l][k-l] == 'Q')
            attacks++;
    return attacks;
}

#ifndef CHESSBOARD_HH_
#define CHESSBOARD_HH_

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
    ChessBoard(const unsigned&bs);
    char operator()(int i, int j) const;
    void SetSquare(int i, int j, char ch); 
		
    void Clean();

    // used for doublechecking the correctness of the solvers
    int CountAttacks();
    int CountSingleAttacks(int h, int k);

private:
		std::vector<std::vector<char> > cb;
};

#endif /*CHESSBOARD_HH_*/

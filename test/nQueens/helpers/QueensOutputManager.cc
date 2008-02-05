#include "QueensOutputManager.hh"

void QueensOutputManager::OutputState(const std::vector<unsigned> &a, ChessBoard& cb) const
{
  unsigned int i, j;
  for (i = 0; i < in; i++)
    for (j = 0; j < in; j++)
      cb.SetSquare(i,j,'-');
  for (i = 0; i < in; i++)
    cb.SetSquare(i,a[i],'Q');
}

void QueensOutputManager::InputState(std::vector<unsigned> &a, const ChessBoard& cb) const
{
  for (unsigned int i = 0; i < in; i++)
    for (unsigned int j = 0; j < in; j++)
      if (cb(i,j) == 'Q')
			{
				a[i] = j;
				break;
			}
}

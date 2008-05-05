#include "QueensStateManager.hh"
#include <utils/Random.hh>
#include <vector>
#include <stdexcept>

QueensStateManager::QueensStateManager(const unsigned& bs)
: StateManager<unsigned,std::vector<unsigned> >(bs, "QueensStateManager")
{}

void QueensStateManager::RandomState(std::vector<unsigned> &a)
{
	std::vector<bool> tag(in, false);
	unsigned int i, j;
	
	for (j = 0; j < in; j++)
	{ 
		do
			i = Random::Int(0, in - 1);
		while (tag[i]);
		tag[i] = true;
		a[j] = i;
	}
}

bool QueensStateManager::CheckConsistency(const std::vector<unsigned> &a) const
{
  std::vector<bool> tag(in, false);
  for (unsigned int j = 0; j < in; j++)
    {
      if (a[j] >= in)
	throw std::runtime_error("State is not consistent (queen out of the chessboard)");
      if (tag[a[j]])
	throw std::runtime_error("State is not consistent (queens do not form a permutation)");
      tag[a[j]] = true;
    }
  return true;
}

std::ostream& operator<<(std::ostream& os, const std::vector<unsigned>& a)
{
	for (unsigned i = 0; i < a.size(); i++)
		os << a[i] << ' ';
	os << std::endl;
	
	return os;
}

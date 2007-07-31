#include "QueensStateManager.hh"
#include <utils/Random.hh>
#include <vector>

QueensStateManager::QueensStateManager(const unsigned& bs)
: StateManager<unsigned,std::vector<unsigned> >(bs)
{}

void QueensStateManager::RandomState(std::vector<unsigned> &a)
{
	std::vector<bool> tag(in);
	int i, j;
	
	for (j = 0; j < in; j++)
		tag[j] = false;

	for (j = 0; j < in; j++)
	{ 
		do
			i = Random::Int(0, in - 1);
		while (tag[i]);
		tag[i] = true;
		a[j] = i;
	}
}

std::ostream& operator<<(std::ostream& os, const std::vector<unsigned>& a)
{
	for (unsigned i = 0; i < a.size(); i++)
		os << a[i] << ' ';
	os << std::endl;
	
	return os;
}

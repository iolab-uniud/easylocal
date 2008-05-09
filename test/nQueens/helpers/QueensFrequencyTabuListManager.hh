#ifndef QUEENSFREQUENCYTABULISTMANAGER_HH_
#define QUEENSFREQUENCYTABULISTMANAGER_HH_

#include <helpers/TabuListManager.hh>
#include <vector>
#include "../data/Swap.hh"

class QueensFrequencyTabuListManager : public FrequencyTabuListManager<std::vector<int>, Swap>
{
public:
	QueensFrequencyTabuListManager(int min = 0, int max = 0) 
	: FrequencyTabuListManager<std::vector<int>, Swap>(min,max) {}
protected:
	bool Inverse(const Swap& m1, const Swap& m2) const; 
};

bool QueensFrequencyTabuListManager::Inverse(const Swap& m1, const Swap& m2) const
{ return m1 == m2; }


#endif /*QUEENSFREQUENCYTABULISTMANAGER_HH_*/

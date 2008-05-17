#if !defined(_QUEENSTABULISTMANAGER_HH_)
#define _QUEENSTABULISTMANAGER_HH_

#include <helpers/TabuListManager.hh>
#include <vector>
#include "../data/Swap.hh"

/** This class handles the tabu list. */
class QueensTabuListManager : public TabuListManager<std::vector<int>, Swap>
{
/* public:
	QueensTabuListManager() 
	: TabuListManager<std::vector<int>, Swap>() {} */
protected:
	bool Inverse(const Swap& m1, const Swap& m2) const { return m1 == m2; }
};

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


#endif /*_QUEENSTABULISTMANAGER_HH_*/

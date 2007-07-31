#ifndef QUEENSTABULISTMANAGER_HH_
#define QUEENSTABULISTMANAGER_HH_

#include <helpers/TabuListManager.hh>
#include <vector>
#include "../data/Swap.hh"

/** This class handles the tabu list. */
class QueensTabuListManager : public TabuListManager<std::vector<unsigned>, Swap>
{
public:
	QueensTabuListManager(int min = 0, int max = 0) 
	: TabuListManager<std::vector<unsigned>, Swap>(min,max) {}
protected:
	bool Inverse(const Swap& m1, const Swap& m2) const { return m1 == m2; }
};

#endif /*QUEENSTABULISTMANAGER_HH_*/

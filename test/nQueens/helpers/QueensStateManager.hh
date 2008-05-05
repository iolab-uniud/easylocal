#ifndef QUEENSSTATEMANAGER_H_
#define QUEENSSTATEMANAGER_H_

#include <iostream>
#include <helpers/StateManager.hh>

/** This class is the state manager for the problem. */
class QueensStateManager
            : public StateManager<unsigned,std::vector<unsigned> >
{
public:
    QueensStateManager(const unsigned& bs);
    void RandomState(std::vector<unsigned> &a);
	void CheckConsistency(const std::vector<unsigned> &a) const;
};

std::ostream& operator<<(std::ostream& os, const std::vector<unsigned>& a);


#endif /*QUEENSSTATEMANAGER_H_*/

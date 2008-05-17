#if !defined(_QUEENSSTATEMANAGER_H_)
#define _QUEENSSTATEMANAGER_H_

#include <iostream>
#include <helpers/StateManager.hh>

/** This class is the state manager for the problem. */
class QueensStateManager
            : public StateManager<int,std::vector<int> >
{
public:
    QueensStateManager(const int& bs);
    void RandomState(std::vector<int> &a);
  bool CheckConsistency(const std::vector<int> &a) const;
};

std::ostream& operator<<(std::ostream& os, const std::vector<int>& a);


#endif /*_QUEENSSTATEMANAGER_H_*/

#ifndef FREQUENCYTABULISTMANAGER_HH_
#define FREQUENCYTABULISTMANAGER_HH_

#include "../../helpers/TabuListManager.hh"
#include <map>

template <class State, class Move, typename CFtype = int>
class FrequencyTabuListManager
            : public TabuListManager<State, Move>
{
public:
    void Print(std::ostream& os = std::cout) const;
    void InsertMove(const State& st, const Move& mv, double mv_cost, double curr, double best);
    bool ProhibitedMove(const State& st, const Move& mv, double mv_cost) const;
    void Clean();
protected:
    FrequencyTabuListManager(unsigned int min = 0, unsigned int max = 0,
                             double thr = 0.04, unsigned int min_it = 100);
    typedef std::map<Move,unsigned long> MapType;
    MapType frequency_map;
    double threshold;
    unsigned int min_iter;
};

/*************************************************************************
 * Implementation
 *************************************************************************/


template <class State, class Move, typename CFtype>
void FrequencyTabuListManager<State,Move,CFtype>::Print(std::ostream& os) const
{
    TabuListManager<State,Move,CFtype>::Print(os);
    os << "Number of iterations: " << this->iter << std::endl;
    for (typename MapType::const_iterator mv_i = frequency_map.begin(), mv_end = frequency_map.end(); mv_i != mv_end; mv_i++)
        os << "Move : " << mv_i->first << ", frequency : "
        << mv_i->second << " (" << mv_i->second/(double)this->iter << "); "
        << std::endl;
}

template <class State, class Move, typename CFtype>
void FrequencyTabuListManager<State,Move,CFtype>::InsertMove(const State& st, const Move& mv, double mv_cost, double curr, double best)
{
    TabuListManager<State,Move,CFtype>::InsertMove(st, mv,mv_cost,curr,best);
    if (frequency_map.find(mv) != frequency_map.end())
        frequency_map[mv]++;
    else
        frequency_map[mv] = 1;
}

template <class State, class Move, typename CFtype>
FrequencyTabuListManager<State,Move,CFtype>::FrequencyTabuListManager(unsigned int min,
        unsigned int max,
        double thr,
        unsigned int min_it)
        : TabuListManager<State,Move,CFtype>(min,max), threshold(thr), min_iter(min_it)
{}

template <class State, class Move, typename CFtype>
bool FrequencyTabuListManager<State,Move,CFtype>::ProhibitedMove(const State& st, const Move& mv, double mv_cost) const
{
    if (Aspiration(st, mv,mv_cost))
        return false;
    if (ListMember(mv))
        return true;
    else if (this->iter > min_iter)
    {
        typename MapType::const_iterator it = frequency_map.find(mv);
        if (it != frequency_map.end() && it->second/double(this->iter) > threshold)
            return true;
    }
    return false;
}

/**
    Cleans the data: deletes all the elements of the tabu list.
*/
template <class State, class Move, typename CFtype>
void FrequencyTabuListManager<State,Move,CFtype>::Clean()
{
    TabuListManager<State,Move,CFtype>::Clean();
    frequency_map.clear();
}

#endif /*FREQUENCYTABULISTMANAGER_HH_*/

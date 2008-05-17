#if !defined(PROHIBITIONMANAGER_HH_)
#define PROHIBITIONMANAGER_HH_

/** The Prohibition Manager deals with move prohibition mechanisms
     that prevents cycling and allows for diversification.  
     
     This class is at the top of the hierarchy: we have also a more
     specific prohibition manager, which maintains a list of @c Move
     elements according to the prohibition mechanisms of tabu search.  
     @ingroup Helpers
  */
template <class State, class Move, typename CFtype = int>
class ProhibitionManager
{
public:
    /** Marks a given move as prohibited, according to the prohibition
    strategy.
    @param mv the move
    @param mv_cost the cost of the move
    @param curr the cost of the current solution
    @param best the cost of the best solution found so far  */
    virtual void InsertMove(const State& st, const Move& mv, const CFtype& mv_cost, const CFtype& curr,
                            const CFtype& best) = 0;
    /** Checks whether the given move is prohibited, according to the
    prohibition strategy.
    @param st the state
    @param mv the move
    @param mv_cost the cost of the move
    @param curr the cost of the current solution
    @param best the cost of the best solution found so far
    @return true if the move is prohibited in the given state, false otherwise */
    virtual bool ProhibitedMove(const State& st, const Move& mv, const CFtype& mv_cost) const = 0;
    /** Resets the prohibition manager mechanisms. */
    virtual void Clean() = 0;
    virtual void UpdateIteration() = 0;
protected:
      ProhibitionManager();
    virtual ~ProhibitionManager() {}
  const std::string name;
};

template <class State, class Move, typename CFtype>
ProhibitionManager<State,Move,CFtype>::ProhibitionManager() 
{}

#endif /*PROHIBITIONMANAGER_HH_*/

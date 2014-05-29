#if !defined(_TABU_LIST_MANAGER_HH_)
#define _TABU_LIST_MANAGER_HH_

#include <list>
#include <map>
#include <sstream>

#include "easylocal/helpers/prohibitionmanager.hh"
#include "easylocal/utils/random.hh"
#include "easylocal/utils/types.hh"

namespace EasyLocal {

  namespace Core {

    template <class State, class Move, typename CFtype>
    class TabuListManager;

    template <class State, class Move, typename CFtype>
    class FrequencyTabuListManager;

    /** The class for a @c Move item in the Tabu List.
    It is simply a compound data made up of the @c Move itself and the
    iteration at which the element shall leave the list.
    */
    template <class State, class Move, typename CFtype>
    class TabuListItem
    {
      friend class TabuListManager<State, Move,CFtype>;
      friend class FrequencyTabuListManager<State, Move,CFtype>;
    public:
      /** Creates a tabu list item constituted by a move
      and the leaving iteration passed as parameters.
      @param mv the move to insert into the list
      @param out the iteration at which the move leaves the list.
      */
      TabuListItem(Move mv, unsigned long int out)
        : elem(mv), out_iter(out)
      {
        
      }
      virtual ~TabuListItem() {}


    protected:
      Move elem;              /**< The move stored in the list item. */
      unsigned long int out_iter; /**< iteration at which the element
        leaves the list */
    };

    /** The Tabu List Manager handles a list of @c Move elements according
    to the prohibition mechanisms of tabu search.
    Namely it maintains an item in the list for a number of iterations
    that varies randomly in a given range.
    Each time a new @c Move is inserted in the list, the ones which their
    iteration count has expired are removed.
    @ingroup Helpers
    */
    template <class State, class Move, typename CFtype>
    class TabuListManager
      : public ProhibitionManager<State,Move,CFtype>
    {
    public:
      typedef Move ThisMove;

      void Print(std::ostream& os = std::cout) const;
      virtual void InsertMove(const State& st, const Move& mv, const CFtype& mv_cost, const CFtype& curr, const CFtype& best);
      virtual bool ProhibitedMove(const State& st, const Move& mv, const CFtype& mv_cost) const;
      /** Sets the length of the tabu list to be comprised in the range
      [min, max].
      @param min the minimum tabu tenure
      @param max the maximum tabu tenure */
      void SetLength(unsigned int min, unsigned int max);
  
      virtual void Clean();
      /** Returns the minimum number of iterations a move is considered tabu.
      @return the minimum tabu tenure */
      unsigned int MinTenure() const
        { return min_tenure; }
      /** Returns the maximum number of iterations a move is considered tabu.
      @return the maximum tabu tenure */
      unsigned int MaxTenure() const
        { return max_tenure; }
      /** Verifies whether a move is the inverse of another one. Namely it
      tests whether mv1 is the inverse of mv2 (that will be an element of
      the tabu list).
      @note @bf To be implemented in the application.
      @param mv1 the move to be tested
      @param mv2 the move used for comparison  */
      virtual bool Inverse(const Move& mv1, const Move& mv2) const = 0;
      void UpdateIteration() { PurgeList(); iter++; }
      unsigned ListLength() const { return tlist.size(); }
      TabuListManager(unsigned int min_tenure, unsigned int max_tenure, std::string);
      TabuListManager(std::string);
      /** Virtual destructor. */
      virtual ~TabuListManager();
      virtual std::string StatusString() const;

    protected:
      TabuListManager() : TabuListManager("TabuList") {}
      virtual bool Aspiration(const State& st, const Move&, const CFtype& mv_cost) const;
      virtual void InsertIntoList(const State& st, const Move& mv);
      void PurgeList();
      /** Updates the function associated with the aspiration criterion.
      For default it does nothing.
      @param mv_cost the cost of the move
      @param curr the cost of the current solution
      @param best the cost of the best solution found so far */
      void UpdateAspirationFunction(const CFtype& curr_cost, const CFtype& best_cost)
        { current_state_cost = curr_cost; best_state_cost = best_cost; }
      bool ListMember(const Move&) const;
      // parameters
      Parameter<unsigned int> min_tenure; /**< The minimum tenure of the tabu list. */
      Parameter<unsigned int> max_tenure;  /**< The maximum tenure of the tabu list. */
      unsigned long int iter; /**< The current iteration. */
      std::list<TabuListItem<State,Move,CFtype> > tlist; /**< The list of tabu moves. */
      CFtype current_state_cost; /**< The cost of current state of the attached runner (for the aspiration criterion) */
      CFtype best_state_cost; /**< The cost of best state of the attached runner (for the aspiration criterion) */
    };

    template <class State, class Move, typename CFtype>
    class FrequencyTabuListManager
      : public TabuListManager<State, Move, CFtype>
    {
    public:
      void Print(std::ostream& os = std::cout) const;
      virtual void InsertMove(const State& st, const Move& mv, const CFtype& mv_cost, const CFtype& curr, const CFtype& best);
      virtual bool ProhibitedMove(const State& st, const Move& mv, const CFtype& mv_cost) const;
      virtual void Clean();
    protected:
      FrequencyTabuListManager(double thr = 0.04, unsigned int min_it = 100);
      typedef std::map<Move,unsigned long int> MapType;
      MapType frequency_map;
      double threshold;
      unsigned int min_iter;
    };

    /*************************************************************************
    * Implementation
    *************************************************************************/
    /**
    Constructs a tabu list manager object which manages a list of
    the given tenure (i.e., the number of steps a move is considered tabu).
 
    @param min the minimum tabu tenure
    @param max the maximum tabu tenure
    */
    template <class State, class Move, typename CFtype>
    TabuListManager<State, Move,CFtype>::TabuListManager(std::string name)
      : ProhibitionManager<State,Move,CFtype>(name, "List of moves which cannot be done"),
    min_tenure("min_tabu_tenure", "Minimum length of the tabu list", this->parameters),
    max_tenure("max_tabu_tenure", "Maximum length of the tabu list", this->parameters),
    iter(0)
    {
      min_tenure = 0;
      max_tenure = 1;
    }

    template <class State, class Move, typename CFtype>
    TabuListManager<State, Move,CFtype>::TabuListManager(unsigned int min_t, unsigned int max_t, std::string name)
      : TabuListManager(name)
    {
      min_tenure = min_t;
      max_tenure = max_t;
    }

    template <class State, class Move, typename CFtype>
    void TabuListManager<State, Move,CFtype>::SetLength(unsigned int min, unsigned int max)
    {
      min_tenure = min;
      max_tenure = max;
    }

    template <class State, class Move, typename CFtype>
    TabuListManager<State, Move,CFtype>::~TabuListManager()
    {
        
    }

    /**
    Inserts the move in the tabu list and updates the aspiration function.
 
    @param mv the move to add
    @param mv_cost the move cost
    @param best the best state cost found so far
    */
    template <class State, class Move, typename CFtype>
    void TabuListManager<State, Move,CFtype>::InsertMove(const State& st, const Move& mv, const CFtype& mv_cost, const CFtype& curr,
    const CFtype& best)
    {
      InsertIntoList(st, mv);
      UpdateAspirationFunction(curr,best);
    }

    /**
    Checks whether the given move is prohibited.
 
    @param mv the move to check
    @param mv_cost the move cost
    @param curr the current state cost
    @param best the best state cost found so far
    @return true if the move mv is prohibited, false otherwise
    */
    template <class State, class Move, typename CFtype>
    bool TabuListManager<State, Move,CFtype>::ProhibitedMove(const State& st, const Move& mv, const CFtype& mv_cost) const
    {
      return !Aspiration(st, mv, mv_cost) && ListMember(mv);
    }

    /**
    Cleans the data: deletes all the elements of the tabu list.
    */
    template <class State, class Move, typename CFtype>
    void TabuListManager<State, Move,CFtype>::Clean()
    {
      tlist.clear();
      iter = 0;
    }

    /**
    Checks whether the inverse of a given move belongs to the tabu list.
 
    @param mv the move to check
    @return true if the inverse of the move belongs to the tabu list,
    false otherwise
    */
    template <class State, class Move, typename CFtype>
    bool TabuListManager<State, Move,CFtype>::ListMember(const Move& mv) const
    {
      typename std::list<TabuListItem<State,Move,CFtype> >::const_iterator p = tlist.begin();
      while (p != tlist.end())
      {
        if (Inverse(mv, p->elem))
          return true;
        else
          p++;
      }
      return false;
    }

    /**
    Prints the current status of the tabu list on an output stream.
 
    @param os the output stream
    @param tl the tabu list manager to output
    */
    template <class State, class Move, typename CFtype>
    void TabuListManager<State, Move,CFtype>::Print(std::ostream& os) const
    {
      os <<  "Tabu List Manager: " << this->name << std::endl;
      os <<  "  Tenure: " << min_tenure << " - " << max_tenure << std::endl;
      typename std::list<TabuListItem<State,Move,CFtype> >::const_iterator p = tlist.begin();
      while (p != tlist.end())
      {
        os << "  " << p->elem << " (" << p->out_iter - iter << ")" << std::endl;
        p++;
      }
    }

    template <class State, class Move, typename CFtype>
    std::string TabuListManager<State, Move, CFtype>::StatusString() const 
    {
      std::stringstream ss;
      ss << min_tenure << " < " << tlist.size() << " < " << max_tenure;
      return ss.str();
    }

    /**
    Checks whether the aspiration criterion is satisfied for a given move.
    By default, it verifies if the move cost applied to the current state
    gives a value lower than the best state cost found so far.
 
    @param mv the move
    @param mv_cost the move cost
    @param curr the cost of the current state
    @param best the cost of the best state found so far
    @return true if the aspiration criterion is satisfied, false otherwise
    */
    template <class State, class Move, typename CFtype>
    bool TabuListManager<State, Move,CFtype>::Aspiration(const State& st, const Move&, const CFtype& mv_cost) const
    {
      return LessThan<CFtype>(current_state_cost + mv_cost, best_state_cost);
    }

    /**
    Inserts the move into the tabu list, and update the list removing
    the moves for which the tenure has elapsed.
 
    @param mv the move to add
    */
    template <class State, class Move, typename CFtype>
    void TabuListManager<State, Move,CFtype>::InsertIntoList(const State& st, const Move& mv)
    {
      unsigned int tenure = (unsigned int)Random::Int(min_tenure, max_tenure);
      TabuListItem<State,Move,CFtype> li(mv, iter + tenure);
      tlist.push_front(li);
      UpdateIteration();
    }

    /**
    Inserts the move into the tabu list, and update the list removing
    the moves for which the tenure has elapsed.
 
    @param mv the move to add
    */
    template <class State, class Move, typename CFtype>
    void TabuListManager<State, Move,CFtype>::PurgeList()
    {
      typename std::list<TabuListItem<State,Move,CFtype> >::iterator p = tlist.begin();
      while (p != tlist.end())
        if (p->out_iter <= iter) // Note: it must be "<=" and not "==" because for bimodal runners
          // this function is not invoked at every iteration (and thus there
          // are old moves to be removed)
          p = tlist.erase(p);
      else
        p++;
    }
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
    void FrequencyTabuListManager<State,Move,CFtype>::InsertMove(const State& st, const Move& mv, const CFtype& mv_cost, const CFtype& curr, const CFtype& best)
    {
      TabuListManager<State,Move,CFtype>::InsertMove(st, mv,mv_cost,curr,best);
      if (frequency_map.find(mv) != frequency_map.end())
        frequency_map[mv]++;
      else
        frequency_map[mv] = 1;
    }

    template <class State, class Move, typename CFtype>
    FrequencyTabuListManager<State,Move,CFtype>::FrequencyTabuListManager(double thr,
    unsigned int min_it)
      : TabuListManager<State,Move,CFtype>(), threshold(thr), min_iter(min_it)
    {
          
    }

    template <class State, class Move, typename CFtype>
    bool FrequencyTabuListManager<State,Move,CFtype>::ProhibitedMove(const State& st, const Move& mv, const CFtype& mv_cost) const
    {
      if (this->Aspiration(st, mv,mv_cost))
        return false;
      if (this->ListMember(mv))
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
  }
}

#endif // _TABU_LIST_MANAGER_HH_

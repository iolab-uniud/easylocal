#if !defined(_NEIGHBORHOOD_EXPLORER_HH_)
#define _NEIGHBORHOOD_EXPLORER_HH_

#include "helpers/DeltaCostComponent.hh"
#include "helpers/StateManager.hh"
#include "utils/Random.hh"

#include <typeinfo>
#include <iostream>
#include <stdexcept>
#include <memory>

namespace EasyLocal {

  namespace Core {
        
    class EmptyNeighborhood : public std::logic_error
    {
    public:
      EmptyNeighborhood() : std::logic_error("Empty neighborhood") {}
    };

    /** The Neighborhood Explorer is responsible for the strategy
    exploited in the exploration of the neighborhood, and for
    computing the variations of the cost function due to a specific
    @ref Move.
 
    @ingroup Helpers
    */
    template <class Input, class State, class Move, typename CFtype>
    class NeighborhoodExplorer
    {
    public:
      typedef Move ThisMove;
  
      /** Prints the configuration of the object (attached cost components)
      @param os Output stream
      */
      void Print(std::ostream& os = std::cout) const;
  
      /**
      Generates a random move in the neighborhood of a given state.
      @note To be implemented in the application (MustDef)
      @param st the start state
      @param mv the generated move
      */
      virtual void RandomMove(const State &st, Move& mv) const throw (EmptyNeighborhood) = 0;
  
      /** Generates the first move in the neighborhood (a total ordering
      of the neighborhood is assumed). It is always used on
      cooperation with @ref NextMove to generate the whole
      neighborhood. It returns @c void because it is assumed that at
      least a move exists in the neighborhood.  It writes the first
      move in @c mv.
   
      @note To be implemented in the application (MustDef)
      @param st the start state
      @param mv the move
      */
      virtual void FirstMove(const State& st, Move& mv) const throw (EmptyNeighborhood) = 0;
  
      /** Generates the move that follows mv in the exploration of the
      neighborhood of the state st.
      It returns the generated move in the same variable mv.
      @return @c false if @c mv is the last in the neighborhood of the state.
   
      @note To be implemented in the application.
      @param st the start state
      @param mv the move
      */
      virtual bool NextMove(const State &st, Move& mv) const = 0;
  
      /**
      Generate the first improvingt move in the exploration of the neighborhood
      of a given state. It uses @ref FirstMove and @ref NextMove
      @param st the start state
      @param mv the generated move
      @throws EmptyNeighborhood when the State st has no neighbor
      */
      virtual CFtype FirstImprovingMove(const State& st, Move& mv) const throw (EmptyNeighborhood);
    
      /**
      Generates the best move in the full exploration of the neighborhood
      of a given state. It uses @ref FirstMove and @ref NextMove
      @param st the start state.
      @param mv the generated move.
      @return the variation of the cost due to the Move mv.
      @throws EmptyNeighborhood when the State st has no neighbor
      */
      virtual CFtype BestMove(const State& st, Move& mv) const throw (EmptyNeighborhood);
  
      /**
      Generates the best move in a random sample exploration of the neighborhood
      of a given state.
      @param st the start state.
      @param mv the generated move.
      @param samples the number of sampled neighbors
      @return the variation of the cost due to the Move mv.
      @throws EmptyNeighborhood when the State st has no neighbor
      */
      virtual CFtype SampleMove(const State &st, Move& mv, unsigned int samples) const throw (EmptyNeighborhood);
  
      /**
      States whether a move is feasible or not in a given state.
      By default it considers all the moves as feasible, but it can
      be overwritten by the user.
   
      @param st the start state
      @param @c mv the move checked for feasibility
      @return @c true if the move @mv is feasible in @c st, false otherwise
      */
      virtual bool FeasibleMove(const State& st, const Move& mv) const
      {
        return true;
      }
  
      /**
      Modifies the state passed as parameter by applying a given
      move upon it.
   
      @note To be implemented in the application (MustDef)
      @param st the state to modify
      @param mv the move to be applied
      */
      virtual void MakeMove(State &st, const Move& mv) const = 0;
  
    
      virtual bool NextRelatedMove(const State &st, Move& mv, const Move& mv2) const
        { return NextMove(st,mv); }
  
      virtual bool FirstRelatedMove(const State &st, Move& mv, const Move& mv2) const
      {
        try
        {
          FirstMove(st,mv);
        }
        catch (EmptyNeighborhood e)
        {
          return false;
        }
        return true;
      } 
  
      // evaluation function
  
      /**
      Computes the differences in the cost function obtained by applying the move @c mv 
      to the state @c st.
   
      @param st the state to modify
      @param mv the move to be applied
      @return the difference in the cost function
      */
      virtual CFtype DeltaCostFunction(const State& st, const Move& mv) const;
  
      /**
      Computes the differences in the cost function obtained by applying the move @c mv
      to the state @c st and returns the unaggregated value as a vector of components.
   
      @param st the state to modify
      @param mv the move to be applied
      @return the difference in the cost function for each cost component
      */
      virtual std::vector<CFtype> DeltaCostFunctionComponents(const State& st, const Move& mv) const;
  
      /**
      Computes the differences in the objective component of the cost function obtained 
      by applying the move @c mv to the state @c st.
   
      @param st the state to modify
      @param mv the move to be applied
      @return the difference in the objective function
      */ 
      virtual CFtype DeltaObjective(const State& st, const Move & mv) const;
      /**
      Computes the differences in the violations component (i.e., hard constraints)
      of the cost function obtained by applying the move @c mv to the state @c st.
   
      @param st the state to modify
      @param mv the move to be applied
      @return the difference in the violation function
      */
 
      virtual CFtype DeltaViolations(const State& st, const Move & mv) const;
  
      /**
      Adds a delta cost component to the neighborhood explorer, which is responsible for computing
      one component of the cost function.
      A delta cost component requires the implementation of a way to compute the difference in the
      cost function without simulating the move on a given state.
   
      @param dcc a delta cost component object
      */
      virtual void AddDeltaCostComponent(DeltaCostComponent<Input,State,Move,CFtype>& dcc);
  
      /**
      Adds a cost component to the neighborhood explorer, which is responsible for computing
      one component of the cost function.
      A cost component passed to the neighborhood explorer will compute the difference in the
      cost function due to that component as the difference between the cost in the current state
      and the new state obtained after (actually) performing the move. It will be wrapped into a delta cost component
      by means of an adapter and it might be seen as an unimplemented delta cost component.
      @note In general it is a quite unefficient way to compute the contribution of the move and
      it should be avoided, if possible.
   
      @param cc a cost component object
      */
 
      virtual void AddCostComponent(CostComponent<Input,State,CFtype>& cc);
  
      /**
      Returns the number of delta cost components attached to the neighborhood explorer.
      @return the size of the delta cost components vector
      */
      virtual size_t DeltaCostComponents() const
        { return delta_hard_cost_components.size() + delta_soft_cost_components.size(); }
  
      /**
      Returns an element of the delta cost component vector attached to the neighborhood explorer.
      @param i the index of the required delta cost component
      @return the delta cost component of index i
      */
      virtual DeltaCostComponent<Input,State,Move,CFtype>& GetDeltaCostComponent(unsigned int i)
      {
        if (i < delta_hard_cost_components.size())
          return *delta_hard_cost_components[i];
        else if (i < delta_hard_cost_components.size() + delta_soft_cost_components.size())
          return *delta_soft_cost_components[i - delta_hard_cost_components.size()];
        else
          throw std::logic_error("GetDeltaCostComponent: index out of bounds");
      }
  
      /**
      Returns the number of delta cost components attached to the neighborhood explorer which are hard cost components.
      @return the size of the hard delta cost components vector
      */
      virtual size_t DeltaHardCostComponents() const
        { return delta_hard_cost_components.size(); }
  
      /**
      Returns an element of the delta hard cost component vector attached to the neighborhood explorer.
      @param i the index of the required delta cost component
      @return the delta cost component of index i
      */
      virtual DeltaCostComponent<Input,State,Move,CFtype>& GetDeltaHardCostComponent(unsigned int i)
      {
        if (i < delta_hard_cost_components.size())
          return *delta_hard_cost_components[i];
        else
          throw std::logic_error("GetDeltaHardCostComponent: index out of bounds");
      }
  
      /**
      Returns the number of delta cost components attached to the neighborhood explorer which are soft cost components.
      @return the size of the hard delta cost components vector
      */
      virtual size_t DeltaSoftCostComponents() const
        { return delta_soft_cost_components.size(); }
  
      /**
      Returns an element of the delta soft cost component vector attached to the neighborhood explorer.
      @param i the index of the required delta cost component
      @return the delta cost component of index i
      */
      virtual DeltaCostComponent<Input,State,Move,CFtype>& GetDeltaSoftCostComponent(unsigned int i)
      {
        if (i < delta_soft_cost_components.size())
          return *delta_soft_cost_components[i];
        else
          throw std::logic_error("GetDeltaSoftCostComponent: index out of bounds");
      }
  
      /**
      Retuns the modality of the neighborhood explorer, i.e., the number of different kind of
      moves handled by it.
      @return the modality of the neighborhood explorer
      */
      virtual unsigned int Modality() const
        { return 1; }
  
      /**
      Returns the modality of the move passed as parameter, i.e., the number of move components
      in a composite move that are active.
      @param mv a move
      @return the modality of the move
      */
      virtual unsigned int MoveModality(const Move& mv) const
        { return 0; }
    protected:
      /**
      Constructs a neighborhood explorer passing a pointer to a state manager
      and a pointer to the input.
   
      @param in a pointer to an input object.
      @param sm a pointer to a compatible state manager.
      @param name the name associated to the NeighborhoodExplorer.
      */
      NeighborhoodExplorer(const Input& in, StateManager<Input,State,CFtype>& sm, std::string name);
      virtual ~NeighborhoodExplorer() {}
  
      const Input& in;/**< A reference to the input */
      StateManager<Input, State,CFtype>& sm; /**< A reference to the attached state manager. */
  
      /** Lists of delta cost components (or adapters) */
      std::vector<DeltaCostComponent<Input,State,Move,CFtype>* > delta_hard_cost_components, delta_soft_cost_components;
  
      /** List of created adapters (to be automatically deleted in the destructor). */
      std::vector<std::unique_ptr<DeltaCostComponentAdapter<Input, State, Move, CFtype>>> dcc_adapters;
  
      /** Name of user-defined neighborhood explorer */
      std::string name;
  
      /** States whether there are unimplemented delta cost components attached */
      bool unimplemented_hard_components, unimplemented_soft_components;
    };

    /*************************************************************************
    * Implementation
    *************************************************************************/


    template <class Input, class State, class Move, typename CFtype>
    NeighborhoodExplorer<Input,State,Move,CFtype>::NeighborhoodExplorer(const Input& i,
    StateManager<Input,State,CFtype>& e_sm, std::string e_name)
      : in(i), sm(e_sm), name(e_name), unimplemented_hard_components(false), unimplemented_soft_components(false)
    {
      
    }

    /**
    Evaluates the variation of the cost function obtainted either by applying the move to
    the given state or simulating it.
    The tentative definition computes a weighted sum of the variation of
    the violations function and of the difference in the objective function.
 
    @param st the start state
    @param mv the move
    @return the variation in the cost function
    */
    template <class Input, class State, class Move, typename CFtype>
    CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::DeltaCostFunction(const State& st, const Move & mv) const
    {
      CFtype delta_hard_cost = 0, delta_soft_cost = 0;
  
      // compute delta costs (of implemented delta cost components)
      for (DeltaCostComponent<Input,State,Move,CFtype>* dcc : delta_hard_cost_components)
        if (dcc->IsDeltaImplemented())
          delta_hard_cost += dcc->DeltaCost(st, mv);
      for (DeltaCostComponent<Input,State,Move,CFtype>* dcc : delta_soft_cost_components)
        if (dcc->IsDeltaImplemented())
          delta_soft_cost += dcc->DeltaCost(st, mv);
  
      // only if there is at least one unimplemented delta cost component (i.e., a wrapper along a cost component)
      if (unimplemented_hard_components || unimplemented_soft_components)
      {
        // compute move
        State new_st = st;
        MakeMove(new_st, mv);
    
        if (unimplemented_hard_components)
          for (DeltaCostComponent<Input,State,Move,CFtype>* dcc : delta_hard_cost_components)
            if (!dcc->IsDeltaImplemented())
        {
          // get reference to cost component
          CostComponent<Input,State,CFtype>& cc = dcc->GetCostComponent();
          delta_hard_cost +=  cc.Weight() * (cc.ComputeCost(new_st) - cc.ComputeCost(st));
        }
        if (unimplemented_soft_components)
          for (DeltaCostComponent<Input,State,Move,CFtype>* dcc : delta_soft_cost_components)
            if (!dcc->IsDeltaImplemented())
        {
          // get reference to cost component
          CostComponent<Input,State,CFtype>& cc = dcc->GetCostComponent();
          delta_soft_cost +=  cc.Weight() * (cc.ComputeCost(new_st) - cc.ComputeCost(st));
        }
      }
  
      return HARD_WEIGHT * delta_hard_cost + delta_soft_cost;
    }

    template <class Input, class State, class Move, typename CFtype>
    std::vector<CFtype> NeighborhoodExplorer<Input,State,Move,CFtype>::DeltaCostFunctionComponents(const State& st, const Move & mv) const
    {
      std::vector<CFtype> delta_cost_function(delta_hard_cost_components.size() + delta_soft_cost_components.size(), (CFtype)0);

      for (unsigned int i = 0; i < delta_hard_cost_components.size(); i++)
      {
        DeltaCostComponent<Input, State, Move, CFtype>* dcc = delta_hard_cost_components[i];
        if (dcc->IsDeltaImplemented())
          delta_cost_function[i] = dcc->DeltaCost(st, mv);
      }
      for (unsigned int i = 0; i < delta_soft_cost_components.size(); i++)
      {
        DeltaCostComponent<Input, State, Move, CFtype>* dcc = delta_soft_cost_components[i];
        if (dcc->IsDeltaImplemented())
          delta_cost_function[i + delta_hard_cost_components.size()] = dcc->DeltaCost(st, mv);
      }

      // only if there is at least one unimplemented delta cost component (i.e., a wrapper along a cost component)
      if (unimplemented_hard_components || unimplemented_soft_components)
      {
        // compute move
        State new_st = st;
        MakeMove(new_st, mv);
    
        if (unimplemented_hard_components)
          for (unsigned int i = 0; i < delta_hard_cost_components.size(); i++)
        {
          DeltaCostComponent<Input, State, Move, CFtype>* dcc = delta_hard_cost_components[i];
          if (!dcc->IsDeltaImplemented())
          {
            // get reference to cost component
            CostComponent<Input,State,CFtype>& cc = dcc->GetCostComponent();
            delta_cost_function[i] =  cc.Weight() * (cc.ComputeCost(new_st) - cc.ComputeCost(st));
          }
        }
        if (unimplemented_soft_components)
          for (unsigned int i = 0; i < delta_soft_cost_components.size(); i++)
        {
          DeltaCostComponent<Input, State, Move, CFtype>* dcc = delta_soft_cost_components[i];
          if (!dcc->IsDeltaImplemented())
          {
            // get reference to cost component
            CostComponent<Input,State,CFtype>& cc = dcc->GetCostComponent();
            delta_cost_function[i + delta_hard_cost_components.size()] =  cc.Weight() * (cc.ComputeCost(new_st) - cc.ComputeCost(st));
          }
        }
      }
  
      return delta_cost_function;
    }

    template <class Input, class State, class Move, typename CFtype>
    CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::DeltaViolations(const State& st, const Move & mv) const
    {
      CFtype delta_hard_cost = 0;
  
      // compute delta costs (of implemented delta cost components)
      for (DeltaCostComponent<Input,State,Move,CFtype>* dcc : delta_hard_cost_components)
        if (dcc->IsDeltaImplemented())
          delta_hard_cost += dcc->DeltaCost(st, mv);
  
      // only if there is at least one unimplemented delta cost component (i.e., a wrapper along a cost component)
      if (unimplemented_hard_components)
      {
        // compute move
        State new_st = st;
        MakeMove(new_st, mv);
    
        for (DeltaCostComponent<Input,State,Move,CFtype>* dcc : delta_hard_cost_components)
          if (!dcc->IsDeltaImplemented())
        {
          // get reference to cost component
          CostComponent<Input,State,CFtype>& cc = dcc->GetCostComponent();
          delta_hard_cost +=  cc.Weight() * (cc.ComputeCost(new_st) - cc.ComputeCost(st));
        }
      }
  
      return delta_hard_cost;
    }

    template <class Input, class State, class Move, typename CFtype>
    CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::DeltaObjective(const State& st, const Move & mv) const
    {
      CFtype delta_soft_cost = 0;
  
      // compute delta costs (of implemented delta cost components)
      for (DeltaCostComponent<Input,State,Move,CFtype>* dcc : delta_soft_cost_components)
        if (dcc->IsDeltaImplemented())
          delta_soft_cost += dcc->DeltaCost(st, mv);
  
      // only if there is at least one unimplemented delta cost component (i.e., a wrapper along a cost component)
      if (unimplemented_soft_components)
      {
        // compute move
        State new_st = st;
        MakeMove(new_st, mv);
    
        for (DeltaCostComponent<Input,State,Move,CFtype>* dcc : delta_soft_cost_components)
          if (!dcc->IsDeltaImplemented())
        {
          // get reference to cost component
          CostComponent<Input,State,CFtype>& cc = dcc->GetCostComponent();
          delta_soft_cost +=  cc.Weight() * (cc.ComputeCost(new_st) - cc.ComputeCost(st));
        }
      }
  
      return delta_soft_cost;
    }

    template <class Input, class State, class Move, typename CFtype>
    void NeighborhoodExplorer<Input,State,Move,CFtype>::AddDeltaCostComponent(DeltaCostComponent<Input,State,Move,CFtype>& dcc)
    {
      if (dcc.IsHard())
        delta_hard_cost_components.push_back(&dcc);
      else
        delta_soft_cost_components.push_back(&dcc);
    }

    template <class Input, class State, class Move, typename CFtype>
    void NeighborhoodExplorer<Input,State,Move,CFtype>::AddCostComponent(CostComponent<Input,State,CFtype>& cc)
    {

      dcc_adapters.push_back(std::unique_ptr<DeltaCostComponentAdapter<Input, State, Move, CFtype>>(new DeltaCostComponentAdapter<Input, State, Move, CFtype>(in, cc, *this)));
      if (cc.IsHard())
      {
        unimplemented_hard_components = true;
        delta_hard_cost_components.push_back(dcc_adapters[dcc_adapters.size() - 1].get());
      }
      else
      {
        unimplemented_soft_components = true;
        delta_soft_cost_components.push_back(dcc_adapters[dcc_adapters.size() - 1].get());
      }
    }

    template <class Input, class State, class Move, typename CFtype>
    CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::BestMove(const State &st, Move& mv) const throw (EmptyNeighborhood)
    {
      unsigned int number_of_bests = 1; // number of moves found with the same best value

      FirstMove(st, mv);
      Move best_move = mv;
      CFtype mv_cost = DeltaCostFunction(st, mv);
      CFtype best_delta = mv_cost;
  
      while (NextMove(st, mv))
      {
        mv_cost = DeltaCostFunction(st, mv);
        if (LessThan(mv_cost, best_delta))
        {
          best_move = mv;
          best_delta = mv_cost;
          number_of_bests = 1;
        }
        else if (EqualTo(mv_cost, best_delta))
        {
          if (Random::Int(0, number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
            best_move = mv;
          number_of_bests++;
        }
      }
  
      mv = best_move;
      return best_delta;
    }

    template <class Input, class State, class Move, typename CFtype>
    CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::FirstImprovingMove(const State &st, Move& mv) const throw (EmptyNeighborhood)
    {
      unsigned int number_of_bests = 0;
      FirstMove(st, mv);
      CFtype mv_cost = DeltaCostFunction(st, mv);
      Move best_move = mv;
      CFtype best_delta = mv_cost;
  
      while (NextMove(st, mv))
      {
        mv_cost = DeltaCostFunction(st, mv);
        if (LessThan(mv_cost, (CFtype)0))
          return mv_cost; // mv is an improving move
    
        if (LessThan(mv_cost, best_delta))
        {
          best_move = mv;
          best_delta = mv_cost;
          number_of_bests = 1;
        }
        else if (EqualTo(mv_cost, best_delta))
        {
          if (Random::Int(0, number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
            best_move = mv;
          number_of_bests++;
        }
      }
  
      // these instructions are reached when no improving move has been found
      mv = best_move;
      return best_delta;
    }

    template <class Input, class State, class Move, typename CFtype>
    CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::SampleMove(const State &st, Move& mv, unsigned int samples) const throw (EmptyNeighborhood)
    {
      unsigned int number_of_bests = 0;
      unsigned int s = 1;
      CFtype mv_cost;
  
      RandomMove(st, mv);
      mv_cost = DeltaCostFunction(st, mv);
      Move best_move = mv;
      CFtype best_delta = mv_cost;
	
      do
      {
        if (LessThan(mv_cost, best_delta))
        {
          best_move = mv;
          best_delta = mv_cost;
          number_of_bests = 1;
        }
        else if (EqualTo(mv_cost, best_delta))
        {
          if (Random::Int(0, number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
            best_move = mv;
          number_of_bests++;
        }
        RandomMove(st, mv);
        mv_cost = DeltaCostFunction(st, mv);
        s++;
      }
      while (s < samples);
  
      mv = best_move;
      return best_delta;
    }     
  }
}

#endif // _NEIGHBORHOOD_EXPLORER_HH_

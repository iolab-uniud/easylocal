#if !defined(_NEIGHBORHOOD_EXPLORER_HH_)
#define _NEIGHBORHOOD_EXPLORER_HH_

#include <typeinfo>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <iterator>

#include "easylocal/helpers/deltacostcomponent.hh"
#include "easylocal/helpers/statemanager.hh"
#include "easylocal/utils/random.hh"

namespace EasyLocal {
  
  namespace Core {    
    
    /** Exception raised when the neighborhood is empty. */
    class EmptyNeighborhood : public std::logic_error
    {
    public:
      EmptyNeighborhood() : std::logic_error("Empty neighborhood") {}
    };    
    
    template <class Move, typename CFtype>
    struct EvaluatedMove
    {
      static EvaluatedMove empty;
      EvaluatedMove() : is_valid(false) {}
      EvaluatedMove(const Move& move) : move(move), is_valid(false) {}
      EvaluatedMove(const Move& move, CostStructure<CFtype> cost) : is_valid(true), move(move), cost(cost) {}
      
      Move move;
      bool is_valid;
      CostStructure<CFtype> cost;
    };
    
    template <class Move, typename CFtype>
    std::ostream& operator<<(std::ostream& os, const EvaluatedMove<Move, CFtype>& em)
    {
      os << em.move;
      if (em.is_valid)
        os << " " << em.cost;
      else
        os << " not_valid";
      return os;
    }
    
    template <class Move, typename CFtype>
    EvaluatedMove<Move, CFtype> EvaluatedMove<Move, CFtype>::empty = EvaluatedMove<Move, CFtype>();
    
    /** The Neighborhood Explorer is responsible for the strategy exploited in the exploration of the neighborhood, and for computing the variations of the cost function due to a specific
     @ref Move.
     @ingroup Helpers
     */
    template <class Input, class State, class Move, typename CFtype = int>
    class NeighborhoodExplorer
    {
    public:
      typedef Input InputType;
      typedef Move MoveType;
      typedef State StateType;
      typedef CFtype CostType;
      
      typedef typename std::function<bool(const Move& mv, CostStructure<CFtype> move_cost)> MoveAcceptor;
      
      /** Checks if a move in the neighborhood is legal.
       @note Can be implemented in the application (MayRedef)
       @param st the start state
       @param mv the move
       */
      virtual bool FeasibleMove(const State &st, const Move& mv) const
      { return true; }
      
      /** Generates a random move in the neighborhood of a given state.
       @note To be implemented in the application (MustDef)
       @param st the start state
       @param mv the generated move
       */
      virtual void RandomMove(const State &st, Move& mv) const throw (EmptyNeighborhood) = 0;
      
      /** Generates the first move in the neighborhood (a total ordering of the neighborhood is assumed). It is always used on cooperation with @ref NextMove to generate the whole neighborhood. It returns @c void because it is assumed that at least a move exists in the neighborhood.  It writes the first move in @c mv.
       @note To be implemented in the application (MustDef)
       @param st the start state
       @param mv the move
       */
      virtual void FirstMove(const State& st, Move& mv) const throw (EmptyNeighborhood) = 0;
      
      /** Generates the move that follows mv in the exploration of the neighborhood of the state st. It returns the generated move in the same variable mv.
       @return @c false if @c mv is the last in the neighborhood of the state.
       @note To be implemented in the application.
       @param st the start state
       @param mv the move
       */
      virtual bool NextMove(const State &st, Move& mv) const = 0;
      
      /** Modifies the state passed as parameter by applying a given move upon it.
       @note To be implemented in the application (MustDef)
       @param st the state to modify
       @param mv the move to be applied
       */
      virtual void MakeMove(State &st, const Move& mv) const = 0;
      
      /** Computes the differences in the cost function obtained by applying the move @c mv to the state @c st and returns the unaggregated value as a vector of components.
       @param st the state to modify
       @param mv the move to be applied
       @return the difference in the cost function for each cost component
       */
      virtual CostStructure<CFtype> DeltaCostFunctionComponents(const State& st, const Move& mv, const std::vector<double>& weights = std::vector<double>(0)) const;
      
      /** Adds a delta cost component to the neighborhood explorer, which is responsible for computing one component of the cost function. A delta cost component requires the implementation of a way to compute the difference in the cost function without simulating the move on a given state.
       @param dcc a delta cost component object
       */
      virtual void AddDeltaCostComponent(DeltaCostComponent<Input, State, Move, CFtype>& dcc);
      
      /** Adds a cost component to the neighborhood explorer, which is responsible for computing one component of the cost function. A cost component passed to the neighborhood explorer will compute the difference in the cost function due to that component as the difference between the cost in the current state and the new state obtained after (actually) performing the move. It will be wrapped into a delta cost component by means of an adapter and it might be seen as an unimplemented delta cost component.
       @note In general it is a quite unefficient way to compute the contribution of the move and it should be avoided, if possible.
       @param cc a cost component object
       */
      
      virtual void AddCostComponent(CostComponent<Input, State, CFtype>& cc);
      
      /** Returns the number of delta cost components attached to the neighborhood explorer.
       @return the size of the delta cost components vector
       */
      virtual size_t DeltaCostComponents() const
      {
        return delta_hard_cost_components.size() + delta_soft_cost_components.size();
      }
      
      /** Retuns the modality of the neighborhood explorer, i.e., the number of different kind of moves handled by it.
       @return the modality of the neighborhood explorer
       */
      virtual size_t Modality() const
      { return 1; }
      
      /**
       Constructs a neighborhood explorer passing a n input object and a state manager.
       
       @param in a pointer to an input object.
       @param sm a pointer to a compatible state manager.
       @param name the name associated to the NeighborhoodExplorer.
       */
      NeighborhoodExplorer(const Input& in, StateManager<Input, State, CFtype>& sm, std::string name);
      
      virtual ~NeighborhoodExplorer() {}
      
      /**
       This method will select the first move in the exhaustive neighborhood exploration that
       matches with the criterion expressed by the functional object bool f(const Move& mv, CFtype cost)
       */
      virtual EvaluatedMove<Move, CFtype> SelectFirst(const State& st, size_t& explored, const MoveAcceptor& AcceptMove, const std::vector<double>& weights = std::vector<double>(0)) const throw (EmptyNeighborhood);
      
      /**
       This method will select the best move in the exhaustive neighborhood exploration that
       matches with the criterion expressed by the functional object bool f(const Move& mv, CFtype cost)
       */
      virtual EvaluatedMove<Move, CFtype> SelectBest(const State& st, size_t& explored, const MoveAcceptor& AcceptMove, const std::vector<double>& weights = std::vector<double>(0)) const throw (EmptyNeighborhood);
      
      /**
       This method will select the first move in a random neighborhood exploration that
       matches with the criterion expressed by the functional object bool f(const Move& mv, CFtype cost)
       */
      virtual EvaluatedMove<Move, CFtype> RandomFirst(const State& st, size_t samples, size_t& explored, const MoveAcceptor& AcceptMove, const std::vector<double>& weights = std::vector<double>(0)) const throw (EmptyNeighborhood);
      
      /**
       This method will select the best move in a random neighborhood exploration that
       matches with the criterion expressed by the functional object bool f(const Move& mv, CFtype cost)
       */
      virtual EvaluatedMove<Move, CFtype> RandomBest(const State& st, size_t samples, size_t& explored, const MoveAcceptor& AcceptMove, const std::vector<double>& weights = std::vector<double>(0)) const throw (EmptyNeighborhood);
      
    protected:
      
      const Input& in;/**< A reference to the input */
      StateManager<Input, State, CFtype>& sm; /**< A reference to the attached state manager. */
      
      /** Lists of delta cost components (or adapters) */
      std::vector<DeltaCostComponent<Input, State, Move, CFtype>* > delta_hard_cost_components, delta_soft_cost_components;
      
      /** List of created adapters (to be automatically deleted in the destructor). */
      std::vector<std::unique_ptr<DeltaCostComponentAdapter<Input, State, Move, CFtype>>> dcc_adapters;
      
      /** Name of user-defined neighborhood explorer */
      std::string name;
      
      /** States whether there are unimplemented delta cost components attached */
      bool unimplemented_hard_components, unimplemented_soft_components;
    };
    
    /** IMPLEMENTATION */
    
    template <class Input, class State, class Move, typename CFtype>
    NeighborhoodExplorer<Input, State, Move, CFtype>::NeighborhoodExplorer(const Input& i, StateManager<Input, State, CFtype>& e_sm, std::string e_name)
    : in(i), sm(e_sm), name(e_name), unimplemented_hard_components(false), unimplemented_soft_components(false)
    {}
    
    /** Evaluates the variation of the cost function obtainted either by applying the move to the given state or simulating it. The tentative definition computes a weighted sum of the variation of the violations function and of the difference in the objective function.
     @param st the start state
     @param mv the move
     @return the variation in the cost function
     */
    template <class Input, class State, class Move, typename CFtype>
    CostStructure<CFtype> NeighborhoodExplorer<Input, State, Move, CFtype>::DeltaCostFunctionComponents(const State& st, const Move & mv, const std::vector<double>& weights) const
    {
      CFtype delta_hard_cost = 0, delta_soft_cost = 0;
      double delta_weighted_cost = 0.0;
      std::vector<CFtype> delta_cost_function(CostComponent<Input, State, CFtype>::CostComponents(), (CFtype)0);
      
      for (size_t i = 0; i < delta_hard_cost_components.size(); i++)
      {
        DeltaCostComponent<Input, State, Move, CFtype>* dcc = delta_hard_cost_components[i];
        if (dcc->IsDeltaImplemented())
        {
          CFtype current_delta_cost = delta_cost_function[dcc->Index()] = dcc->DeltaCost(st, mv);
          delta_hard_cost += current_delta_cost;
          if (!weights.empty())
            delta_weighted_cost += HARD_WEIGHT * weights[dcc->Index()] * current_delta_cost;
        }
      }
      for (size_t i = 0; i < delta_soft_cost_components.size(); i++)
      {
        DeltaCostComponent<Input, State, Move, CFtype>* dcc = delta_soft_cost_components[i];
        if (dcc->IsDeltaImplemented())
        {
          CFtype current_delta_cost = delta_cost_function[dcc->Index()] = dcc->DeltaCost(st, mv);
          delta_soft_cost += current_delta_cost;
          if (!weights.empty())
            delta_weighted_cost += weights[dcc->Index()] * current_delta_cost;
        }
      }
      
      // only if there is at least one unimplemented delta cost component (i.e., a wrapper along a cost component)
      if (unimplemented_hard_components || unimplemented_soft_components)
      {
        // compute move
        State new_st = st;
        MakeMove(new_st, mv);
        
        if (unimplemented_hard_components)
          for (size_t i = 0; i < delta_hard_cost_components.size(); i++)
          {
            DeltaCostComponent<Input, State, Move, CFtype>* dcc = delta_hard_cost_components[i];
            if (!dcc->IsDeltaImplemented())
            {
              // get reference to cost component
              CostComponent<Input, State, CFtype>& cc = dcc->GetCostComponent();
              CFtype current_delta_cost = delta_cost_function[cc.Index()] =  cc.Weight() * (cc.ComputeCost(new_st) - cc.ComputeCost(st));
              delta_hard_cost += current_delta_cost;
              if (!weights.empty())
                delta_weighted_cost += HARD_WEIGHT * weights[cc.Index()] * current_delta_cost;
            }
          }
        if (unimplemented_soft_components)
          for (size_t i = 0; i < delta_soft_cost_components.size(); i++)
          {
            DeltaCostComponent<Input, State, Move, CFtype>* dcc = delta_soft_cost_components[i];
            if (!dcc->IsDeltaImplemented())
            {
              // get reference to cost component
              CostComponent<Input, State, CFtype>& cc = dcc->GetCostComponent();
              CFtype current_delta_cost =  delta_cost_function[cc.Index()] =  cc.Weight() * (cc.ComputeCost(new_st) - cc.ComputeCost(st));
              delta_soft_cost += current_delta_cost;
              if (!weights.empty())
                delta_weighted_cost += weights[cc.Index()] * current_delta_cost;
            }
          }
      }
      
      if (!weights.empty())
        return CostStructure<CFtype>(HARD_WEIGHT * delta_hard_cost + delta_soft_cost, delta_weighted_cost, delta_hard_cost, delta_soft_cost, delta_cost_function);
      else
        return CostStructure<CFtype>(HARD_WEIGHT * delta_hard_cost + delta_soft_cost, delta_hard_cost, delta_soft_cost, delta_cost_function);
    }
    
    template <class Input, class State, class Move, typename CFtype>
    void NeighborhoodExplorer<Input, State, Move, CFtype>::AddDeltaCostComponent(DeltaCostComponent<Input, State, Move, CFtype>& dcc)
    {
      if (dcc.IsHard())
        delta_hard_cost_components.push_back(&dcc);
      else
        delta_soft_cost_components.push_back(&dcc);
    }
    
    template <class Input, class State, class Move, typename CFtype>
    void NeighborhoodExplorer<Input, State, Move, CFtype>::AddCostComponent(CostComponent<Input, State, CFtype>& cc)
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
        
    /**
     This method will select the first move in the exhaustive neighborhood exploration that
     matches with the criterion expressed by the functional object bool f(const Move& mv, CFtype cost)
     */
    template <class Input, class State, class Move, typename CFtype>
    EvaluatedMove<Move, CFtype> NeighborhoodExplorer<Input, State, Move, CFtype>::SelectFirst(const State& st, size_t& explored, const MoveAcceptor& AcceptMove, const std::vector<double>& weights) const throw (EmptyNeighborhood)
    {
      explored = 0;
      EvaluatedMove<Move, CFtype> mv;
      FirstMove(st, mv.move);
      do
      {
        mv.cost = DeltaCostFunctionComponents(st, mv.move, weights);
        explored++;
        mv.is_valid = true;
        
        if (AcceptMove(mv.move, mv.cost))
          return mv; // mv passes the acceptance criterion
      }
      while (NextMove(st, mv.move));
      
      // exiting this loop means that there is no mv passing the acceptance criterion
      return EvaluatedMove<Move, CFtype>::empty;
    }
    
    /**
     This method will select the best move in the exhaustive neighborhood exploration that
     matches with the criterion expressed by the functional object bool f(const Move& mv, CFtype cost)
     */
    template <class Input, class State, class Move, typename CFtype>
    EvaluatedMove<Move, CFtype> NeighborhoodExplorer<Input, State, Move, CFtype>::SelectBest(const State& st, size_t& explored, const MoveAcceptor& AcceptMove, const std::vector<double>& weights) const throw (EmptyNeighborhood)
    {
      unsigned int number_of_bests = 0; // number of moves found with the same best value
      explored = 0;
      EvaluatedMove<Move, CFtype> mv, best_move;
      FirstMove(st, mv.move);
      
      do
      {
        mv.cost = DeltaCostFunctionComponents(st, mv.move, weights);
        explored++;
        mv.is_valid = true;
        if (AcceptMove(mv.move, mv.cost))
        {
          if (number_of_bests == 0)
          {
            best_move = mv;
            number_of_bests = 1;
          }
          else if (mv.cost < best_move.cost)
          {
            best_move = mv;
            number_of_bests = 1;
          }
          else if (mv.cost == best_move.cost)
          {
            if (Random::Int(0, number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
              best_move = mv;
              number_of_bests++;
          }
        }
      }
      while (NextMove(st, mv.move));
      
      if (number_of_bests == 0)
        return EvaluatedMove<Move, CFtype>::empty;
      
      return best_move;
    }
    
    /**
     This method will select the first move in the random neighborhood exploration that
     matches with the criterion expressed by the functional object bool f(const Move& mv, CFtype cost)
     */
    template <class Input, class State, class Move, typename CFtype>
    EvaluatedMove<Move, CFtype> NeighborhoodExplorer<Input, State, Move, CFtype>::RandomFirst(const State& st, size_t samples, size_t& explored, const MoveAcceptor& AcceptMove, const std::vector<double>& weights) const throw (EmptyNeighborhood)
    {
      EvaluatedMove<Move, CFtype> mv;
      explored = 0;
      while (explored < samples)
      {
        RandomMove(st, mv.move);
        mv.cost = DeltaCostFunctionComponents(st, mv.move, weights);
        explored++;
        mv.is_valid = true;
        if (AcceptMove(mv.move, mv.cost))
          return mv;
      }
      // exiting this loop means that there is no mv passing the acceptance criterion
      return EvaluatedMove<Move, CFtype>::empty;
    }
    
    /**
     This method will select the best move in the exhaustive neighborhood exploration that
     matches with the criterion expressed by the functional object bool f(const Move& mv, CFtype cost)
     */
    template <class Input, class State, class Move, typename CFtype>
    EvaluatedMove<Move, CFtype> NeighborhoodExplorer<Input, State, Move, CFtype>::RandomBest(const State& st, size_t samples, size_t& explored, const MoveAcceptor& AcceptMove, const std::vector<double>& weights) const throw (EmptyNeighborhood)
    {
      unsigned int number_of_bests = 0; // number of moves found with the same best value
      EvaluatedMove<Move, CFtype> mv, best_move;
      
      explored = 0;
      while (explored < samples)
      {
        RandomMove(st, mv.move);
        mv.cost = DeltaCostFunctionComponents(st, mv.move, weights);
        explored++;
        mv.is_valid = true;
        if (AcceptMove(mv.move, mv.cost))
        {
          if (number_of_bests == 0)
          {
            best_move = mv;
            number_of_bests = 1;
          }
          else if (mv.cost < best_move.cost)
          {
            best_move = mv;
            number_of_bests = 1;
          }
          else if (mv.cost == best_move.cost)
          {
            if (Random::Int(0, number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
              best_move = mv;
              number_of_bests++;
          }
        }
      }
      
      if (number_of_bests == 0)
        return EvaluatedMove<Move, CFtype>::empty;
      
      return best_move;
    }
  }
}

#endif // _NEIGHBORHOOD_EXPLORER_HH_

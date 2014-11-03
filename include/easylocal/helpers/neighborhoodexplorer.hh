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
      EvaluatedMove(bool is_valid = true) : is_valid(is_valid) {}
      EvaluatedMove(const Move& move, CostComponents<CFtype> cost) : is_valid(true), move(move), cost(cost) {}
      EvaluatedMove(const Move& move, CFtype total = 0, CFtype violations = 0, CFtype objective = 0) : is_valid(true), move(move)
      {
        cost.total = total;
        cost.violations = violations;
        cost.objective = objective;
      }
      bool is_valid;
      Move move;
      CostComponents<CFtype> cost;
    };
    
    template <class Move, typename CFtype>
    EvaluatedMove<Move, CFtype> EvaluatedMove<Move, CFtype>::empty = EvaluatedMove<Move, CFtype>(false);
    
    /** The Neighborhood Explorer is responsible for the strategy exploited in the exploration of the neighborhood, and for computing the variations of the cost function due to a specific
     @ref Move.
     @ingroup Helpers
     */
    template <class Input, class State, class Move, typename CFtype = int>
    class NeighborhoodExplorer
    {
    public:
      typedef Move MoveType;
      typedef State StateType;
      typedef CFtype CostType;
      
      typedef typename std::function<bool(const Move& mv, CostComponents<CFtype> move_cost)> MoveAcceptor;
      
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
      
      /**
       Generates the move that follows mv in the exploration of the neighborhood of the state st. It returns the generated move in the same variable mv.
       Explores the neighborhood starting from an initial_move that is not the first move in the enumeration.
       @note It is an internal fix for template instantiation in Multimodal Neighborhood Explorers.
       @return @c false if @c mv is the last in the neighborhood of the state.
       @note To be implemented in the application.
       @param st the start state
       @param mv the move
       */
      inline bool NextMoveWithFirst(const State &st, Move& mv, const Move& initial_mv) const
      {
        if (!NextMove(st, mv))
          FirstMove(st, mv);
        return (mv != initial_mv);
      }
      
      /** @copydoc NextMoveWithFirst() */
      inline bool NextMove(const State &st, Move& mv, const Move& initial_mv) const
      {
        return NextMoveWithFirst(st, mv, initial_mv);
      }
      
      /** Generate the first improvingt move in the exploration of the neighborhood of a given state. It uses @ref FirstMove and @ref NextMove
       @param st the start state
       @param mv the generated move
       @throws EmptyNeighborhood when the State st has no neighbor
       */
      virtual CFtype FirstImprovingMove(const State& st, Move& mv) const throw (EmptyNeighborhood);
      
      /** Generates the best move in the full exploration of the neighborhood of a given state. It uses @ref FirstMove and @ref NextMove
       @param st the start state.
       @param mv the generated move.
       @return the variation of the cost due to the Move mv.
       @throws EmptyNeighborhood when the State st has no neighbor
       */
      virtual CFtype BestMove(const State& st, Move& mv) const throw (EmptyNeighborhood);
      
      /** Modifies the state passed as parameter by applying a given move upon it.
       @note To be implemented in the application (MustDef)
       @param st the state to modify
       @param mv the move to be applied
       */
      virtual void MakeMove(State &st, const Move& mv) const = 0;
      
      /** Computes the differences in the cost function obtained by applying the move @c mv to the state @c st.
       @param st the state to modify
       @param mv the move to be applied
       @return the difference in the cost function
       */
      virtual CFtype DeltaCostFunction(const State& st, const Move& mv) const;
      
      /** Computes the differences in the cost function obtained by applying the move @c mv to the state @c st and returns the unaggregated value as a vector of components.
       @param st the state to modify
       @param mv the move to be applied
       @return the difference in the cost function for each cost component
       */
      virtual CostComponents<CFtype> DeltaCostFunctionComponents(const State& st, const Move& mv, const std::vector<double>& weights = std::vector<double>(0)) const;
      
      /** Computes the differences in the objective component of the cost function obtained by applying the move @c mv to the state @c st.
       @param st the state to modify
       @param mv the move to be applied
       @return the difference in the objective function
       */
      virtual CFtype DeltaObjective(const State& st, const Move & mv) const;
      /** Computes the differences in the violations component (i.e., hard constraints) of the cost function obtained by applying the move @c mv to the state @c st.
       @param st the state to modify
       @param mv the move to be applied
       @return the difference in the violation function
       */
      
      virtual CFtype DeltaViolations(const State& st, const Move & mv) const;
      
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
      virtual size_t NumberOfDeltaCostComponents() const
      {
        return delta_hard_cost_components.size() + delta_soft_cost_components.size();
      }
      
      /** Returns an element of the delta cost component vector attached to the neighborhood explorer.
       @param i the index of the required delta cost component
       @return the delta cost component of index i
       */
      virtual DeltaCostComponent<Input, State, Move, CFtype>& GetDeltaCostComponent(unsigned int i)
      {
        if (i < delta_hard_cost_components.size())
          return *delta_hard_cost_components[i];
        else if (i < delta_hard_cost_components.size() + delta_soft_cost_components.size())
          return *delta_soft_cost_components[i - delta_hard_cost_components.size()];
        else
          throw std::logic_error("GetDeltaCostComponent: index out of bounds");
      }
      
      /** Returns the number of delta cost components attached to the neighborhood explorer which are hard cost components.
       @return the size of the hard delta cost components vector
       */
      virtual size_t DeltaHardCostComponents() const
      { return delta_hard_cost_components.size(); }
      
      /** Returns an element of the delta hard cost component vector attached to the neighborhood explorer.
       @param i the index of the required delta cost component
       @return the delta cost component of index i
       */
      virtual DeltaCostComponent<Input, State, Move, CFtype>& GetDeltaHardCostComponent(unsigned int i)
      {
        if (i < delta_hard_cost_components.size())
          return *delta_hard_cost_components[i];
        else
          throw std::logic_error("GetDeltaHardCostComponent: index out of bounds");
      }
      
      /** Returns the number of delta cost components attached to the neighborhood explorer which are soft cost components.
       @return the size of the hard delta cost components vector
       */
      virtual size_t DeltaSoftCostComponents() const
      { return delta_soft_cost_components.size(); }
      
      /** Returns an element of the delta soft cost component vector attached to the neighborhood explorer.
       @param i the index of the required delta cost component
       @return the delta cost component of index i
       */
      virtual DeltaCostComponent<Input, State, Move, CFtype>& GetDeltaSoftCostComponent(unsigned int i)
      {
        if (i < delta_soft_cost_components.size())
          return *delta_soft_cost_components[i];
        else
          throw std::logic_error("GetDeltaSoftCostComponent: index out of bounds");
      }
      
      /** Retuns the modality of the neighborhood explorer, i.e., the number of different kind of moves handled by it.
       @return the modality of the neighborhood explorer
       */
      virtual unsigned int Modality() const
      { return 1; }
      
      /** Returns the modality of the move passed as parameter, i.e., the number of move components in a composite move that are active.
       @param mv a move
       @return the modality of the move
       */
      virtual unsigned int MoveModality(const Move& mv) const
      { return 0; }
      
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
      virtual EvaluatedMove<Move, CFtype> SelectFirst(const State& st, const MoveAcceptor& AcceptMove, const std::vector<double>& weights = std::vector<double>(0)) const throw (EmptyNeighborhood);
      
      /**
       This method will select the best move in the exhaustive neighborhood exploration that
       matches with the criterion expressed by the functional object bool f(const Move& mv, CFtype cost)
       */
      virtual EvaluatedMove<Move, CFtype> SelectBest(const State& st, const MoveAcceptor& AcceptMove, const std::vector<double>& weights = std::vector<double>(0)) const throw (EmptyNeighborhood);
      
      
      /**
       This method will select the first move in a random neighborhood exploration that
       matches with the criterion expressed by the functional object bool f(const Move& mv, CFtype cost)
       */
      virtual EvaluatedMove<Move, CFtype> RandomFirst(const State& st, size_t samples, size_t& sampled, const MoveAcceptor& AcceptMove, const std::vector<double>& weights = std::vector<double>(0)) const throw (EmptyNeighborhood);
      
      /**
       This method will select the best move in a random neighborhood exploration that
       matches with the criterion expressed by the functional object bool f(const Move& mv, CFtype cost)
       */
      virtual EvaluatedMove<Move, CFtype> RandomBest(const State& st, size_t samples, size_t& sampled, const MoveAcceptor& AcceptMove, const std::vector<double>& weights = std::vector<double>(0)) const throw (EmptyNeighborhood);
      
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
    CFtype NeighborhoodExplorer<Input, State, Move, CFtype>::DeltaCostFunction(const State& st, const Move & mv) const
    {
      CFtype delta_hard_cost = 0, delta_soft_cost = 0;
      
      // compute delta costs (of implemented delta cost components)
      for (DeltaCostComponent<Input, State, Move, CFtype>* dcc : delta_hard_cost_components)
        if (dcc->IsDeltaImplemented())
          delta_hard_cost += dcc->DeltaCost(st, mv);
      
      for (DeltaCostComponent<Input, State, Move, CFtype>* dcc : delta_soft_cost_components)
        if (dcc->IsDeltaImplemented())
          delta_soft_cost += dcc->DeltaCost(st, mv);
      
      // only if there is at least one unimplemented delta cost component (i.e., a wrapper along a cost component)
      if (unimplemented_hard_components || unimplemented_soft_components)
      {
        // compute move
        State new_st = st;
        MakeMove(new_st, mv);
        
        if (unimplemented_hard_components)
          for (DeltaCostComponent<Input, State, Move, CFtype>* dcc : delta_hard_cost_components)
            if (!dcc->IsDeltaImplemented())
            {
              // get reference to cost component
              CostComponent<Input, State, CFtype>& cc = dcc->GetCostComponent();
              delta_hard_cost +=  cc.Weight() * (cc.ComputeCost(new_st) - cc.ComputeCost(st));
            }
        if (unimplemented_soft_components)
          for (DeltaCostComponent<Input, State, Move, CFtype>* dcc : delta_soft_cost_components)
            if (!dcc->IsDeltaImplemented())
            {
              // get reference to cost component
              CostComponent<Input, State, CFtype>& cc = dcc->GetCostComponent();
              delta_soft_cost +=  cc.Weight() * (cc.ComputeCost(new_st) - cc.ComputeCost(st));
            }
      }
      
      return HARD_WEIGHT * delta_hard_cost + delta_soft_cost;
    }
    
    template <class Input, class State, class Move, typename CFtype>
    CostComponents<CFtype> NeighborhoodExplorer<Input, State, Move, CFtype>::DeltaCostFunctionComponents(const State& st, const Move & mv, const std::vector<double>& weights) const
    {
      CFtype delta_hard_cost = 0, delta_soft_cost = 0, delta_weighted_cost = 0;
      std::vector<CFtype> delta_cost_function(CostComponent<Input, State, CFtype>::NumberOfCostComponents(), (CFtype)0);
      
      for (unsigned int i = 0; i < delta_hard_cost_components.size(); i++)
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
      for (unsigned int i = 0; i < delta_soft_cost_components.size(); i++)
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
          for (unsigned int i = 0; i < delta_hard_cost_components.size(); i++)
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
          for (unsigned int i = 0; i < delta_soft_cost_components.size(); i++)
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
        return CostComponents<CFtype>(HARD_WEIGHT * delta_hard_cost + delta_soft_cost, delta_weighted_cost, delta_hard_cost, delta_soft_cost, delta_cost_function);
      else
        return CostComponents<CFtype>(HARD_WEIGHT * delta_hard_cost + delta_soft_cost, delta_hard_cost, delta_soft_cost, delta_cost_function);
    }
    
    template <class Input, class State, class Move, typename CFtype>
    CFtype NeighborhoodExplorer<Input, State, Move, CFtype>::DeltaViolations(const State& st, const Move & mv) const
    {
      CFtype delta_hard_cost = 0;
      
      // compute delta costs (of implemented delta cost components)
      for (DeltaCostComponent<Input, State, Move, CFtype>* dcc : delta_hard_cost_components)
        if (dcc->IsDeltaImplemented())
          delta_hard_cost += dcc->DeltaCost(st, mv);
      
      // only if there is at least one unimplemented delta cost component (i.e., a wrapper along a cost component)
      if (unimplemented_hard_components)
      {
        // compute move
        State new_st = st;
        MakeMove(new_st, mv);
        
        for (DeltaCostComponent<Input, State, Move, CFtype>* dcc : delta_hard_cost_components)
          if (!dcc->IsDeltaImplemented())
          {
            // get reference to cost component
            CostComponent<Input, State, CFtype>& cc = dcc->GetCostComponent();
            delta_hard_cost +=  cc.Weight() * (cc.ComputeCost(new_st) - cc.ComputeCost(st));
          }
      }
      
      return delta_hard_cost;
    }
    
    template <class Input, class State, class Move, typename CFtype>
    CFtype NeighborhoodExplorer<Input, State, Move, CFtype>::DeltaObjective(const State& st, const Move & mv) const
    {
      CFtype delta_soft_cost = 0;
      
      // compute delta costs (of implemented delta cost components)
      for (DeltaCostComponent<Input, State, Move, CFtype>* dcc : delta_soft_cost_components)
        if (dcc->IsDeltaImplemented())
          delta_soft_cost += dcc->DeltaCost(st, mv);
      
      // only if there is at least one unimplemented delta cost component (i.e., a wrapper along a cost component)
      if (unimplemented_soft_components)
      {
        // compute move
        State new_st = st;
        MakeMove(new_st, mv);
        
        for (DeltaCostComponent<Input, State, Move, CFtype>* dcc : delta_soft_cost_components)
          if (!dcc->IsDeltaImplemented())
          {
            // get reference to cost component
            CostComponent<Input, State, CFtype>& cc = dcc->GetCostComponent();
            delta_soft_cost +=  cc.Weight() * (cc.ComputeCost(new_st) - cc.ComputeCost(st));
          }
      }
      
      return delta_soft_cost;
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
    
    template <class Input, class State, class Move, typename CFtype>
    CFtype NeighborhoodExplorer<Input, State, Move, CFtype>::BestMove(const State &st, Move& mv) const throw (EmptyNeighborhood)
    {
      EvaluatedMove<Move, CFtype> em = SelectBest(st, [](const Move& mv, CostComponents<CFtype> cost) { return true; });
      if (!em.is_valid)
      {
        throw EmptyNeighborhood();
      }
      mv = em.move;
      return em.cost.total;
    }
    
    template <class Input, class State, class Move, typename CFtype>
    CFtype NeighborhoodExplorer<Input, State, Move, CFtype>::FirstImprovingMove(const State &st, Move& mv) const throw (EmptyNeighborhood)
    {
      EvaluatedMove<Move, CFtype> em = SelectFirst(st, [](const Move& mv, CostComponents<CFtype> cost) { return LessThan(cost.total, (CFtype)0); });
      if (!em.is_valid)
      {
        throw EmptyNeighborhood();
      }
      mv = em.move;
      return em.cost.total;
    }
  }
  
  /**
   This method will select the first move in the exhaustive neighborhood exploration that
   matches with the criterion expressed by the functional object bool f(const Move& mv, CFtype cost)
   */
  template <class Input, class State, class Move, typename CFtype>
  EvaluatedMove<Move, CFtype> NeighborhoodExplorer<Input, State, Move, CFtype>::SelectFirst(const State& st, const MoveAcceptor& AcceptMove, const std::vector<double>& weights) const throw (EmptyNeighborhood)
  {
    unsigned int number_of_bests = 0;
    Move mv;
    FirstMove(st, mv);
    CostComponents<CFtype> mv_cost = DeltaCostFunctionComponents(st, mv, weights);
    Move best_move = mv;
    CostComponents<CFtype> best_delta = mv_cost;
    
    while (NextMove(st, mv))
    {
      mv_cost = DeltaCostFunctionComponents(st, mv, weights);
      if (AcceptMove(mv, mv_cost))
        return EvaluatedMove<Move, CFtype>(mv, mv_cost); // mv passes the acceptance criterion
      
      if (LessThan(mv_cost.total, best_delta.total))
      {
        best_move = mv;
        best_delta = mv_cost;
        number_of_bests = 1;
      }
      else if (EqualTo(mv_cost.total, best_delta.total))
      {
        if (Random::Int(0, number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
          best_move = mv;
        number_of_bests++;
      }
    }
    // exiting this loop means that there is no mv passing the acceptance criterion
    return EvaluatedMove<Move, CFtype>::empty;
  }
  
  /**
   This method will select the best move in the exhaustive neighborhood exploration that
   matches with the criterion expressed by the functional object bool f(const Move& mv, CFtype cost)
   */
  template <class Input, class State, class Move, typename CFtype>
  EvaluatedMove<Move, CFtype> NeighborhoodExplorer<Input, State, Move, CFtype>::SelectBest(const State& st, const MoveAcceptor& AcceptMove, const std::vector<double>& weights) const throw (EmptyNeighborhood)
  {
    // TODO: review by checking for move acceptance in an outer if
    unsigned int number_of_bests = 1; // number of moves found with the same best value
    Move mv;
    FirstMove(st, mv);
    Move best_move = mv;
    CostComponents<CFtype> mv_cost = DeltaCostFunctionComponents(st, mv, weights);
    CostComponents<CFtype> best_delta = mv_cost;
    bool first_found = AcceptMove(mv, mv_cost);
    
    while (NextMove(st, mv))
    {
      mv_cost = DeltaCostFunctionComponents(st, mv, weights);
      if (!first_found && AcceptMove(mv, mv_cost))
      {
        best_move = mv;
        best_delta = mv_cost;
        number_of_bests = 1;
        first_found = true;
      }
      if (AcceptMove(mv, mv_cost) && LessThan(mv_cost.total, best_delta.total))
      {
        best_move = mv;
        best_delta = mv_cost;
        number_of_bests = 1;
      }
      else if (AcceptMove(mv, mv_cost) && EqualTo(mv_cost.total, best_delta.total))
      {
        if (Random::Int(0, number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
          best_move = mv;
        number_of_bests++;
      }
    }
    
    if (!first_found)
      return EvaluatedMove<Move, CFtype>::empty;
    
    return EvaluatedMove<Move, CFtype>(best_move, best_delta);
  }
  
  /**
   This method will select the first move in the random neighborhood exploration that
   matches with the criterion expressed by the functional object bool f(const Move& mv, CFtype cost)
   */
  template <class Input, class State, class Move, typename CFtype>
  EvaluatedMove<Move, CFtype> NeighborhoodExplorer<Input, State, Move, CFtype>::RandomFirst(const State& st, size_t samples, size_t& sampled, const MoveAcceptor& AcceptMove, const std::vector<double>& weights) const throw (EmptyNeighborhood)
  {
    Move mv;
    CostComponents<CFtype> mv_cost;
    for (sampled = 0; sampled < samples; sampled++)
    {
      RandomMove(st, mv);
      mv_cost = DeltaCostFunctionComponents(st, mv, weights);
      if (AcceptMove(mv, mv_cost))
        return EvaluatedMove<Move, CFtype>(mv, mv_cost);
    }
    // exiting this loop means that there is no mv passing the acceptance criterion
    return EvaluatedMove<Move, CFtype>::empty;
  }
  
  /**
   This method will select the best move in the exhaustive neighborhood exploration that
   matches with the criterion expressed by the functional object bool f(const Move& mv, CFtype cost)
   */
  template <class Input, class State, class Move, typename CFtype>
  EvaluatedMove<Move, CFtype> NeighborhoodExplorer<Input, State, Move, CFtype>::RandomBest(const State& st, size_t samples, size_t& sampled, const MoveAcceptor& AcceptMove, const std::vector<double>& weights) const throw (EmptyNeighborhood)
  {
    // TODO: review by checking for move acceptance in an outer if
    unsigned int number_of_bests = 1; // number of moves found with the same best value
    Move mv;
    RandomMove(st, mv);
    Move best_move = mv;
    CostComponents<CFtype> mv_cost = DeltaCostFunctionComponents(st, mv, weights);
    CostComponents<CFtype> best_delta = mv_cost;
    bool first_found = AcceptMove(mv, mv_cost);

    for (sampled = 0; sampled < samples; sampled++)
    {
      mv_cost = DeltaCostFunctionComponents(st, mv, weights);
      if (!first_found && AcceptMove(mv, mv_cost))
      {
        best_move = mv;
        best_delta = mv_cost;
        number_of_bests = 1;
        first_found = true;
      }
      if (AcceptMove(mv, mv_cost) && LessThan(mv_cost.total, best_delta.total))
      {
        best_move = mv;
        best_delta = mv_cost;
        number_of_bests = 1;
      }
      else if (AcceptMove(mv, mv_cost) && EqualTo(mv_cost.total, best_delta.total))
      {
        if (Random::Int(0, number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
          best_move = mv;
          number_of_bests++;
      }
      RandomMove(st, mv);
    }
    
    if (!first_found)
      return EvaluatedMove<Move, CFtype>::empty;
    
    return EvaluatedMove<Move, CFtype>(best_move, best_delta);
  }
}

#endif // _NEIGHBORHOOD_EXPLORER_HH_

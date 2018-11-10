#pragma once

#include <typeinfo>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <iterator>
#include <functional>

#include "easylocal/helpers/deltacostcomponent.hh"
#include "easylocal/helpers/statemanager.hh"
#include "easylocal/utils/random.hh"
#include "easylocal/utils/deprecationhandler.hh"

namespace EasyLocal
{
  
  namespace Core
  {
    
    /** Exception raised when the neighborhood is empty. */
    class EmptyNeighborhood : public std::logic_error
    {
    public:
      EmptyNeighborhood() : std::logic_error("Empty neighborhood") {}
    };
    
    template <class Move, class CostStructure = DefaultCostStructure<int>>
    struct EvaluatedMove
    {
      static EvaluatedMove empty;
      EvaluatedMove() : is_valid(false) {}
      EvaluatedMove(const Move &move) : move(move), is_valid(false) {}
      EvaluatedMove(const Move &move, const CostStructure &cost) : is_valid(true), move(move), cost(cost) {}
      
      Move move;
      bool is_valid;
      CostStructure cost;
    };
    
    template <class Move, class CostStructure>
    std::ostream &operator<<(std::ostream &os, const EvaluatedMove<Move, CostStructure> &em)
    {
      os << em.move;
      if (em.is_valid)
        os << " " << em.cost;
      else
        os << " not_valid";
      return os;
    }
    
    template <class Move, class CostStructure>
    EvaluatedMove<Move, CostStructure> EvaluatedMove<Move, CostStructure>::empty = EvaluatedMove<Move, CostStructure>();
    
    /** The Neighborhood Explorer is responsible for the strategy exploited in the exploration of the neighborhood, and for computing the variations of the cost function due to a specific
     @ref Move.
     @ingroup Helpers
     */
    template <class _Input, class _State, class _Move, class _CostStructure = DefaultCostStructure<int>>
    class NeighborhoodExplorer : protected DeprecationHandler<_Input>
    {
    public:
      typedef _Input Input;
      typedef _Move Move;
      typedef _State State;
      typedef typename _CostStructure::CFtype CFtype;
      typedef _CostStructure CostStructure;
      
      typedef typename std::function<bool(const Move &mv, const CostStructure &move_cost)> MoveAcceptor;
      
      /* Copies all the delta cost components from another neighborhood explorer of the same class
       @param ne the neighborhood explorer from which the data has to be copied
       */
      void CopyDeltaCostComponents(const NeighborhoodExplorer<Input, State, Move, CostStructure> &ne)
      {
        this->delta_hard_cost_components = ne.delta_hard_cost_components;
        this->delta_soft_cost_components = ne.delta_soft_cost_components;
        this->dcc_adapters = ne.dcc_adapters;
        this->unimplemented_hard_components = ne.unimplemented_hard_components;
        this->unimplemented_soft_components = ne.unimplemented_soft_components;
      }
      
      /** Checks if a move in the neighborhood is legal.
       @note Can be implemented in the application (MayRedef)
       @param in the input object
       @param st the start state
       @param mv the move
       */
      virtual bool FeasibleMove(const Input& in, const State &st, const Move &mv) const
      {
        return true;
      }
      
      /** Generates a random move in the neighborhood of a given state.
       @note To be implemented in the application (MustDef)
       @param in the input object
       @param st the start state
       @param mv the generated move
       */
      virtual void RandomMove(const Input& in, const State &st, Move &mv) const = 0;
      
      /** Generates the first move in the neighborhood (a total ordering of the neighborhood is assumed). It is always used on cooperation with @ref NextMove to generate the whole neighborhood. It returns @c void because it is assumed that at least a move exists in the neighborhood.  It writes the first move in @c mv.
       @note To be implemented in the application (MustDef)
       @param in the input object
       @param st the start state
       @param mv the move
       */
      virtual void FirstMove(const Input& in, const State &st, Move &mv) const = 0;
      
      /** Generates the move that follows mv in the exploration of the neighborhood of the state st. It returns the generated move in the same variable mv.
       @return @c false if @c mv is the last in the neighborhood of the state.
       @note To be implemented in the application.
       @param in the input object
       @param st the start state
       @param mv the move
       */
      virtual bool NextMove(const Input& in, const State &st, Move &mv) const = 0;
      
      /** Modifies the state passed as parameter by applying a given move upon it.
       @note To be implemented in the application (MustDef)
       param in the input object
       @param in the input object
       
       @param st the state to modify
       @param mv the move to be applied
       */
      virtual void MakeMove(const Input& in, State &st, const Move &mv) const = 0;
      
      // It can be safely removed
      //  /** Old-style method, without the Input object
      //   @deprecated
      //   */
      //  [[deprecated("This is the old style easylocal interface, it is mandatory to upgrade to Input-less class and Input-aware methods")]]
      //  CostStructure DeltaCostFunctionComponents(const State &st, const Move &mv, const std::vector<double> &weights = std::vector<double>(0)) const
      //  {
      //    throw std::runtime_error("You should update your NeighborhoodExplorer by adding a const Input& reference to the method");
      //  }
      
      
      /** Computes the differences in the cost function obtained by applying the move @c mv to the state @c st and returns the unaggregated value as a vector of components.
       @param in the input object
       @param st the state to modify
       @param mv the move to be applied
       @return the difference in the cost function for each cost component
       */
      virtual CostStructure DeltaCostFunctionComponents(const Input& in, const State &st, const Move &mv, const std::vector<double> &weights = std::vector<double>(0)) const;
      
      /** Adds a delta cost component to the neighborhood explorer, which is responsible for computing one component of the cost function. A delta cost component requires the implementation of a way to compute the difference in the cost function without simulating the move on a given state.
       @param dcc a delta cost component object
       */
      virtual void AddDeltaCostComponent(DeltaCostComponent<Input, State, Move, CFtype> &dcc);
      
      /** Adds a cost component to the neighborhood explorer, which is responsible for computing one component of the cost function. A cost component passed to the neighborhood explorer will compute the difference in the cost function due to that component as the difference between the cost in the current state and the new state obtained after (actually) performing the move. It will be wrapped into a delta cost component by means of an adapter and it might be seen as an unimplemented delta cost component.
       @note In general it is a quite unefficient way to compute the contribution of the move and it should be avoided, if possible.
       @param cc a cost component object
       */
      
      virtual void AddCostComponent(CostComponent<Input, State, CFtype> &cc);
      
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
      {
        return 1;
      }
      
      // These methods have been deprecated
      
      /** Old-style method, without the Input object
       @deprecated
       */
      [[deprecated("This is the old style easylocal interface, it is mandatory to upgrade to Input-less class and Input-aware methods")]]
      bool FeasibleMove(const State &st, const Move &mv) const
      {
        return FeasibleMove(this->GetInput(), st, mv);
      }
      
      /** Old-style method, without the Input object
       @deprecated
       */
      [[deprecated("This is the old style easylocal interface, it is mandatory to upgrade to Input-less class and Input-aware methods")]]
      void RandomMove(const State &st, Move &mv) const
      {
        RandomMove(this->GetInput(), st, mv);
      }
      
      /** Old-style method, without the Input object
       @deprecated
       */
      [[deprecated("This is the old style easylocal interface, it is mandatory to upgrade to Input-less class and Input-aware methods")]]
      void FirstMove(const State &st, Move &mv) const
      {
        FirstMove(this->GetInput(), st, mv);
      }
      
      /** Old-style method, without the Input object
       @deprecated
       */
      [[deprecated("This is the old style easylocal interface, it is mandatory to upgrade to Input-less class and Input-aware methods")]]
      bool NextMove(const State &st, Move &mv) const
      {
        return NextMove(this->GetInput(), st, mv);
      }
      
      /** Old-style method, without the Input object
       @deprecated
       */
      [[deprecated("This is the old style easylocal interface, it is mandatory to upgrade to Input-less class and Input-aware methods")]]
      void MakeMove(State &st, const Move &mv) const
      {
        MakeMove(this->GetInput(), st, mv);
      }
      
      /**
       Deprecated constructor.
       @deprecated
       */
      [[deprecated("This is the old style easylocal interface, it is mandatory to upgrade to Input-less class and Input-aware methods")]]
      NeighborhoodExplorer(const Input &in, StateManager<Input, State, CostStructure> &sm, std::string name)  : DeprecationHandler<Input>(in), sm(sm), name(name), unimplemented_hard_components(false), unimplemented_soft_components(false)
      {}
      
      /**
       Constructs a neighborhood explorer passing a n input object and a state manager.
       
       @param sm a reference to a compatible state manager.
       @param name the name associated to the NeighborhoodExplorer.
       */
      NeighborhoodExplorer(StateManager<Input, State, CostStructure> &sm, std::string name);
      
      
      virtual ~NeighborhoodExplorer() {}
      
      /**
       This method will select the first move in the exhaustive neighborhood exploration that
       matches with the criterion expressed by the functional object bool f(const Move& mv, CostStructure cost)
       */
      virtual EvaluatedMove<Move, CostStructure> SelectFirst(const Input& in, const State &st, size_t &explored, const MoveAcceptor &AcceptMove, const std::vector<double> &weights = std::vector<double>(0)) const;
      
      /**
       This method will select the best move in the exhaustive neighborhood exploration that
       matches with the criterion expressed by the functional object bool f(const Move& mv, CostStructure cost)
       */
      virtual EvaluatedMove<Move, CostStructure> SelectBest(const Input& in, const State &st, size_t &explored, const MoveAcceptor &AcceptMove, const std::vector<double> &weights = std::vector<double>(0)) const;
      
      /**
       This method will select the first move in a random neighborhood exploration that
       matches with the criterion expressed by the functional object bool f(const Move& mv, CostStructure cost)
       */
      virtual EvaluatedMove<Move, CostStructure> RandomFirst(const Input& in, const State &st, size_t samples, size_t &explored, const MoveAcceptor &AcceptMove, const std::vector<double> &weights = std::vector<double>(0)) const;
      
      /**
       This method will select the best move in a random neighborhood exploration that
       matches with the criterion expressed by the functional object bool f(const Move& mv, CostStructure cost)
       */
      virtual EvaluatedMove<Move, CostStructure> RandomBest(const Input& in, const State &st, size_t samples, size_t &explored, const MoveAcceptor &AcceptMove, const std::vector<double> &weights = std::vector<double>(0)) const;
      
    protected:
      StateManager<Input, State, CostStructure> &sm; /**< A reference to the attached state manager. */
      
      /** Lists of delta cost components (or adapters) */
      std::vector<DeltaCostComponent<Input, State, Move, CFtype> *> delta_hard_cost_components, delta_soft_cost_components;
      
      /** List of created adapters (to be automatically deleted in the destructor). */
      std::vector<std::shared_ptr<DeltaCostComponentAdapter<Input, State, Move, CostStructure>>> dcc_adapters;
      
      /** Name of user-defined neighborhood explorer */
      std::string name;
      
      /** States whether there are unimplemented delta cost components attached */
      bool unimplemented_hard_components, unimplemented_soft_components;
    };
    
    /** IMPLEMENTATION */
    
    template <class Input, class State, class Move, class CostStructure>
    NeighborhoodExplorer<Input, State, Move, CostStructure>::NeighborhoodExplorer(StateManager<Input, State, CostStructure> &sm, std::string name)
    : sm(sm), name(name), unimplemented_hard_components(false), unimplemented_soft_components(false)
    {
    }
    
    /** Evaluates the variation of the cost function obtainted either by applying the move to the given state or simulating it. The tentative definition computes a weighted sum of the variation of the violations function and of the difference in the objective function.
     @param st the start state
     @param mv the move
     @return the variation in the cost function
     */
    template <class Input, class State, class Move, class CostStructure>
    CostStructure NeighborhoodExplorer<Input, State, Move, CostStructure>::DeltaCostFunctionComponents(const Input& in, const State &st, const Move &mv, const std::vector<double> &weights) const
    {
      CFtype delta_hard_cost = 0, delta_soft_cost = 0;
      double delta_weighted_cost = 0.0;
      std::vector<CFtype> delta_cost_function(sm.CostComponents(), static_cast<CFtype>(0));
      
      for (size_t i = 0; i < delta_hard_cost_components.size(); i++)
      {
        auto dcc = delta_hard_cost_components[i];
        if (dcc->IsDeltaImplemented())
        {
          CFtype current_delta_cost = delta_cost_function[sm.CostComponentIndex(dcc->cc)] = dcc->DeltaCost(in, st, mv);
          delta_hard_cost += current_delta_cost;
          if (!weights.empty())
            delta_weighted_cost += HARD_WEIGHT * weights[sm.CostComponentIndex(dcc->cc)] * current_delta_cost;
        }
      }
      for (size_t i = 0; i < delta_soft_cost_components.size(); i++)
      {
        auto dcc = delta_soft_cost_components[i];
        if (dcc->IsDeltaImplemented())
        {
          CFtype current_delta_cost = delta_cost_function[sm.CostComponentIndex(dcc->cc)] = dcc->DeltaCost(in, st, mv);
          delta_soft_cost += current_delta_cost;
          if (!weights.empty())
            delta_weighted_cost += weights[sm.CostComponentIndex(dcc->cc)] * current_delta_cost;
        }
      }
      
      // only if there is at least one unimplemented delta cost component (i.e., a wrapper along a cost component)
      if (unimplemented_hard_components || unimplemented_soft_components)
      {
        // compute move
        State new_st = st;
        MakeMove(in, new_st, mv);
        
        if (unimplemented_hard_components)
          for (size_t i = 0; i < delta_hard_cost_components.size(); i++)
          {
            auto dcc = delta_hard_cost_components[i];
            if (!dcc->IsDeltaImplemented())
            {
              // get reference to cost component
              auto &cc = dcc->GetCostComponent();
              CFtype current_delta_cost = delta_cost_function[sm.CostComponentIndex(cc)] = cc.Weight() * (cc.ComputeCost(in, new_st) - cc.ComputeCost(in, st));
              delta_hard_cost += current_delta_cost;
              if (!weights.empty())
                delta_weighted_cost += HARD_WEIGHT * weights[sm.CostComponentIndex(cc)] * current_delta_cost;
            }
          }
        if (unimplemented_soft_components)
          for (size_t i = 0; i < delta_soft_cost_components.size(); i++)
          {
            auto dcc = delta_soft_cost_components[i];
            if (!dcc->IsDeltaImplemented())
            {
              // get reference to cost component
              auto &cc = dcc->GetCostComponent();
              CFtype current_delta_cost = delta_cost_function[sm.CostComponentIndex(cc)] = cc.Weight() * (cc.ComputeCost(in, new_st) - cc.ComputeCost(in, st));
              delta_soft_cost += current_delta_cost;
              if (!weights.empty())
                delta_weighted_cost += weights[sm.CostComponentIndex(cc)] * current_delta_cost;
            }
          }
      }
      
      if (!weights.empty())
        return CostStructure(HARD_WEIGHT * delta_hard_cost + delta_soft_cost, delta_weighted_cost, delta_hard_cost, delta_soft_cost, delta_cost_function);
      else
        return CostStructure(HARD_WEIGHT * delta_hard_cost + delta_soft_cost, delta_hard_cost, delta_soft_cost, delta_cost_function);
    }
    
    template <class Input, class State, class Move, class CostStructure>
    void NeighborhoodExplorer<Input, State, Move, CostStructure>::AddDeltaCostComponent(DeltaCostComponent<Input, State, Move, CFtype> &dcc)
    {
      if (dcc.IsHard())
        delta_hard_cost_components.push_back(&dcc);
      else
        delta_soft_cost_components.push_back(&dcc);
    }
    
    template <class Input, class State, class Move, class CostStructure>
    void NeighborhoodExplorer<Input, State, Move, CostStructure>::AddCostComponent(CostComponent<Input, State, CFtype> &cc)
    {
      
      dcc_adapters.push_back(std::make_shared<DeltaCostComponentAdapter<Input, State, Move, CostStructure>>(cc, *this));
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
     matches with the criterion expressed by the functional object bool f(const Move& mv, CostStructure cost)
     */
    template <class Input, class State, class Move, class CostStructure>
    EvaluatedMove<Move, CostStructure> NeighborhoodExplorer<Input, State, Move, CostStructure>::SelectFirst(const Input& in, const State &st, size_t &explored, const MoveAcceptor &AcceptMove, const std::vector<double> &weights) const
    {
      explored = 0;
      EvaluatedMove<Move, CostStructure> mv;
      FirstMove(in, st, mv.move);
      do
      {
        mv.cost = DeltaCostFunctionComponents(in, st, mv.move, weights);
        explored++;
        mv.is_valid = true;
        
        if (AcceptMove(mv.move, mv.cost))
          return mv; // mv passes the acceptance criterion
      } while (NextMove(in, st, mv.move));
      
      // exiting this loop means that there is no mv passing the acceptance criterion
      return EvaluatedMove<Move, CostStructure>::empty;
    }
    
    /**
     This method will select the best move in the exhaustive neighborhood exploration that
     matches with the criterion expressed by the functional object bool f(const Move& mv, CostStructure cost)
     */
    template <class Input, class State, class Move, class CostStructure>
    EvaluatedMove<Move, CostStructure> NeighborhoodExplorer<Input, State, Move, CostStructure>::SelectBest(const Input& in, const State &st, size_t &explored, const MoveAcceptor &AcceptMove, const std::vector<double> &weights) const
    {
      unsigned int number_of_bests = 0; // number of moves found with the same best value
      explored = 0;
      EvaluatedMove<Move, CostStructure> mv, best_move;
      FirstMove(in, st, mv.move);
      
      do
      {
        mv.cost = DeltaCostFunctionComponents(in, st, mv.move, weights);
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
            if (Random::Rand(0U, number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
              best_move = mv;
            number_of_bests++;
          }
        }
      } while (NextMove(in, st, mv.move));
      
      if (number_of_bests == 0)
        return EvaluatedMove<Move, CostStructure>::empty;
      
      return best_move;
    }
    
    /**
     This method will select the first move in the random neighborhood exploration that
     matches with the criterion expressed by the functional object bool f(const Move& mv, CostStructure cost)
     */
    template <class Input, class State, class Move, class CostStructure>
    EvaluatedMove<Move, CostStructure> NeighborhoodExplorer<Input, State, Move, CostStructure>::RandomFirst(const Input& in, const State &st, size_t samples, size_t &explored, const MoveAcceptor &AcceptMove, const std::vector<double> &weights) const
    {
      EvaluatedMove<Move, CostStructure> mv;
      explored = 0;
      while (explored < samples)
      {
        RandomMove(in, st, mv.move);
        mv.cost = DeltaCostFunctionComponents(in, st, mv.move, weights);
        mv.is_valid = true;
        explored++;
        if (AcceptMove(mv.move, mv.cost))
          return mv;
      }
      // exiting this loop means that there is no mv passing the acceptance criterion
      return EvaluatedMove<Move, CostStructure>::empty;
    }
    
    /**
     This method will select the best move in the exhaustive neighborhood exploration that
     matches with the criterion expressed by the functional object bool f(const Move& mv, CostStructure cost)
     */
    template <class Input, class State, class Move, class CostStructure>
    EvaluatedMove<Move, CostStructure> NeighborhoodExplorer<Input, State, Move, CostStructure>::RandomBest(const Input& in, const State &st, size_t samples, size_t &explored, const MoveAcceptor &AcceptMove, const std::vector<double> &weights) const
    {
      unsigned int number_of_bests = 0; // number of moves found with the same best value
      EvaluatedMove<Move, CostStructure> mv, best_move;
      
      explored = 0;
      while (explored < samples)
      {
        RandomMove(in, st, mv.move);
        mv.cost = DeltaCostFunctionComponents(in, st, mv.move, weights);
        mv.is_valid = true;
        explored++;
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
            if (Random::Rand(0U, number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
              best_move = mv;
            number_of_bests++;
          }
        }
      }
      
      if (number_of_bests == 0)
        return EvaluatedMove<Move, CostStructure>::empty;
      
      return best_move;
    }
  } // namespace Core
} // namespace EasyLocal

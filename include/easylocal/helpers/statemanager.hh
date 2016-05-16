#pragma once

#include <iostream>
#include <vector>
#include <cmath>
#include <stdexcept>

#include "easylocal/helpers/costcomponent.hh"
#include "easylocal/helpers/coststructure.hh"

namespace EasyLocal {
  
  namespace Core {      
    
    /**
     This constant multiplies the value of the Violations function in thehierarchical formulation of the Cost function (i.e., CostFunction(s) = HARD_WEIGHT * Violations(s) + Objective(s)).
     @todo The use of the global HARD_WEIGHT is a rough solution, waiting for an idea of a general mechanism for managing cost function weights.
     */
    
#if !defined(HARD_WEIGHT_SET)
    const int HARD_WEIGHT = 1000;
#define HARD_WEIGHT_SET
#endif
    
    /**
     @brief This component is responsible for all operations on the state which are
     independent of the neighborhood definition, such as generating a random state
     or computing the cost of a state.
     
     @tparam Input the class representing the problem input
     @tparam Output the class representing the problem output
     @tparam CostStructure the type of the cost structure to be used (by default an aggregated cost structure with distinct violations and objectives)
     
     @remarks no @ref Move template is supplied to this class.
     @ingroup Helpers
     */
    template <class Input, class State, class CostStructure = DefaultCostStructure<int>>
    class StateManager : public Printable
    {
    public:
      
      typedef typename CostStructure::CFtype CFtype;
      
      /**
       Print the configuration of the object (attached cost components)
       @param os the output stream where the description has to be printed
       */
      void Print(std::ostream& os = std::cout) const;
      
      /**
       Generates a random state
       @param st the state to be written
       */
      virtual void RandomState(State &st) = 0;
      
      /**
       Looks for the best state out of a given sample of random states.
       @param st state to be written
       @param samples number of states sampled
       @return a cost structure with the cost of the state @c st
       */
      virtual CostStructure SampleState(State &st, unsigned int samples);
      
      /**
       Generate a greedy state with a random component controlled by the
       parameters alpha and k.
       
       @note During the construction phase of GRASP one chooses, from an adaptive
       RCL (Restricted Candidate List), a solution element (e.g. be a boolean
       variable in the case of SAT) to set in the initial solution. The RCL contains
       the best components to set based on a greedy policy. In order to increase
       the number of optimal initial solutions and to improve the overall GRASP
       procedure, the RCL should contain more than the greedy component; @c alpha
       and @c k are used to control how many other components are included in
       the RCL.
       In particular, if the greedy component has value @c p, by setting @c alpha
       we can include in the RCL also components which has a value greater than
       @c alpha * @c p.
       Pretty much in the same way, @c k can be used to restrict the RCL to the
       best @c k components according to the greedy policy. In principle @c alpha
       and @c k should be used alternatively.
       
       @param st the state to be written
       @param alpha percentage of the value of the greedy component above which
       non-greedy component can be added to the GRASP's RCL
       @param k length of the GRASP's RCL
       
       @remarks this method is somehow specific for GRASP. The meaning of @c alpha
       and @k makes sense only when related to this approach.
       
       */
      virtual void GreedyState(State &st, double alpha, unsigned int k);
      
      /**
       Generate a greedy state.
       @note To be implemented in the application. Default behaviour is RandomState
       (MayRedef).
       */
      virtual void GreedyState(State &st);
      
      /**
       Compute the cost function calling the cost components.
       @param st the state to be evaluated
       @param an optional vector of weights for the cost components.
       @return all the components of the cost function in the given state
       
       @remarks The normal definition computes a weighted sum of the violation
       function and the objective function.
       
       @note It is rarely needed to redefine this method.
       */
      virtual CostStructure CostFunctionComponents(const State& st, const std::vector<double>& weights = std::vector<double>(0)) const;
      
      /**
       Check whether the lower bound of the cost function components has been reached. The
       tentative definition verifies whether the state costs are equal to zero.
       @return true if the lower bound of the cost function has been reached
       */
      virtual bool LowerBoundReached(const CostStructure& costs) const;
      
      /**
       Check whether the cost of the current state has reached the lower bound.
       By default calls @c LowerBoundReached(CostFunctionComponents(st)).
       @return true if the state is optimal w.r.t. the lower bound cost function
       has been reached
       @param st the state to be evaluated
       */
      virtual bool OptimalStateReached(const State& st) const;
      
      /**
       Add a component to the cost component array.
       @param cc the cost component to be added
       */
      void AddCostComponent(CostComponent<Input, State, CFtype>& cc);
      
      
      /**
       Clear the cost component array.
       */
      void ClearCostStructure();
      
      /**
       Compute the distance of two states (e.g. the Hamming distance).
       Currently used only by the @ref GeneralizedLocalSearchObserver.
       @return the distance, assumed always @c unsigned int
       */
      virtual unsigned int StateDistance(const State& st1, const State& st2) const;
      
      
      /**
       Check whether the state is consistent. In particular, should check whether
       the redundant data structures are consistent with the main ones. Used only
       for debugging purposes.
       */
      virtual bool CheckConsistency(const State& st) const = 0;
      
      /** Name of the state manager */
      const std::string name;
      
    protected:
      /**
       Build an StateManager object linked to the provided input.
       @param in a reference to an input object
       @param name is the name of the object
       */
      StateManager(const Input& in, std::string name);
      
      /** Destructor. */
      virtual ~StateManager() {}
      
      /**
       The set of the cost components. Hard and soft ones are all in this @c vector.
       */
      std::vector<CostComponent<Input, State, CFtype>*> cost_component;
      
      /** Input object. */
      const Input& in;
    };
    
    /* **************************************************************************
     * Implementation
     * **************************************************************************/
    
    template <class Input, class State, class CostStructure>
    StateManager<Input, State, CostStructure>::StateManager(const Input& in, std::string name)
    :  name(name), in(in)
    {}
    
    template <class Input, class State, class CostStructure>
    void StateManager<Input, State, CostStructure>::Print(std::ostream& os) const
    {
      os  << "State Manager: " + name << std::endl;
      os  << "Violations:" << std::endl;
      for (size_t i = 0; i < cost_component.size(); i++)
        if (cost_component[i]->IsHard())
          cost_component[i]->Print(os);
      os  << "Objective:" << std::endl;
      for (size_t i = 0; i < cost_component.size(); i++)
        if (cost_component[i]->IsSoft())
          cost_component[i]->Print(os);
    }
    
    template <class Input, class State, class CostStructure>
    CostStructure StateManager<Input, State, CostStructure>::SampleState(State &st,
                                                           unsigned int samples)
    {
      unsigned int s = 1;
      RandomState(st);
      CostStructure cost = CostFunctionComponents(st);
      State best_state(in);
      best_state = st;
      CostStructure best_cost = cost;
      while (s < samples)
      {
        RandomState(st);
        cost = CostFunctionComponents(st);
        if (cost < best_cost)
        {
          best_state = st;
          best_cost = cost;
        }
        s++;
      }
      st = best_state;
      return best_cost;
    }
    
    template <class Input, class State, class CostStructure>
    void StateManager<Input, State, CostStructure>::GreedyState(State &st, double alpha,
                                                         unsigned int k)
    {
      GreedyState(st);
    }
    
    template <class Input, class State, class CostStructure>
    void StateManager<Input, State, CostStructure>::GreedyState(State &st)
    {
      throw std::runtime_error("For using this feature GreedyState must be implemented in the concrete class!");
    }
    
    template <class Input, class State, class CostStructure>
    CostStructure StateManager<Input, State, CostStructure>::CostFunctionComponents(const State& st, const std::vector<double>& weights) const
    {
      CFtype hard_cost = 0, soft_cost = 0;
      double weighted_cost = 0.0;
      std::vector<CFtype> cost_function(CostComponent<Input, State, CFtype>::CostComponents(), (CFtype)0);
      for (size_t i = 0; i < cost_component.size(); i++)
      {
        CFtype current_cost = cost_function[cost_component[i]->Index()] = cost_component[i]->Cost(st);
        if (cost_component[i]->IsHard())
        {
          hard_cost += current_cost;
          if (!weights.empty())
            weighted_cost += HARD_WEIGHT * weights[cost_component[i]->Index()] * current_cost;
        }
        else
        {
          soft_cost += current_cost;
          if (!weights.empty())
            weighted_cost += weights[cost_component[i]->Index()] * current_cost;
        }
      }
      
      if (!weights.empty())
        return CostStructure(HARD_WEIGHT * hard_cost + soft_cost, weighted_cost, hard_cost, soft_cost, cost_function);
      else
        return CostStructure(HARD_WEIGHT * hard_cost + soft_cost, hard_cost, soft_cost, cost_function);
    }
    
    template <class Input, class State, class CostStructure>
    bool StateManager<Input, State, CostStructure>::LowerBoundReached(const CostStructure& costs) const
    {
      return costs == 0;
    }
    
    template <class Input, class State, class CostStructure>
    bool StateManager<Input, State, CostStructure>::OptimalStateReached(const State& st) const
    {
      return LowerBoundReached(CostFunctionComponents(st));
    }
    
    template <class Input, class State, class CostStructure>
    void StateManager<Input, State, CostStructure>::AddCostComponent(CostComponent<Input, State, CFtype>& cc)
    {
      cost_component.push_back(&cc);
    }    
    
    template <class Input, class State, class CostStructure>
    unsigned int StateManager<Input, State, CostStructure>::StateDistance(const State& st1,
                                                                   const State& st2) const
    {
      throw std::runtime_error("In order to use this feature StateDistance must be implemented in the concrete class!");
      return 0;
    }
  }
}


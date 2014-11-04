#if !defined(_STATE_MANAGER_HH_)
#define _STATE_MANAGER_HH_

#include <iostream>
#include <vector>
#include <cmath>
#include <stdexcept>

#include "easylocal/helpers/costcomponent.hh"
#include "easylocal/utils/types.hh"

namespace EasyLocal {
  
  namespace Core {
    
    template <typename CFtype>
    struct CostStructure
    {
      CostStructure() : total(0), violations(0), objective(0), all_components(0), weighted(0.0), is_weighted(false) {}
      CostStructure(CFtype total, CFtype violations, CFtype objective, const std::vector<CFtype>& all_components) : total(total), violations(violations), objective(objective), all_components(all_components), weighted(total), is_weighted(false) {}
      CostStructure(CFtype total, double weighted, CFtype violations, CFtype objective, const std::vector<CFtype>& all_components) : total(total), violations(violations), objective(objective), all_components(all_components), weighted(weighted), is_weighted(true) {}
      
      
      CFtype total, violations, objective;
      std::vector<CFtype> all_components;
      double weighted;
      
      bool is_weighted;
      
      CostStructure operator+(const CostStructure& other)
      {
        CostStructure res = *this;
        res.total += other.total;
        res.violations += other.violations;
        res.objective += other.objective;
        for (size_t i = 0; i < other.all_components.size(); i++)
          res.all_components[i] += other.all_components[i];
        return res;
      }
      
      CostStructure& operator+=(const CostStructure& other)
      {
        this->total += other.total;
        this->violations += other.violations;
        this->objective += other.objective;
        for (size_t i = 0; i < other.all_components.size(); i++)
          this->all_components[i] += other.all_components[i];
        return *this;
      }
      
      explicit operator double() const
      {
        if (is_weighted)
          return weighted;
        else
          return (double)total;
      }      
    };
    
    template <class CFtype>
    bool operator<(const CostStructure<CFtype>& cs1, const CostStructure<CFtype>& cs2)
    {
      if (cs1.is_weighted && cs2.is_weighted)
        return LessThan(cs1.weighted, cs2.weighted);
      return LessThan(cs1.total, cs2.total);
    }
    
    template <class CFtype>
    bool operator<(CFtype c1, const CostStructure<CFtype>& cs2)
    {
      if (cs2.is_weighted)
        return LessThan((double)c1, cs2.weighted);
      return LessThan(c1, cs2.total);
    }
    
    template <class CFtype>
    bool operator<(const CostStructure<CFtype>& cs1, CFtype c2)
    {
      if (cs1.is_weighted)
        return LessThan(cs1.weighted, (double)c2);
      return LessThan(cs1.total, c2);
    }
    
    template <class CFtype, typename OtherType>
    bool operator<(OtherType c1, const CostStructure<CFtype>& cs2)
    {
      if (cs2.is_weighted)
        return LessThan((double)c1, cs2.weighted);
      return LessThan((CFtype)c1, cs2.total);
    }
    
    template <class CFtype, typename OtherType>
    bool operator<(const CostStructure<CFtype>& cs1, OtherType c2)
    {
      if (cs1.is_weighted)
        return LessThan(cs1.weighted, (double)c2);
      return LessThan(cs1.total, (CFtype)c2);
    }
    
    template <class CFtype>
    bool operator<=(const CostStructure<CFtype>& cs1, const CostStructure<CFtype>& cs2)
    {
      if (cs1.is_weighted && cs2.is_weighted)
        return LessThanOrEqualTo(cs1.weighted, cs2.weighted);
      return LessThanOrEqualTo(cs1.total, cs2.total);
    }
    
    template <class CFtype>
    bool operator<=(CFtype c1, const CostStructure<CFtype>& cs2)
    {
      if (cs2.is_weighted)
        return LessThanOrEqualTo((double)c1, cs2.weighted);
      return LessThanOrEqualTo(c1, cs2.total);
    }
    
    template <class CFtype>
    bool operator<=(const CostStructure<CFtype>& cs1, CFtype c2)
    {
      if (cs1.is_weighted)
        return LessThanOrEqualTo(cs1.weighted, (double)c2);
      return LessThanOrEqualTo(cs1.total, c2);
    }
    
    template <class CFtype, typename OtherType>
    bool operator<=(OtherType c1, const CostStructure<CFtype>& cs2)
    {
      if (cs2.is_weighted)
        return LessThanOrEqualTo((double)c1, cs2.weighted);
      return LessThanOrEqualTo((CFtype)c1, cs2.total);
    }
    
    template <class CFtype, typename OtherType>
    bool operator<=(const CostStructure<CFtype>& cs1, OtherType c2)
    {
      if (cs1.is_weighted)
        return LessThanOrEqualTo(cs1.weighted, (double)c2);
      return LessThanOrEqualTo(cs1.total, (CFtype)c2);
    }
    
    template <class CFtype>
    bool operator==(const CostStructure<CFtype>& cs1, const CostStructure<CFtype>& cs2)
    {
      if (cs1.is_weighted && cs2.is_weighted)
        return EqualTo(cs1.weighted, cs2.weighted);
      return EqualTo(cs1.total, cs2.total);
    }
    
    template <class CFtype>
    bool operator==(CFtype c1, const CostStructure<CFtype>& cs2)
    {
      if (cs2.is_weighted)
        return EqualTo((double)c1, cs2.weighted);
      return EqualTo(c1, cs2.total);
    }
    
    template <class CFtype>
    bool operator==(const CostStructure<CFtype>& cs1, CFtype c2)
    {
      if (cs1.is_weighted)
        return EqualTo(cs1.weighted, (double)c2);
      return EqualTo(cs1.total, c2);
    }
    
    template <class CFtype, typename OtherType>
    bool operator==(OtherType c1, const CostStructure<CFtype>& cs2)
    {
      if (cs2.is_weighted)
        return EqualTo((double)c1, cs2.weighted);
      return EqualTo((CFtype)c1, cs2.total);
    }
    
    template <class CFtype, typename OtherType>
    bool operator==(const CostStructure<CFtype>& cs1, OtherType c2)
    {
      if (cs1.is_weighted)
        return EqualTo(cs1.weighted, (double)c2);
      return EqualTo(cs1.total, (CFtype)c2);
    }
    
    template <class CFtype>
    bool operator>=(const CostStructure<CFtype>& cs1, const CostStructure<CFtype>& cs2)
    {
      if (cs1.is_weighted && cs2.is_weighted)
        return GreaterThanOrEqualTo(cs1.weighted, cs2.weighted);
      return GreaterThanOrEqualTo(cs1.total, cs2.total);
    }
    
    template <class CFtype>
    bool operator>=(CFtype c1, const CostStructure<CFtype>& cs2)
    {
      if (cs2.is_weighted)
        return GreaterThanOrEqualTo((double)c1, cs2.weighted);
      return GreaterThanOrEqualTo(c1, cs2.total);
    }
    
    template <class CFtype>
    bool operator>=(const CostStructure<CFtype>& cs1, CFtype c2)
    {
      if (cs1.is_weighted)
        return GreaterThanOrEqualTo(cs1.weighted, (double)c2);
      return GreaterThanOrEqualTo(cs1.total, c2);
    }
    
    template <class CFtype, typename OtherType>
    bool operator>=(OtherType c1, const CostStructure<CFtype>& cs2)
    {
      if (cs2.is_weighted)
        return GreaterThanOrEqualTo((double)c1, cs2.weighted);
      return GreaterThanOrEqualTo((CFtype)c1, cs2.total);
    }
    
    template <class CFtype, typename OtherType>
    bool operator>=(const CostStructure<CFtype>& cs1, OtherType c2)
    {
      if (cs1.is_weighted)
        return GreaterThanOrEqualTo(cs1.weighted, (double)c2);
      return GreaterThanOrEqualTo(cs1.total, (CFtype)c2);
    }
    
    template <class CFtype>
    bool operator>(const CostStructure<CFtype>& cs1, const CostStructure<CFtype>& cs2)
    {
      if (cs1.is_weighted && cs2.is_weighted)
        return GreaterThan(cs1.weighted, cs2.weighted);
      return GreaterThan(cs1.total, cs2.total);
    }
    
    template <class CFtype>
    bool operator>(CFtype c1, const CostStructure<CFtype>& cs2)
    {
      if (cs2.is_weighted)
        return GreaterThan((double)c1, cs2.weighted);
      return GreaterThan(c1, cs2.total);
    }
    
    template <class CFtype>
    bool operator>(const CostStructure<CFtype>& cs1, CFtype c2)
    {
      if (cs1.is_weighted)
        return GreaterThan(cs1.weighted, (double)c2);
      return GreaterThan(cs1.total, c2);
    }
    
    template <class CFtype, typename OtherType>
    bool operator>(OtherType c1, const CostStructure<CFtype>& cs2)
    {
      if (cs2.is_weighted)
        return GreaterThan((double)c1, cs2.weighted);
      return GreaterThan((CFtype)c1, cs2.total);
    }
    
    template <class CFtype, typename OtherType>
    bool operator>(const CostStructure<CFtype>& cs1, OtherType c2)
    {
      if (cs1.is_weighted)
        return GreaterThan(cs1.weighted, (double)c2);
      return GreaterThan(cs1.total, (CFtype)c2);
    }
    
    template <class CFtype>
    bool operator!=(const CostStructure<CFtype>& cs1, const CostStructure<CFtype>& cs2)
    {
      if (cs1.is_weighted && cs2.is_weighted)
        return !EqualTo(cs1.weighted, cs2.weighted);
      return !EqualTo(cs1.total, cs2.total);
    }
    
    template <class CFtype>
    bool operator!=(CFtype c1, const CostStructure<CFtype>& cs2)
    {
      if (cs2.is_weighted)
        return !EqualTo((double)c1, cs2.weighted);
      return !EqualTo(c1, cs2.total);
    }
    
    template <class CFtype>
    bool operator!=(const CostStructure<CFtype>& cs1, CFtype c2)
    {
      if (cs1.is_weighted)
        return !EqualTo(cs1.weighted, (double)c2);
      return !EqualTo(cs1.total, c2);
    }
    
    template <class CFtype, typename OtherType>
    bool operator!=(OtherType c1, const CostStructure<CFtype>& cs2)
    {
      if (cs2.is_weighted)
        return !EqualTo((double)c1, cs2.weighted);
      return !EqualTo((CFtype)c1, cs2.total);
    }
    
    template <class CFtype, typename OtherType>
    bool operator!=(const CostStructure<CFtype>& cs1, OtherType c2)
    {
      if (cs1.is_weighted)
        return !EqualTo(cs1.weighted, (double)c2);
      return !EqualTo(cs1.total, (CFtype)c2);
    }
    
    template <typename CFtype>
    std::ostream& operator<<(std::ostream& os, const CostStructure<CFtype>& cc)
    {
      os << cc.total << "(viol: " << cc.violations << ", obj: " << cc.objective << ", {";
      for (size_t i = 0; i < cc.all_components.size(); i++)
      {
        if (i > 0)
          os << ", ";
        os << cc.all_components[i];
      }
      os << "})";
      
      return os;
    }
    
    
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
     @tparam CFtype the type (codomain) of the objective function (typically int)
     
     @remarks no @ref Move template is supplied to this class.
     @ingroup Helpers
     */
    template <class Input, class State, typename CFtype = int>
    class StateManager : public Printable
    {
    public:
      
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
       */
      virtual CFtype SampleState(State &st, unsigned int samples);
      
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
       @return the value of the cost function in the given state (hard + soft costs)
       
       @remarks The normal definition computes a weighted sum of the violation
       function and the objective function.
       
       @note It is rarely needed to redefine this method.
       */
      virtual CFtype CostFunction(const State& st) const;
      
      /**
       Compute the cost function calling the cost components.
       @param st the state to be evaluated
       @param an optional vector of weights for the cost components.
       @return all the components of the cost function in the given state
       
       @remarks The normal definition computes a weighted sum of the violation
       function and the objective function.
       
       @note It is rarely needed to redefine this method.
       */
      virtual CostStructure<CFtype> CostFunctionComponents(const State& st, const std::vector<double>& weights = std::vector<double>(0)) const;
      
      /**
       Compute the violations by calling the hard cost components (it is rarely
       needed to redefine it).
       @param st the state to be evaluated
       @return the violations (hard costs)
       
       @note It is rarely needed to redefine this method.
       */
      virtual CFtype Violations(const State& st) const;
      
      /**
       Compute the objective function calling the soft cost components.
       @param st the state to be evaluated
       @return the objective (soft costs)
       
       @note It is rarely needed to redefine this method.
       */
      virtual CFtype Objective(const State& st) const;
      
      /**
       Check whether the lower bound of the cost function has been reached. The
       tentative definition verifies whether the state cost is equal to zero.
       @return true if the lower bound of the cost function has been reached
       @param st the state to be evaluated
       */
      virtual bool LowerBoundReached(const CFtype& fvalue) const;
      
      /**
       Check whether the cost of the current state has reached the lower bound.
       By default calls @c LowerBoundReached(CostFunction(st)).
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
      
      /**
       Access a cost component by index
       @return the reference to a cost component
       @param i the index of the cost component
       */
      CostComponent<Input, State, CFtype>& GetCostComponent(unsigned int i) const { return *(cost_component[i]); }
      
      /**
       Get the number of registered cost components
       @return the numer of cost components
       */
      size_t CostComponents() const { return cost_component.size(); }
      
      /**
       Compute the cost relative to a specific cost component.
       @return the cost of a specific cost component
       @param i the index of the cost component
       @param st the state to be evaluated
       */
      CFtype Cost(const State& st, unsigned int i) const { return cost_component[i]->Cost(st); }
      
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
      std::vector<CostComponent<Input, State, CFtype>* > cost_component;
      
      /** Input object. */
      const Input& in;
    };
    
    /* **************************************************************************
     * Implementation
     * **************************************************************************/
    
    template <class Input, class State, typename CFtype>
    StateManager<Input, State, CFtype>::StateManager(const Input& i, std::string e_name)
    :  name(e_name), in(i)
    {
      
    }
    
    template <class Input, class State, typename CFtype>
    void StateManager<Input, State, CFtype>::Print(std::ostream& os) const
    {
      os  << "State Manager: " + name << std::endl;
      os  << "Violations:" << std::endl;
      for (unsigned int i = 0; i < cost_component.size(); i++)
        if (cost_component[i]->IsHard())
          cost_component[i]->Print(os);
      os  << "Objective:" << std::endl;
      for (unsigned int i = 0; i < cost_component.size(); i++)
        if (cost_component[i]->IsSoft())
          cost_component[i]->Print(os);
    }
    
    template <class Input, class State, typename CFtype>
    CFtype StateManager<Input, State, CFtype>::SampleState(State &st,
                                                           unsigned int samples)
    {
      unsigned int s = 1;
      RandomState(st);
      CFtype cost = CostFunction(st);
      State best_state(in);
      best_state = st;
      CFtype best_cost = cost;
      while (s < samples)
      {
        RandomState(st);
        cost = CostFunction(st);
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
    
    template <class Input, class State, typename CFtype>
    void StateManager<Input, State, CFtype>::GreedyState(State &st, double alpha,
                                                         unsigned int k)
    {
      GreedyState(st);
    }
    
    template <class Input, class State, typename CFtype>
    void StateManager<Input, State, CFtype>::GreedyState(State &st)
    {
      throw std::runtime_error("For using this feature GreedyState must be implemented in the concrete class!");
    }
    
    template <class Input, class State, typename CFtype>
    CFtype StateManager<Input, State, CFtype>::CostFunction(const State& st) const
    {
      CFtype hard_cost = 0, soft_cost = 0;
      for (unsigned int i = 0; i < cost_component.size(); i++)
        if (cost_component[i]->IsHard())
          hard_cost += cost_component[i]->Cost(st);
        else
          soft_cost += cost_component[i]->Cost(st);
      
      return HARD_WEIGHT * hard_cost  + soft_cost;
    }
    
    template <class Input, class State, typename CFtype>
    CostStructure<CFtype> StateManager<Input, State, CFtype>::CostFunctionComponents(const State& st, const std::vector<double>& weights) const
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
        return CostStructure<CFtype>(HARD_WEIGHT * hard_cost + soft_cost, weighted_cost, hard_cost, soft_cost, cost_function);
      else
        return CostStructure<CFtype>(HARD_WEIGHT * hard_cost + soft_cost, hard_cost, soft_cost, cost_function);
    }
    
    template <class Input, class State, typename CFtype>
    bool StateManager<Input, State, CFtype>::LowerBoundReached(const CFtype& fvalue) const
    {
      return IsZero(fvalue);
    }
    
    template <class Input, class State, typename CFtype>
    bool StateManager<Input, State, CFtype>::OptimalStateReached(const State& st) const
    {
      return LowerBoundReached(CostFunction(st));
    }
    
    template <class Input, class State, typename CFtype>
    CFtype StateManager<Input, State, CFtype>::Violations(const State& st) const
    {
      CFtype cost = 0;
      for (unsigned int i = 0; i < cost_component.size(); i++)
        if (cost_component[i]->IsHard())
          cost += cost_component[i]->Cost(st);
      return cost;
    }
    
    template <class Input, class State, typename CFtype>
    CFtype StateManager<Input, State, CFtype>::Objective(const State& st) const
    {
      CFtype cost = 0;
      for (unsigned int i = 0; i < cost_component.size(); i++)
        if (cost_component[i]->IsSoft())
          cost += cost_component[i]->Cost(st);
      return cost;
    }
    
    template <class Input, class State, typename CFtype>
    void StateManager<Input, State, CFtype>::AddCostComponent(CostComponent<Input, State, CFtype>& cc)
    {
      cost_component.push_back(&cc);
    }
    
    template <class Input, class State, typename CFtype>
    void StateManager<Input, State, CFtype>::ClearCostStructure()
    {
      cost_component.clear();
    }
    
    template <class Input, class State, typename CFtype>
    unsigned int StateManager<Input, State, CFtype>::StateDistance(const State& st1,
                                                                   const State& st2) const
    {
      throw std::runtime_error("For using this feature StateDistance must be implemented in the concrete class!");
      return 0;
    }
  }
}

#endif // _STATE_MANAGER_HH_

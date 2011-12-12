#if !defined(_STATE_MANAGER_HH_)
#define _STATE_MANAGER_HH_

#include <iostream>
#include <vector>
#include <cmath>
#include <helpers/CostComponent.hh>
#include <utils/Types.hh>
#include <stdexcept>

/** 
  This constant multiplies the value of the Violations function in the hierarchical 
  formulation of the Cost function (i.e., CostFunction(s) = HARD_WEIGHT * Violations(s) 
  + Objective(s)).

  @todo The use of the global HARD_WEIGHT is a rough solution, waiting for an idea of 
  a general mechanism for managing cost function weights.
*/

#if !defined(HARD_WEIGHT_SET)
const int HARD_WEIGHT = 1000;
#define HARD_WEIGHT_SET
#endif

/** 
  The State Manager is responsible for all operations on the state
  which are independent of the neighborhood definition, such as
  generating a random state, and computing the cost of a state.
  No @c Move template is supplied to it.  

  @tparam Input User defined input class
  @tparam Output User defined output class
  @tparam CFtype Codomain of the objective function (int by default)
  @ingroup Helpers
*/
template <class Input, class State, typename CFtype = int>
class StateManager
{
public:
  /** Prints the configuration of the object (attached cost components)
      @param os Output stream 
  */
  void Print(std::ostream& os = std::cout) const;
  /** Generates a random state.
  @note To be implemented in the application (MustDef).
  @param st state to be written */
  virtual void RandomState(State &st) = 0;

  /**
     Looks for the best state out of a given number of random
     states.
     
     @param st state to be written 
     @param samples number of states sampled
  */
  virtual CFtype SampleState(State &st, unsigned int samples);


  /** 
      Generate a greedy state with a random component controlled by the parameters alpha and k

      @todo Complete the documentation of alpha and k
  */
  virtual void GreedyState(State &st, double alpha, unsigned int k);

  /** 
      Generate a greedy state.
      @note To be implemented in the application. Default behaviour is RandomState (MayRedef).
  */
  virtual void GreedyState(State &st);

  /**
     Compute the cost function calling the cost components. 
     The normal definition computes a weighted sum of the violation
     function and the objective function (rarely it is needed to
     redefine it) 

     @param st the state to be evaluated 
     @return the value of the cost function in the given state (hard + soft costs)
   */
  virtual CFtype CostFunction(const State& st) const;

  /**
     Compute the violations calling the hard cost components (rarely it is needed to redefine it)
     @param st the state to be evaluated
     @return the violations (hard costs)
   */
  virtual CFtype Violations(const State& st) const;

  /**
     Compute the objective function calling the soft cost components (rarely it is needed to redefine it)
     @param st the state to be evaluated
     @return the objective (soft costs)
   */
  virtual CFtype Objective(const State& st) const;

  /** Checks whether the lower bound of the cost function has been
      reached. The tentative definition verifies whether the state
      cost is equal to zero.  @return true if the lower bound of the
      cost function has reached, false otherwise 
      @param st the state to be evaluated
  */
  virtual bool LowerBoundReached(const CFtype& fvalue) const;

  /** Checks whether the cost of the current state has
      reached the lower bound. By default calls @c LowerBoundReached(CostFunction(st)).
      @return true if the state is optimal w.r.t. the lower bound
      cost function has reached, false otherwise 
      @param st the state to be evaluated
  */
  virtual bool OptimalStateReached(const State& st) const;

  /**
     Adds a component to the cost component array
     @param cc the cost component to be added
  */
  void AddCostComponent(CostComponent<Input,State,CFtype>& cc);


  /**
     Clears the cost component array
  */
  void ClearCostComponents();

  /**
     Computes the distance of two states (for example the Hamming distance). 
     Currently not used by any solver. Used only by the @ref GeneralizedLocalSearchObserver.
     @return the distance, assumed always @c unsigned int
  */
  virtual unsigned int StateDistance(const State& st1, const State& st2) const;

  
  /**
     Checks whether the state is consistent. In particular, should
     check whether the redundant data structures are consistent with
     the main ones. Used only for debugging purposes.
  */
  virtual bool CheckConsistency(const State& st) const = 0;
  
  /**
     @return the reference to a cost component
     @param i the index of the cost component
  */
  CostComponent<Input, State,CFtype>& GetCostComponent(unsigned int i) const { return *(cost_component[i]); }

  /**
     @return the numer of cost components
  */
  size_t CostComponents() const { return cost_component.size(); }

  /**
     @return the cost of a specific cost component
     @param i the index of the cost component
     @param st the state to be evaluated
  */
  CFtype Cost(const State& st, unsigned int i) const { return cost_component[i]->Cost(st); }

  const std::string name;
protected:
  /**
     Builds an StateManager object linked to the provided input.
     
     @param in a reference to an input object
     @param name is the name of the object
  */
  StateManager(const Input& in, std::string name);
  virtual ~StateManager() {}

  /**
     The set of the cost components. Hard and soft ones are all in this @c vector.
  */
  std::vector<CostComponent<Input,State,CFtype>* > cost_component;
  const Input& in;
};

// -----------------------------------------------------------------------
// Implementation
// ----------------------------------------------------------------------

/**
Builds a state manager object linked to the provided input.
 
 @param in a reference to an input object
 @param name a name for the state manager
 */
template <class Input, class State, typename CFtype>
StateManager<Input,State,CFtype>::StateManager(const Input& i, std::string e_name)
  :  name(e_name), in(i)
{}

template <class Input, class State, typename CFtype>
void StateManager<Input,State,CFtype>::Print(std::ostream& os) const
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
CFtype StateManager<Input,State,CFtype>::SampleState(State &st,
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
void StateManager<Input,State,CFtype>::GreedyState(State &st, double alpha, unsigned int k)
{
   GreedyState(st);
}

template <class Input, class State, typename CFtype>
void StateManager<Input,State,CFtype>::GreedyState(State &st)
{
  throw std::runtime_error("For using this feature GreedyState must be implemented in the concrete class!");
}

template <class Input, class State, typename CFtype>
CFtype StateManager<Input,State,CFtype>::CostFunction(const State& st) const
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
bool StateManager<Input,State,CFtype>::LowerBoundReached(const CFtype& fvalue) const
{ 
  return IsZero(fvalue); 
}

template <class Input, class State, typename CFtype>
bool StateManager<Input,State,CFtype>::OptimalStateReached(const State& st) const
{ 
  return LowerBoundReached(CostFunction(st)); 
}

template <class Input, class State, typename CFtype>
CFtype StateManager<Input,State,CFtype>::Violations(const State& st) const
{
  CFtype cost = 0;
  for (unsigned int i = 0; i < cost_component.size(); i++)
    if (cost_component[i]->IsHard())
      cost += cost_component[i]->Cost(st);
  return cost;
}

template <class Input, class State, typename CFtype>
CFtype StateManager<Input,State,CFtype>::Objective(const State& st) const
{
  CFtype cost = 0;
  for (unsigned int i = 0; i < cost_component.size(); i++)
    if (cost_component[i]->IsSoft())
      cost += cost_component[i]->Cost(st);
  return cost;
}

template <class Input, class State, typename CFtype>
void StateManager<Input,State,CFtype>::AddCostComponent(CostComponent<Input,State,CFtype>& cc)
{
  cost_component.push_back(&cc);
}

template <class Input, class State, typename CFtype>
void StateManager<Input,State,CFtype>::ClearCostComponents()
{
  cost_component.clear();
}

template <class Input, class State, typename CFtype>
unsigned int StateManager<Input,State,CFtype>::StateDistance(const State& st1, const State& st2) const
{ 
  throw std::runtime_error("For using this feature StateDistance must be implemented in the concrete class!");
  return 0;
}

#endif // _STATE_MANAGER_HH_

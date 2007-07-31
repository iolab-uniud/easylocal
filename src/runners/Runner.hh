#ifndef RUNNER_HH_
#define RUNNER_HH_

#include "AbstractRunner.hh"
#include "../helpers/StateManager.hh"
#include <set>
#ifdef EASYLOCAL_PTHREADS
#include <pthread.h>
#endif

/** @defgroup Runners Runner classes
    Runner classes are the algorithmic core of the framework. They are 
    responsible for performing a run of a local search technique, 
    starting from an initial state and leading to a final one. 
*/ 

template <class Input, class State, typename CFtype = int>
class Runner
            : public AbstractRunner<Input, State,CFtype>
{
public:
    // Runner interface
    void SetState(const State& st);
    const State& GetState() const;
    CFtype GetStateCost() const;
    void Go() throw(EasyLocalException);
    void Step(unsigned int n) throw(EasyLocalException);
    void ComputeCost();
    unsigned long GetIterationsPerformed() const;
    unsigned long GetMaxIteration() const;
    void SetMaxIteration(unsigned long max);
    virtual void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout) = 0;
    virtual void Check() const throw(EasyLocalException);
  virtual bool LowerBoundReached() const;
protected:
    Runner(const Input& i, StateManager<Input,State,CFtype>& sm);
    /* state manipulations */
    virtual void GoCheck() const throw(EasyLocalException) = 0;
    /** Actions to be perfomed at the beginning of the run. */
    virtual void InitializeRun();
    /** Actions to be performed at the end of the run. */
    virtual void TerminateRun();
    virtual void UpdateIterationCounter();
    bool MaxIterationExpired() const;
    /** Encodes the criterion used to stop the search. */
    virtual bool StopCriterion() = 0;
    /** Encodes the criterion used to select the move at each step. */
    virtual void SelectMove() = 0;
    /** Verifies whether the move selected could be performed. */
    virtual bool AcceptableMove();
    /** Actually performs the move. */
    virtual void MakeMove() = 0;
    /** Stores the move and updates the related data. */
    virtual void StoreMove() = 0;
    virtual void UpdateStateCost() = 0;
    // input
    const Input& in; /**< A pointer to the input object. */
    // helpers
    StateManager<Input, State,CFtype>& sm; /**< A pointer to the attached
    	state manager. */

    // state data
    State current_state; /**< The current state object. */
    CFtype current_state_cost; /**< The cost of the current state. */
    bool current_state_set; /**< A flag that whether the current state is set.
          It is so until a new input is given. */

    State best_state;         /**< The best state object. */
    CFtype best_state_cost;   /**< The cost of the best state. */

    unsigned long iteration_of_best; /**< The iteration when the best
    	state has found. */
    unsigned long number_of_iterations; /**< The overall number of iterations
    	   performed. */
    unsigned long start_iteration;
    unsigned long max_iteration; /**< The maximum number of iterations
        allowed. */
#ifdef EASYLOCAL_PTHREADS
public:
    pthread_t GoThread() throw(EasyLocalException);
protected:
    static void* _pthread_start_function(void *);
#endif
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
  Creates a move runner and links it to a given state manager, neighborhood
  explorer and input objects. In addition, it sets its name and type to
  the given values.

  @param sm a pointer to a compatible state manager
  @param ne a pointer to a compatible neighborhood explorer
  @param in a pointer to the input object
  @param name the name of the runner
*/
template <class Input, class State, typename CFtype>
Runner<Input,State,CFtype>::Runner(const Input& i, StateManager<Input,State,CFtype>& e_sm)
        : in(i), sm(e_sm),
        current_state(in), current_state_set(false),
        best_state(in),
        number_of_iterations(0), max_iteration(ULONG_MAX)
{}


/**
   Checks whether the object state is consistent with all the related
   objects.
*/
template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::Check() const
throw(EasyLocalException)
{}

#ifdef EASYLOCAL_PTHREADS
template <class Input, class State, typename CFtype>
pthread_t Runner<Input,State,CFtype>::GoThread() throw(EasyLocalException)
{
    pthread_t id;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
		
    pthread_create(&id, &attr, Runner<Input,State,CFtype>::_pthread_start_function, this);
 
    pthread_attr_destroy(&attr);
     
    return id;
}

template <class Input, class State, typename CFtype>
void* Runner<Input,State,CFtype>::_pthread_start_function(void *obj)
{
    Runner<Input,State,CFtype>* r = static_cast<Runner<Input,State,CFtype>*>(obj);
    r->Go();
    
    return NULL;
}
#endif


/**
   Sets the internal state of the runner to the value passed as parameter.

   @param s the state to become the current state of the runner
*/
template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::SetState(const State& s)
{
    current_state = s;
    current_state_cost = sm.CostFunction(current_state);
    current_state_set = true;
    best_state = current_state;
    best_state_cost = current_state_cost;
}

/**
   Retrieves the state of the runner.
   
   @return the current state of the runner
*/
template <class Input, class State, typename CFtype>
const State& Runner<Input,State,CFtype>::GetState() const
{
    return best_state;
}

/**
    Returns the cost of the state
    @return the cost of the state
*/
template <class Input, class State, typename CFtype>
CFtype Runner<Input,State,CFtype>::GetStateCost() const
{
    return best_state_cost;
}

/**
    Computes explicitely the cost of the current state (used 
    at the beginning of a run for consistency purpose).
*/
template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::ComputeCost()
{
    current_state_cost = sm.CostFunction(current_state);
}

/**
   Returns the number of iterations executed.

   @return the number of iterations performed by the runner
*/
template <class Input, class State, typename CFtype>
unsigned long Runner<Input,State,CFtype>::GetIterationsPerformed() const
{
    return number_of_iterations;
}

/**
   Returns the maximum value of iterations allowed for the runner.

   @return the maximum value of iterations allowed
*/
template <class Input, class State, typename CFtype>
unsigned long Runner<Input,State,CFtype>::GetMaxIteration() const
{
    return max_iteration;
}

/**
   Sets a bound on the maximum number of iterations allowed for the runner.
   
   @param max the maximum number of iterations allowed */
template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::SetMaxIteration(unsigned long max)
{
    max_iteration = max;
}

template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::TerminateRun()
{
#ifdef EASYLOCAL_PTHREADS
	StoppableObject::Terminating();
#endif
}

/**
   Performs a full run of a local search method.
 */
template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::Go()
throw(EasyLocalException)
{
    GoCheck();
    InitializeRun();
    while (!MaxIterationExpired() && !StopCriterion()
            && !LowerBoundReached() && !this->Timeout())
    {
        UpdateIterationCounter();
        SelectMove();
        if (AcceptableMove())
        {
            MakeMove();
            UpdateStateCost();
            StoreMove();
        }
    }
    TerminateRun();
}

template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::GoCheck() const
throw(EasyLocalException)
{
    if (!current_state_set)
        throw EasyLocalException("Current State not set in object " + this->GetName(),
                                 std::string(__FILE__), __LINE__);
}


/**
   Performs a given number of steps of the local search strategy.

   @param n the number of steps
*/
template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::Step(unsigned int n)
throw(EasyLocalException)
{
    GoCheck();
    for (unsigned int i = 0; i < n; i++)
    {
        UpdateIterationCounter();
        SelectMove();
        if (AcceptableMove())
        {
            MakeMove();
            UpdateStateCost();
            StoreMove();
            if (LowerBoundReached())
                break;
        }
    }
}

/**
   Updates the counter that tracks the number of iterations elapsed.
*/
template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::UpdateIterationCounter()
{
    number_of_iterations++;
}

/**
   Verifies whether the upper bound on the number of iterations
   allowed for the strategy has been reached.

   @return true if the maximum number of iteration has been reached, false
   otherwise
*/
template <class Input, class State, typename CFtype>
bool Runner<Input,State,CFtype>::MaxIterationExpired() const
{
    return number_of_iterations > max_iteration;
}

/**
    Checks whether the selected move can be performed.
    Its tentative definition simply returns true
*/
template <class Input, class State, typename CFtype>
bool Runner<Input,State,CFtype>::AcceptableMove()
{
    return true;
}

/**
   Initializes all the runner variable for starting a new run.
*/
template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::InitializeRun()
{
#ifdef EASYLOCAL_PTHREADS
	StoppableObject::Starting();
#endif
    number_of_iterations = 0;
    iteration_of_best = 0;
    ComputeCost();
}

template <class Input, class State, typename CFtype>
bool Runner<Input,State,CFtype>::LowerBoundReached() const
{
	return sm.LowerBoundReached(current_state_cost);
}

#endif /*RUNNER_HH_*/

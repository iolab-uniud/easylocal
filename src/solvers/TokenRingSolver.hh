#ifndef TOKENRINGSOLVER_HH_
#define TOKENRINGSOLVER_HH_

#include "MultiRunnerSolver.hh"
#include "../helpers/StateManager.hh"
#include "../helpers/OutputManager.hh"
#ifdef EASYLOCAL_PTHREADS
#include <pthread.h>
#endif

/** The Token-ring Solver alternates n runners for a number of
    rounds.   
    @ingroup Solvers
*/
template <class Input, class Output, class State, typename CFtype = int>
class TokenRingSolver
            : public MultiRunnerSolver<Input,Output,State,CFtype>
{
public:
    TokenRingSolver(const Input& in, StateManager<Input, State,CFtype>& e_sm,
                    OutputManager<Input,Output,State,CFtype>& e_om,
                    std::string name = "Anonymous TokenRingSolver");
    void Print(std::ostream& os = std::cout) const;
    void SetRounds(unsigned int r);
    void SetStartRunner(unsigned int sr);
    void Check() const throw(EasyLocalException);
    void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
protected:
    // Run all runners circularly on the same thread
    void Run();
    unsigned int max_idle_rounds; /**< Maximum number of runs without improvement
        allowed. */
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
   Constructs a token-ring runner solver by providing it links to
   a state manager, an output manager, an input, and an output object.

   @param sm a pointer to a compatible state manager
   @param om a pointer to a compatible output manager
   @param in a pointer to an input object
   @param out a pointer to an output object
*/
template <class Input, class Output, class State, typename CFtype>
TokenRingSolver<Input,Output,State,CFtype>::TokenRingSolver(const Input& in,
        StateManager<Input,State,CFtype>& e_sm, OutputManager<Input,Output,State,CFtype>& e_om,
        std::string name)
        : MultiRunnerSolver<Input,Output,State,CFtype>(in, e_sm, e_om, name),
        max_idle_rounds(10)
{}

template <class Input, class Output, class State, typename CFtype>
void TokenRingSolver<Input,Output,State,CFtype>::ReadParameters(std::istream& is, std::ostream& os)
{
	os << "Token Ring Solver: " << this->GetName() << " parameters" << std::endl;
	os << "Runners: " << std::endl; 
	
	 for (unsigned int i = 0; i < this->runners.size(); i++)
    {
        os  << "Runner[" << i << "]" << std::endl;
        this->runners[i]->ReadParameters(is, os);
    }
    os << "Max idle rounds: ";
    is >> max_idle_rounds;
    os << "Timeout: ";
    is >> this->timeout;
}

template <class Input, class Output, class State, typename CFtype>
void TokenRingSolver<Input,Output,State,CFtype>::Print(std::ostream& os) const
{
    os  << "Token Ring Solver: " << this->GetName() << std::endl;

    for (unsigned int i = 0; i < this->runners.size(); i++)
    {
        os  << "Runner[" << i << "]" << std::endl;
        this->runners[i]->Print(os);
    }

}

/**
   Sets the number of rounds to the given value.
   
   @param r the number of rounds.
*/
template <class Input, class Output, class State, typename CFtype>
void TokenRingSolver<Input,Output,State,CFtype>::SetRounds(unsigned int r)
{ max_idle_rounds = r; }

/**
   Starts the token-ring from the i-th runner.
   
   @param i the runner which to start form
*/
template <class Input, class Output, class State, typename CFtype>
void TokenRingSolver<Input,Output,State,CFtype>::SetStartRunner(unsigned int i)
{ this->start_runner = i; }

/**
   Checks wether the object state is consistent with all the related
   objects.
*/
template <class Input, class Output, class State, typename CFtype>
void TokenRingSolver<Input,Output,State,CFtype>::Check() const
throw(EasyLocalException)
{
    LocalSearchSolver<Input,Output,State,CFtype>::Check();
    if (this->runners.size() == 0)
      throw EasyLocalException("Check(): runners not set in object " + this->GetName());
    for (unsigned int i = 0; i < this->runners.size(); i++)
      this->runners[i]->Check();
}

/**
   Outputs the solver state on a given output stream.

   @param os the output stream
*/
//  template <class Input, class Output, class State, typename CFtype>
//   void TokenRingSolver<Input,Output,State,CFtype>::Print(std::ostream& os) const
//   {
//     os << "Solver State" << std::endl;
//     for (unsigned int i = 0; i < this->runners.size(); i++)
//       {
//  os << "Runner " << i << std::endl;
//  this->runners[i]->Print(os);
//       }
//   }

/**
   Runs all the managed runners one after another till no improvement
   has produced in a given number of rounds
*/
template <class Input, class Output, class State, typename CFtype>
void TokenRingSolver<Input,Output,State,CFtype>::Run()
{
    // TODO: try to minimize the state copying operations
    // i is the current runner, j is the previous one;
    unsigned int i = this->start_runner, j = (this->start_runner >= 1) ?
                     (this->start_runner - 1) : this->runners.size() - 1;
    unsigned int idle_rounds = 0;
    bool interrupt_search = false;
    bool improvement_found = false;
#ifdef EASYLOCAL_PTHREADS
    pthread_t runner_thd_id;
#endif

    this->internal_state_cost = this->sm.CostFunction(this->internal_state);
    // this->internal_state_cost is used to check
    // whether a full round has produces improvements or not
    this->runners[i]->SetState(this->internal_state);

#ifdef EASYLOCAL_PTHREADS
	if (this->timeout > 0)
    	this->SetTimer();
#endif

    while (idle_rounds < max_idle_rounds && !interrupt_search && !this->Timeout())
    {
        do
        {
#ifndef EASYLOCAL_PTHREADS
            this->runners[i]->Go();
#else
            runner_thd_id = this->runners[i]->GoThread();
#ifdef DEBUG_PTHREADS
            std::cerr << "CREATED " << runner_thd_id << std::endl;
#endif
            double time_left = this->timeout / 10;
            while (!this->runners[i]->WaitTermination(time_left) && !this->Timeout())
#ifdef DEBUG_PTHREADS
                std::cerr << "CHECKING TERMINATION" << std::endl;
#else
				;
#endif

            pthread_join(runner_thd_id, NULL);
#endif
	    
	    if (LessThan(this->runners[i]->GetStateCost(),this->internal_state_cost))
            {
                this->internal_state = this->runners[i]->GetState();
                this->internal_state_cost = this->runners[i]->GetStateCost();
                improvement_found = true;
            }
#if VERBOSE >= 2
	    std::cerr << "Runner: " << i << ", Current cost: " << this->runners[i]->GetStateCost() << 
	      ", Best cost " << this->internal_state_cost << ", Idle rounds : " << idle_rounds << std::endl;
#endif
            if (this->runners[i]->LowerBoundReached() || this->runners.size() == 1)
            {
                interrupt_search = true;
                break;
            }
            j = i;
            i = (i + 1) % this->runners.size();
            this->runners[i]->SetState(this->runners[j]->GetState());
        }
        while (i != this->start_runner && !this->Timeout());

        if (!interrupt_search)
        {
            if (improvement_found)
                idle_rounds = 0;
            else
                idle_rounds++;
            improvement_found = false;
        }
    }
}

#endif /*TOKENRINGSOLVER_HH_*/

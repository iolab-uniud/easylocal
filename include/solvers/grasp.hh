#prgama once

#include <vector>

#include "easylocal/solvers/LocalSearch.hh"

namespace EasyLocal
{

namespace Core
{

/** An Iterated Local Search solver handles both a runner encapsulating a local
     search algorithm and a kicker used for perturbing current solution.
     @ingroup Solvers */
template <class Input, class Solution, class CostStructure = DefaultCostStructure<int>>
class GRASP
    : public LocalSearch<Input, Solution, CostStructure>
{
public:
  GRASP(const Input &i,
        SolutionManager<Input, Solution, CostStructure> &sm,
        OutputManager<Input, Solution, CostStructure> &om,
        std::string name);
  void Print(std::ostream &os = std::cout) const;
  void SetRunner(Runner<Input, Solution, CostStructure> &r);
  unsigned int GetRestarts() const { return restarts; }
  void ReadParameters(std::istream &is = std::cin, std::ostream &os = std::cout);

  void Solve(double alpha, unsigned int k, unsigned int restarts);

protected:
  unsigned int restarts;

  Runner<Input, Solution, CostStructure> *runner; /**< The linked runner. */
};

/*************************************************************************
     * Implementation
     *************************************************************************/

/**
     Constructs an iterated local search solver by providing it links to
     a state manager, an output manager, a runner, a kicker, an input,
     and an output object.
     
     @param sm a pointer to a compatible state manager
     @param om a pointer to a compatible output manager
     @param r a pointer to a compatible runner
     @param k a pointer to a compatible kicker
     @param in a pointer to an input object
     @param out a pointer to an output object
     */
template <class Input, class Solution, class CostStructure>
GRASP<Input, Solution, CostStructure>::GRASP(const Input &i,
                                                  SolutionManager<Input, Solution, CostStructure> &sm,
                                                  OutputManager<Input, Solution, CostStructure> &om,
                                                  std::string name)
    : LocalSearch<Input, Solution, CostStructure>(i, sm, om, name)
{
  runner = nullptr;
}

/**
     Set the runner.
     
     @param r a pointer to a compatible runner to add
     */
template <class Input, class Solution, class CostStructure>
void GRASP<Input, Solution, CostStructure>::SetRunner(Runner<Input, Solution, CostStructure> &r)
{
  runner = &r;
}

// template <class Input, class Solution, class CostStructure>
// void GRASP<Input, Solution, CostStructure>::Print(std::ostream& os) const
// {
//   os  << "Generalized Local Search Solver: " << this->name << std::endl;

//   if (this->runners.size() > 0)
//     for (unsigned int i = 0; i < this->runners.size(); i++)
//       {
// 	os  << "Runner[" << i << "]" << std::endl;
// 	this->runners[i]->Print(os);
//       }
//   else
//     os  << "<no runner attached>" << std::endl;
//   if (p_kicker)
//     p_kicker->Print(os);
//   else
//     os  << "<no kicker attached>" << std::endl;
//   os << "Max idle rounds: " << max_idle_rounds << std::endl;
//   os << "Timeout " << this->timeout << std::endl;
// }

/**
     Solves using a single runner
     */
template <class Input, class Solution, class CostStructure>
void GRASP<Input, Solution, CostStructure>::Solve(double alpha, unsigned int k, unsigned int trials)
{
  bool timeout_expired = false;
  unsigned t;
  if (runner == nullptr)
    throw std::logic_error("No runner set for solver " + this->name);
  //   chrono.Reset();
  //   chrono.Start();
  for (t = 0; t < trials; t++)
  {
    this->sm.GreedyState(this->current_state, alpha, k);
    runner->SetState(this->current_state);
    timeout_expired = LetGo(*runner);

    this->current_state = runner->GetState();
    this->current_state_cost = runner->GetStateCost();

    if (t == 0 || LessThan(this->current_state_cost, this->best_state_cost))
    {
      this->best_state = this->current_state;
      this->best_state_cost = this->current_state_cost;
      if (this->sm.LowerBoundReached(this->best_state_cost))
        break;
    }
    if (timeout_expired)
      break;
  }
  //   chrono.Stop();
}

template <class Input, class Solution, class CostStructure>
void GRASP<Input, Solution, CostStructure>::ReadParameters(std::istream &is, std::ostream &os)
{
  os << "GRASP Solver: " << this->name << " parameters" << std::endl;
  os << "Runner: " << std::endl;

  this->runner->ReadParameters(is, os);

#if defined(HAVE_PTHREAD)
  double timeout;
  os << "Timeout: ";
  is >> timeout;
  this->SetTimeout(timeout);
#endif
}

} // namespace Core
} // namespace EasyLocal

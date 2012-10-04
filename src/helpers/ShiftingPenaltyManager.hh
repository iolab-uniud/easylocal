#if !defined(_SHIFTING_PENALTY_MANAGER_HH_)
#define _SHIFTING_PENALTY_MANAGER_HH_

#include <helpers/DeltaCostComponent.hh>
#include <helpers/CostComponent.hh>
#include <utils/Random.hh>
#include <observers/ShiftingPenaltyObserver.hh>
#include <utils/Parameter.hh>

/** This class implements the shifting penalty mechanism for a single
 DeltaCost component */
template <typename CFtype>
class ShiftingPenaltyManager : public Parametrized
{
  friend class ShiftingPenaltyObserver<CFtype>;

public:
  
  ShiftingPenaltyManager(std::string n);
  
  ShiftingPenaltyManager(CFtype threshold, double s, std::string n);
  
  virtual ~ShiftingPenaltyManager() {}
  
  void AttachObserver(ShiftingPenaltyObserver<CFtype>& ob) { observer = &ob; }
  
  virtual void Print(std::ostream& os = std::cout) const;
  
  virtual void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
 
  virtual bool Reset() = 0;
  
  virtual bool Update(CFtype cost) = 0;
  
  void UpdatePerturbLevel();
  
protected:
  
  std::string name;
  
  // Parameters
  Parameter<double> min_shift;
  Parameter<double> max_shift;
  Parameter<double> cost_threshold;
  Parameter<double> perturb_level;
  Parameter<double> start_shift;

  // State
  double shift;
  double min_perturb;
  double max_perturb;
  
  ShiftingPenaltyObserver<CFtype>* observer;
};


template <typename CFtype>
class ComplexShiftingPenaltyManager : public ShiftingPenaltyManager<CFtype>
{
public:
  ComplexShiftingPenaltyManager(std::string n);
  bool Reset();
  bool Update(CFtype cost);
  
protected:
  
  // Parameters
  Parameter<unsigned int> max_feasible_iter;
  Parameter<unsigned int> max_infeasible_iter;
  
  // State
  unsigned int feasible_iter;
  unsigned int infeasible_iter;
};

template <typename CFtype>
class SimpleShiftingPenaltyManager : public ShiftingPenaltyManager<CFtype>
{
public:
  SimpleShiftingPenaltyManager(std::string n);
  bool Reset();
  bool Update(CFtype cost);
};

/*************************************************************************
 * Implementation
 *************************************************************************/

template <typename CFtype>
ShiftingPenaltyManager<CFtype>::ShiftingPenaltyManager(std::string n)
: name(n), observer(nullptr),
// Parameters
Parametrized("ShiftingPenaltyManager", "parameters for the shifting penalty manager"),
min_shift("min_shift", "Minium cost shift", this->parameters),
max_shift("max_shift", "Maximum cost shift", this->parameters),
cost_threshold("cost_threshold", "Cost threshold", this->parameters),
perturb_level("perturb_level", "Perturbation level", this->parameters),
start_shift("start_shift", "Starting shift", this->parameters)
{
  // Defaults
  min_shift = 0.0001;
  max_shift = 1.0;
  cost_threshold = (CFtype)0;
  min_perturb = 1.03;
  max_perturb = 1.05;
  start_shift = shift = 1.0;
}

template <typename CFtype>
void ShiftingPenaltyManager<CFtype>::ReadParameters(std::istream& is, std::ostream& os)
{
  Parametrized::ReadParameters();
  this->UpdatePerturbLevel();
}

template <typename CFtype>
void ShiftingPenaltyManager<CFtype>::UpdatePerturbLevel()
{
  // Calculate minimum and maximum perturbation from read perturbation level
  min_perturb = perturb_level - (perturb_level - 1) / 10.0;
  max_perturb = perturb_level + (perturb_level - 1) / 10.0;
}

template <typename CFtype>
ComplexShiftingPenaltyManager<CFtype>::ComplexShiftingPenaltyManager(std::string n)
: // Parameters
Parametrized("ComplexShiftingPenaltyManager", "parameters for the complex shifting penalty manager"),
ShiftingPenaltyManager<CFtype>(n),
max_feasible_iter("max_feasible_iter", "Maximum number of feasible iterations", this->parameters),
max_infeasible_iter("max_infeasible_iter", "Maximum number of infeasible iterations", this->parameters)
{
  max_feasible_iter = 10;
  max_feasible_iter = 10;
  feasible_iter = 0;
  infeasible_iter = 0;
}

template <typename CFtype>
SimpleShiftingPenaltyManager<CFtype>::SimpleShiftingPenaltyManager(std::string n)
: ShiftingPenaltyManager<CFtype>(n)
{ }

template <typename CFtype>
void ShiftingPenaltyManager<CFtype>::Print(std::ostream& os) const
{
  // Print arguments, then shift
  Parametrized::Print(os);
  os  << "  Current shift: " << shift << std::endl;
}

template <typename CFtype>
bool ComplexShiftingPenaltyManager<CFtype>::Reset()
{
  feasible_iter = 0;
  infeasible_iter = 0;
  if (this->shift != this->start_shift)
  {
    this->shift = this->start_shift;
    return true;
  }
  else
    return false;
}

template <typename CFtype>
bool ComplexShiftingPenaltyManager<CFtype>::Update(CFtype cost)
{
  bool update = false;
  float perturb = Random::Double(this->min_perturb, this->max_perturb);
  if (cost <= this->cost_threshold)
  {
    feasible_iter++;
    infeasible_iter = 0;
    if (feasible_iter == max_feasible_iter)
    {
      if (this->shift > this->min_shift)
      {
	      this->shift /= perturb;
	      update = true;
	      if (this->shift < this->min_shift)
        {
          this->shift = this->min_shift;
        }
      }
      feasible_iter = 0;
    }
    if (cost < this->cost_threshold)
    {
      this->cost_threshold = cost;
    }
  }
  else
  {
    infeasible_iter++;
    feasible_iter = 0;
    if (infeasible_iter == max_infeasible_iter)
    {
      if (this->shift < this->max_shift)
      {
	      this->shift *= perturb;
	      update = true;
	      if (this->shift > this->max_shift)
        {
          this->shift = this->max_shift;
        }
	    }
      infeasible_iter = 0;
    }
  }
  return update;
}

template <typename CFtype>
bool SimpleShiftingPenaltyManager<CFtype>::Reset()
{
  bool reset;
  if (this->shift != this->start_shift)
  {
    this->shift = this->start_shift;
    reset = true;
  }
  else
    reset = false;
  if (this->observer != nullptr)
    this->observer->NotifyReset(*this);
  
  return reset;
}


template <typename CFtype>
bool SimpleShiftingPenaltyManager<CFtype>::Update(CFtype cost)
{
  bool update = false;
  float perturb = Random::Double(this->min_perturb, this->max_perturb);
  if (cost <= this->cost_threshold)
  {
    if (this->shift > this->min_shift)
    {
      this->shift /= perturb;
      update = true;
      if (this->shift < this->min_shift)
        this->shift = this->min_shift;
    }
    if (cost < this->cost_threshold)
    {
      this->cost_threshold = cost;
      if (this->observer != nullptr)
        this->observer->NotifyNewThreshold(*this);
    }
  }
  else
  {
    if (this->shift < this->max_shift)
    {
      this->shift *= perturb;
      update = true;
      if (this->shift > this->max_shift)
        this->shift = this->max_shift;
    }
  }
  if (this->observer != nullptr)
    this->observer->NotifyUpdate(*this, cost);
  
  return update;
}

#endif // _SHIFTING_PENALTY_MANAGER_HH_
#if !defined(_WEIGHT_MANAGER_HH)
#define _WEIGHT_MANAGER_HH

#include "utils/Parameter.hh"

/** The Weight Manager deals with adaptive weighting schemes
 for the cost function that allow, e.g., to navigate more effectively 
 plateaux in the cost landscape.
 
 This class is at the top of the hierarchy: we have also a more
 specific weight manager, which implements the shifting penalty strategy.
 @ingroup Helpers
 */
template <class State, typename CFtype>
class WeightManager : public Parametrized
{
public:
  /** Given the @c cost_values passed as parameters, it returns the vector of modified cost according
      to the weighting strategy employed.
   */
  virtual std::vector<double> GetModifiedCost(const std::vector<CFtype>& cost_values) const = 0;
  /** Resets the weight manager. */
  virtual void Reset(const State& st) = 0;
  /** Updates the weights according to the weighting strategy. */
  virtual void Update(const std::vector<CFtype>& cost_values, const State& st) = 0;
  
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
protected:
  WeightManager(std::string name, std::string description);
  virtual ~WeightManager() {}
  const std::string name;
  const std::string description;
};

template <class State, typename CFtype>
WeightManager<State,CFtype>::WeightManager(std::string name, std::string description) : Parametrized(name, description), name(name), description(description)
{}

template <class State, typename CFtype>
void WeightManager<State,CFtype>::ReadParameters(std::istream& is, std::ostream& os)
{
  os << "  " << this->name << std::endl;
  Parametrized::ReadParameters(is, os);
}

#endif // _WEIGHT_MANAGER_HH

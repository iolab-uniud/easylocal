#if !defined(_SHIFTING_PENALTY_MANAGER)
#define _SHIFTING_PENALTY_MANAGER

#include "WeightManager.hh"

/** 
 The Shifting Penalty Manager modifies the weights of the hard cost components according
 to the shifting penalty strategy.
 @see Gendreau, M., Hertz, A. and Laporte, G. (1994). A tabu search heuristic for the vehicle routing problem. Management Science, 40 (10), 1276–1290.
 
 @ingroup Helpers
 */
template <class State, typename CFtype>
class ShiftingPenaltyManager : public WeightManager<State, CFtype>
{
public:
  /** Given the @c cost_values passed as parameters, it returns the aggregated modified cost according
      to the weighting strategy employed.
   */
  virtual double GetModifiedCost(const std::vector<CFtype>& cost_values) const = 0;
  /** Resets the prohibition manager mechanisms. */
  virtual void Reset() = 0;
  virtual void Update() = 0;
  
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
protected:
  ShiftingPenaltyManager(std::string name, std::string description);
  virtual ~ShiftingPenaltyManager() {}
  const std::string name;
  const std::string description;
};

template <class State, typename CFtype>
ShiftingPenaltyManager<State,CFtype>::ShiftingPenaltyManager(std::string name, std::string description) : WeightManager<State, CFtype>(name, description)
{}

template <class State, typename CFtype>
void ShiftingPenaltyManager<State,CFtype>::ReadParameters(std::istream& is, std::ostream& os)
{
  os << "  " << this->name << std::endl;
  Parametrized::ReadParameters(is, os);
}

#endif // _SHIFTING_PENALTY_MANAGER

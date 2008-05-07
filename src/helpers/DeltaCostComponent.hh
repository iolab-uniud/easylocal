#ifndef _DELTA_COST_COMPONENT_HH_
#define _DELTA_COST_COMPONENT_HH_

#include <helpers/CostComponent.hh>
#include <helpers/ShiftingPenaltyManager.hh>
#include <stdexcept>

/** The class DeltaCostComponent manages the variation of one single
    component of the cost, either hard or soft 
    @ingroup Helpers      
*/

// // forward class tag declaration
// template <class Input, class State, class Move, typename CFtype>
// class NeighborhoodExplorer;

/**
   TODO: Give a description of deltas
*/

template <typename CFtype>
class ShiftedResult
{
public:
  ShiftedResult() : actual_value(0), shifted_value(0.0) 
  {}
  CFtype actual_value;
  double shifted_value;
};

template <typename CFtype>
ShiftedResult<CFtype> operator+(const ShiftedResult<CFtype>& sr1, const ShiftedResult<CFtype>& sr2);

template <typename CFtype>
ShiftedResult<CFtype> operator-(const ShiftedResult<CFtype>& sr1, const ShiftedResult<CFtype>& sr2);

template <typename CFtype, typename Multype>
ShiftedResult<CFtype> operator*(const Multype& mul, const ShiftedResult<CFtype>& sr);

template <typename CFtype, typename Multype>
ShiftedResult<CFtype> operator*(const ShiftedResult<CFtype>& sr, const Multype& mul);

/** A class which manages the variations of a single component of the
    cost function
    @ingroup Helpers
*/

template <class Input, class State, class Move, typename CFtype = int>
class AbstractDeltaCostComponent
{
public:
  virtual bool IsDeltaImplemented() const = 0;
  virtual bool IsHard() const = 0;
  virtual bool IsSoft() const = 0;
  virtual void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout) = 0;
  virtual void ResetShift() = 0;
  virtual void UpdateShift(const State& st) = 0;  
  const std::string name;
protected:
  AbstractDeltaCostComponent(std::string e_name) : name(e_name) {}
  virtual ~AbstractDeltaCostComponent() {}
};

template <class Input, class State, class Move, bool is_delta_implemented, typename CFtype = int>
class DeltaCostComponent
  : public AbstractDeltaCostComponent<Input,State,Move,CFtype>
{
public:
  void Print(std::ostream& os = std::cout) const;
  DeltaCostComponent(const Input& in, CostComponent<Input,State,CFtype>& cc, std::string name, ShiftingPenaltyManager<CFtype>* spm  = NULL);
  void SetShiftingPenaltyManager(ShiftingPenaltyManager<CFtype>* spm);
  virtual void ResetShift();
  virtual bool IsDeltaImplemented() const  { return is_delta_implemented; }
  CostComponent<Input,State,CFtype>& GetCostComponent() const { return cc; }
  virtual void UpdateShift(const State& st);
  virtual void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
  bool IsHard() const
  { return cc.IsHard(); }
 bool IsSoft() const
  { return cc.IsSoft(); }
protected:
  virtual CFtype ComputeDeltaCost(const State& st, const Move& mv) const = 0;
  const Input& in;
  CostComponent<Input,State,CFtype>& cc;
  ShiftingPenaltyManager<CFtype>* spm;
  bool is_shifted;
};

template <class Input, class State, class Move, typename CFtype = int>
class FilledDeltaCostComponent
: public DeltaCostComponent<Input,State,Move,true,CFtype>
{
public:
  virtual CFtype DeltaCost(const State& st, const Move& mv) const;
  virtual ShiftedResult<CFtype> DeltaShiftedCost(const State& st, const Move& mv) const;
protected:
  FilledDeltaCostComponent(const Input& in, CostComponent<Input,State,CFtype>& cc, std::string name, ShiftingPenaltyManager<CFtype>* spm  = NULL)
    : DeltaCostComponent<Input,State,Move,true,CFtype>(in,cc,name,spm) {}
};

template <class Input, class State, class Move, typename CFtype = int>
class EmptyDeltaCostComponent
: public DeltaCostComponent<Input,State,Move,false,CFtype>
{
public:
  EmptyDeltaCostComponent(const Input& in, CostComponent<Input,State,CFtype>& cc,  std::string name = "", ShiftingPenaltyManager<CFtype>* spm  = NULL)
  : DeltaCostComponent<Input,State,Move,false,CFtype>(in,cc,name,spm)   {}
  virtual CFtype DeltaCost(const State& st, const State& st1) const; 
  virtual ShiftedResult<CFtype> DeltaShiftedCost(const State& st, const State& st1) const;
  CFtype ComputeDeltaCost(const State& st, const Move& mv) const 
  { throw std::logic_error("This method should never be called because the implementation is missing"); }
};


/*************************************************************************
 * Implementation
 *************************************************************************/

template <class Input, class State, class Move, bool is_delta_implemented, typename CFtype>
DeltaCostComponent<Input,State,Move,is_delta_implemented,CFtype>::DeltaCostComponent(const Input& i,
							 CostComponent<Input,State,CFtype>& e_cc, std::string name, ShiftingPenaltyManager<CFtype>* a_spm)
  : AbstractDeltaCostComponent<Input,State,Move,CFtype>(name), in(i), cc(e_cc), spm(a_spm)
{
	if (spm != NULL)
    is_shifted = true;
  else
    is_shifted = false;
}

template <class Input, class State, class Move, bool is_delta_implemented, typename CFtype>
void DeltaCostComponent<Input,State,Move,is_delta_implemented,CFtype>::
       SetShiftingPenaltyManager(ShiftingPenaltyManager<CFtype>* a_spm)
{
  spm = a_spm;
  if (spm != NULL)
    is_shifted = true;
  else
    is_shifted = false;
}




template <class Input, class State, class Move, bool is_delta_implemented, typename CFtype>
void DeltaCostComponent<Input,State,Move,is_delta_implemented,CFtype>::Print(std::ostream& os) const
{
  os << "  DeltaCost Component: " + this->GetName() << std::endl;
}

template <class Input, class State, class Move, typename CFtype>
CFtype FilledDeltaCostComponent<Input,State,Move,CFtype>::DeltaCost(const State& st, const Move& mv) const
{
    return this->cc.Weight() * ComputeDeltaCost(st, mv);
}  

template <class Input, class State, class Move, typename CFtype>
ShiftedResult<CFtype> FilledDeltaCostComponent<Input,State,Move,CFtype>::DeltaShiftedCost(const State& st,
						       const Move& mv) const
{
  ShiftedResult<CFtype> res;
  res.actual_value = this->cc.Weight() * ComputeDeltaCost(st, mv);
  if (this->is_shifted)
    res.shifted_value = res.actual_value * this->spm->Shift();
  else
    res.shifted_value = (double)res.actual_value;

  return res;
}  

/**
   tentative definition, to be replaced with the actual (much more
   efficient) implementation
*/

template <class Input, class State, class Move, typename CFtype>
CFtype EmptyDeltaCostComponent<Input,State,Move,CFtype>::DeltaCost(const State& st,
						       const State& st1) const
{ 
  return this->cc.Weight() * (this->cc.ComputeCost(st1) - this->cc.ComputeCost(st));
}

template <class Input, class State, class Move, typename CFtype>
ShiftedResult<CFtype> EmptyDeltaCostComponent<Input,State,Move,CFtype>::DeltaShiftedCost(const State& st,
						       const State& st1) const
{
  ShiftedResult<CFtype> res;
  res.actual_value = this->cc.Weight() * (this->cc.ComputeCost(st1) - this->cc.ComputeCost(st));
  if (this->is_shifted)
    res.shifted_value =  res.actual_value * this->spm->Shift();
  else
    res.shifted_value =  (double)res.actual_value;

  return res;
}

template <class Input, class State, class Move, bool is_delta_implemented, typename CFtype>
void DeltaCostComponent<Input,State,Move,is_delta_implemented,CFtype>::ResetShift()
{
  if (is_shifted)
    {
      bool reset;
      reset = spm->Reset();
    }
}

template <class Input, class State, class Move, bool is_delta_implemented, typename CFtype>
void DeltaCostComponent<Input,State,Move,is_delta_implemented,CFtype>::UpdateShift(const State& st)
{
  if (is_shifted)
    {
      bool update;
      update = spm->Update(cc.Cost(st));
    }
}   

template <class Input, class State, class Move, bool is_delta_implemented, typename CFtype>
void DeltaCostComponent<Input,State,Move,is_delta_implemented,CFtype>::ReadParameters(std::istream& is, std::ostream& os)
{
  if (is_shifted)
    spm->ReadParameters(is,os);
}


template <typename CFtype>
ShiftedResult<CFtype> operator+(const ShiftedResult<CFtype>& sr1, const ShiftedResult<CFtype>& sr2)
{
  ShiftedResult<CFtype> res = sr1;
  res.actual_value += sr2.actual_value;
  res.shifted_value += sr2.shifted_value;

  return res;
}

template <typename CFtype>
ShiftedResult<CFtype> operator-(const ShiftedResult<CFtype>& sr1, const ShiftedResult<CFtype>& sr2)
{
  ShiftedResult<CFtype> res = sr1;
  res.actual_value -= sr2.actual_value;
  res.shifted_value -= sr2.shifted_value;

  return res; 
}

template <typename CFtype, typename Multype>
ShiftedResult<CFtype> operator*(const Multype& mul, const ShiftedResult<CFtype>& sr)
{
  ShiftedResult<CFtype> res = sr;
  res.actual_value *= mul;
  res.shifted_value *= mul;

  return res;
}

template <typename CFtype, typename Multype>
ShiftedResult<CFtype> operator*(const ShiftedResult<CFtype>& sr, const Multype& mul)
{  
  ShiftedResult<CFtype> res = sr;
  res.actual_value *= mul;
  res.shifted_value *= mul;

  return res;
}


#endif // define _DELTACOSTCOMPONENT_HH_

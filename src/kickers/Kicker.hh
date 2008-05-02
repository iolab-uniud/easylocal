#ifndef KICKER_HH_
#define KICKER_HH_

#include <exception>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

enum KickTypes {
    RANDOM_KICK = 0,
    BEST_KICK,
    TOTAL_BEST_KICK,
    FIRST_IMPROVING_KICK,
    TOTAL_FIRST_IMPROVING_KICK
};

enum KickerTypes {
    SINGLE = 0,
    BIMODAL
};

/** The Kicker class is an interface for the actual Kickers.
    Kickers select a new state by trying to apply a sequence of
    moves.  
    @ingroup Perturbers */
template <class Input, class State, typename CFtype = int>
class Kicker
{
public:
  Kicker(const Input& i, unsigned step, std::string name);
  virtual ~Kicker() {}
  void SetKickType(const KickTypes& kt);
  virtual CFtype SelectKick(const State& st) = 0;
  virtual void MakeKick(State &st) = 0;
  virtual CFtype KickCost() = 0;

  virtual CFtype BestKick(const State& st) = 0;
  virtual CFtype FirstImprovingKick(const State &st) = 0;
  virtual CFtype TotalFirstImprovingKick(const State &st) = 0;
			  //  virtual CFtype DenseBestKick(const State &st) = 0;
  virtual CFtype TotalBestKick(const State &st) = 0;
  virtual CFtype RandomKick(const State &st) = 0;
  virtual void FirstKick(const State &st) = 0;
  virtual bool NextKick() = 0;

  bool SingleKicker() const { return kicker_type == SINGLE; }
  void PrintStatistics(std::ostream& os = std::cout) const;

  virtual void Print(std::ostream& os = std::cout) const = 0;
  virtual void PrintCurrentMoves(unsigned i, std::ostream& os) const = 0;
  virtual void PrintPattern(std::ostream& os = std::cout) {}
  virtual void SetStep(unsigned int s);
  unsigned int Step() const { return step; }
  virtual void ReadParameters(std::istream& is = std::cin,
                              std::ostream& os = std::cout);
  virtual void PrintKick(std::ostream& os = std::cout) const = 0;
  const std::string name;
protected:
  const Input& in;
  std::vector<State> states; // for overcoming the need of () constructor
  unsigned int max_step, step;
  KickTypes current_kick_type;
  KickerTypes kicker_type;
};

/*************************************************************************
 * Implementation
 *************************************************************************/

template <class Input, class State, typename CFtype>
Kicker<Input,State,CFtype>::Kicker(const Input& i, unsigned int s, std::string e_name)
  : name(e_name), in(i), states(0, State(in)), step(s), current_kick_type(BEST_KICK), kicker_type(SINGLE)
{
  states.resize(s + 1, State(in));
}


template <class Input, class State, typename CFtype>
void Kicker<Input,State,CFtype>::SetStep(unsigned int s)
{
  step = s;
  if (step > states.size()+1)
    states.resize(s + 1, State(in));
}

template <class Input, class State, typename CFtype>
void Kicker<Input,State,CFtype>::SetKickType(const KickTypes& kt)
{ current_kick_type = kt; }

template <class Input, class State, typename CFtype>
void Kicker<Input,State,CFtype>::ReadParameters(std::istream& is, std::ostream& os)

{
    unsigned s;
    os << "KICKER -- INPUT PARAMETERS" << std::endl;
    os << "  Step: ";
    is >> s;
    SetStep(s);
}


#endif /*KICKER_HH_*/

#ifndef KICKER_HH_
#define KICKER_HH_

#include <exception>

#ifdef _HAVE_EASYLOCALCONFIG
#include <EasyLocalConfig.hh>
#endif

enum KickTypes {
    RANDOM_KICK = 0,
    BEST_KICK,
    TOTAL_BEST_KICK,
    FIRST_IMPROVING_KICK,
    TOTAL_FIRST_IMPROVING_KICK
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

  virtual CFtype BestKick(const State &st) = 0;
  virtual CFtype FirstImprovingKick(const State &st) = 0;
  virtual CFtype TotalFirstImprovingKick(const State &st) = 0;
  virtual CFtype DenseBestKick(const State &st) = 0;
  virtual CFtype TotalBestKick(const State &st) = 0;
  virtual CFtype RandomKick(const State &st) = 0;
  virtual void FirstKick(const State &st) = 0;
  virtual bool NextKick() = 0;
virtual void PrintCurrentMoves(unsigned i, std::ostream& os) const = 0;



  virtual bool SingleKicker() = 0;

  void PrintStatistics(std::ostream& os = std::cout) const;

  virtual void SetMaxStep(unsigned int s);
  unsigned int MaxStep() const { return max_step; }
  virtual void PrintPattern(std::ostream& os = std::cout) {}
  virtual void SetStep(unsigned int s);
  unsigned int Step() const { return step; }
  virtual void ReadParameters(std::istream& is = std::cin,
                              std::ostream& os = std::cout);
  virtual void PrintKick(std::ostream& os = std::cout) const = 0;
  const std::string name;
protected:
//   virtual CFtype ComputeKickCost(const State& st) = 0;
  const Input& in;
  std::vector<State> states; // for overcoming the need of () constructor
  unsigned int max_step, step;
  KickTypes current_kick_type;
};

/*************************************************************************
 * Implementation
 *************************************************************************/

template <class Input, class State, typename CFtype>
Kicker<Input,State,CFtype>::Kicker(const Input& i, unsigned int s, std::string e_name)
  : name(e_name), in(i), states(0, State(in)), max_step(s), step(s), current_kick_type(BEST_KICK)
{
  states.resize(s + 1, State(in));
}

template <class Input, class State, typename CFtype>
void Kicker<Input,State,CFtype>::SetMaxStep(unsigned int s)
{
    max_step = s;
    states.resize(s + 1, State(in));
}

template <class Input, class State, typename CFtype>
void Kicker<Input,State,CFtype>::SetStep(unsigned int s)
{
	step = s;
  if (s > max_step)
		SetMaxStep(s);
}

template <class Input, class State, typename CFtype>
void Kicker<Input,State,CFtype>::SetKickType(const KickTypes& kt)
{ current_kick_type = kt; }

// template <class Input, class State, typename CFtype>
// void Kicker<Input,State,CFtype>::SelectKick(const State& st)
// {
//     switch (current_kick_type)
//     {
//     case RANDOM_KICK:
//         RandomKick(st);
//         break;
//     case BEST_KICK:
//         BestKick(st);
//         break;
//     case TOTAL_BEST_KICK:
//         TotalBestKick(st);
//         break;
//     case FIRST_IMPROVING_KICK:
//         FirstImprovingKick(st);
//         break;
//     case TOTAL_FIRST_IMPROVING_KICK:
//         TotalFirstImprovingKick(st);
//         break;
//     }
// }


// template <class Input, class State, typename CFtype>
// CFtype Kicker<Input,State,CFtype>::MakeBestKick(State &st)
// {
//     BestKick(st);
//     return MakeKick(st);
// }

// template <class Input, class State, typename CFtype>
// CFtype Kicker<Input,State,CFtype>::MakeFirstImprovingKick(State &st)
// {
//     FirstImprovingKick(st);
//     return MakeKick(st);
// }

// template <class Input, class State, typename CFtype>
// CFtype Kicker<Input,State,CFtype>::MakeTotalFirstImprovingKick(State &st)
// {
//     TotalFirstImprovingKick(st);
//     return MakeKick(st);
// }

// template <class Input, class State, typename CFtype>
// CFtype Kicker<Input,State,CFtype>::MakeDenseBestKick(State &st)
// {
//     DenseBestKick(st);
//     return MakeKick(st);
// }

// template <class Input, class State, typename CFtype>
// CFtype Kicker<Input,State,CFtype>::MakeTotalBestKick(State &st)
// {
//     TotalBestKick(st);
//     return MakeKick(st);
// }

// template <class Input, class State, typename CFtype>
// CFtype Kicker<Input,State,CFtype>::MakeRandomKick(State &st)
// {
//     RandomKick(st);
//     return MakeKick(st);
// }

template <class Input, class State, typename CFtype>
void Kicker<Input,State,CFtype>::PrintStatistics(std::ostream &os) const
{ }


template <class Input, class State, typename CFtype>
void Kicker<Input,State,CFtype>::ReadParameters(std::istream& is, std::ostream& os)

{
    unsigned s;
    os << "KICKER -- INPUT PARAMETERS" << std::endl;
    os << "  Max Step: ";
    is >> s;
    SetMaxStep(s);
}


#endif /*KICKER_HH_*/

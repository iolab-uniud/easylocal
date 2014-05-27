#if !defined(_TYPES_HH_)
#define _TYPES_HH_

#include <EasyLocal.conf.hh>
#include <vector>
#include <iostream>

template <typename CFtype>
bool IsZero(CFtype value);

template <typename CFtype>
bool EqualTo(CFtype value1, CFtype value2);

template <typename CFtype>
bool LessThan(CFtype value1, CFtype value2);

template <typename CFtype>
bool LessOrEqualThan(CFtype value1, CFtype value2);

template <typename CFtype>
bool GreaterThan(CFtype value1, CFtype value2);

template <typename CFtype>
bool GreaterOrEqualThan(CFtype value1, CFtype value2);

template <typename CFtype>
CFtype max(const std::vector<CFtype>& values)
{
 CFtype max_val = values[0];
 for (unsigned int i = 1; i < values.size(); i++)
 if (values[i] > max_val)
 max_val = values[i];
 
 return max_val;
}
 
template <typename CFtype>
CFtype min(const std::vector<CFtype>& values)
{
 CFtype min_val = values[0];
 for (unsigned int i = 1; i < values.size(); i++)
 if (values[i] < min_val)
 min_val = values[i];
 
 return min_val;
}

/** Template class to incapsulate a boolean flag that marks a Move active or inactive in a multi-modal context. */
template <class Move>
class ActiveMove : public Move
{
public:
  bool active;
  Move& RawMove() { return *this; }
};

/** Input operator for ActiveMove, calls input operator for Move. */
template <typename Move>
std::istream& operator>>(std::istream& is, ActiveMove<Move>& m)
{
  is >> static_cast<Move&>(m);
  return is;
}

/** Output operator for ActiveMove, calls output operator for Move. */
template <typename Move>
std::ostream& operator<<(std::ostream& os, const ActiveMove<Move>& m)
{
  if (m.active)
    os << static_cast<const Move&>(m);
  return os;
}

template <class M1, class M2>
struct MoveRelations
{
  static bool RelatedMove(const M1&, const M2&)
  {
    return true;
  }
};

template <class Move>
bool operator==(const ActiveMove<Move>& mv1, const ActiveMove<Move>& mv2)
{
		std::cerr << "operator== " << mv1 << ' ' << mv2 << std::endl;
	if (!mv1.active && !mv2.active)
		return true;
	else if (mv1.active != mv2.active)
		return false;
	else
		return static_cast<Move>(mv1) == static_cast<Move>(mv2);
}

template <class Move>
bool operator<(const ActiveMove<Move>& mv1, const ActiveMove<Move>& mv2)
{
	std::cerr << "operator< " << mv1 << ' ' << mv2 << std::endl;
	if (!mv1.active && !mv2.active)
		return false;
	else if (mv1.active < mv2.active)
		return true;
	else if (mv1.active > mv2.active)
		return false;
	else
		return static_cast<Move>(mv1) < static_cast<Move>(mv2);
}

#endif // _TYPES_HH_

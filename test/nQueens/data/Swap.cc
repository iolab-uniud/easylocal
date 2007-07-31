#include "Swap.hh"

Swap::Swap(int f, int t)
{
    from = f;
    to = t;
}

bool Swap::operator==(const Swap& m) const
{
    return (from == m.from) && (to == m.to);
}

bool Swap::operator!=(const Swap& m) const
{
    return (from != m.from) || (to != m.to);
}

bool Swap::operator<(const Swap& m) const
{
    return from < m.from || (from == m.from && to < m.to);
}

std::ostream& operator<<(std::ostream& os, const Swap& m)
{
    return os << '(' << m.from << ',' << m.to << ')';
}

std::istream& operator>>(std::istream& is, Swap& m)
{
    return is >>  m.from >> m.to;
}

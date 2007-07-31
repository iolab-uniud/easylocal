#ifndef SWAP_H_
#define SWAP_H_

#include <iostream>

/** This class instantiates template Move. It represents a swap of the
row position between two queens.
@ingroup basic */
class Swap
{
public:
    Swap(int f = 0, int t = 0);
    bool operator==(const Swap&) const;
    bool operator!=(const Swap&) const;
    bool operator<(const Swap&) const;
    int from, to;
};

std::istream& operator>>(std::istream& is, Swap& m);
std::ostream& operator<<(std::ostream& os, const Swap& m);

#endif /*SWAP_H_*/

#include "QueensNeighborhoodExplorer.hh"
#include <algorithm>
#include <utils/Random.hh>

// the first parameter is not used, therefore it is not named
// (CC gives a warning if it is named)
void QueensNeighborhoodExplorer::RandomMove(const std::vector<unsigned> &, Swap& sw)
{
    sw.from = Random::Int(0, in - 1);
    do
        sw.to = Random::Int(0, in - 1);
    while (sw.from == sw.to);
    if (sw.from > sw.to) // swap from and to so that from < to
    { unsigned int tmp = sw.from;
        sw.from = sw.to;
        sw.to = tmp;
    }
}

void QueensNeighborhoodExplorer::NextMove(const std::vector<unsigned> &, Swap& sw)
{
    if (sw.to < in - 1) sw.to++;
    else if (sw.from < in - 2)
    { sw.from++; sw.to = sw.from + 1; }
    else
    { sw.from = 0; sw.to = 1; }
}

void QueensNeighborhoodExplorer::MakeMove(std::vector<unsigned> &a, const Swap& sw)
{ 
	std::swap(a[sw.from], a[sw.to]);
	/* int temp = a(sw.from);
    a(sw.from) = a(sw.to);
    a(sw.to) = temp; */
}

bool QueensNeighborhoodExplorer::FeasibleMove(const std::vector<unsigned>&, const Swap&)
{ return true; }

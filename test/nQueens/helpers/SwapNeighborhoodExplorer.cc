#include "SwapNeighborhoodExplorer.hh"
#include <algorithm>
#include <utils/Random.hh>

// the first parameter is not used, therefore it is not named
// (CC gives a warning if it is named)
void SwapNeighborhoodExplorer::RandomMove(const std::vector<int> &, Swap& sw)
{
    sw.from = Random::Int(0, in - 1);
    do
        sw.to = Random::Int(0, in - 1);
    while (sw.from == sw.to);
    if (sw.from > sw.to) // swap from and to so that from < to
    { int tmp = sw.from;
        sw.from = sw.to;
        sw.to = tmp;
    }
}

bool SwapNeighborhoodExplorer::NextMove(const std::vector<int> &, Swap& sw)
{
    if (sw.to < in - 1) 
      {
	sw.to++;
	return true;
      }
    else if (sw.from < in - 2)
      { 
	sw.from++; 
	sw.to = sw.from + 1; 
	return true;
      }
    else
      return false;
}


bool SwapNeighborhoodExplorer::FirstMove(const std::vector<int> &, Swap& sw)
{
  sw.from = 0; 
  sw.to = 1; 
  return true;
}



void SwapNeighborhoodExplorer::MakeMove(std::vector<int> &a, const Swap& sw)
{ 
	std::swap(a[sw.from], a[sw.to]);
	/* int temp = a(sw.from);
    a(sw.from) = a(sw.to);
    a(sw.to) = temp; */
}

bool SwapNeighborhoodExplorer::FeasibleMove(const std::vector<int>&, const Swap&)
{ return true; }

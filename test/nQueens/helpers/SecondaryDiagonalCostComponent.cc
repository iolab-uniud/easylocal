#include "SecondaryDiagonalCostComponent.hh"

int SecondaryDiagonalCostComponent::ComputeCost(const std::vector<unsigned>& a) const
{
  int violations = 0;
  for (unsigned int i = 0; i < in - 1; i++)
    for (unsigned int j = i + 1; j < in; j++)
      if (j - i == a[i] - a[j])
	violations++;
  return violations;
}

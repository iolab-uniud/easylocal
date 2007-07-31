#include "PrimaryDiagonalCostComponent.hh"

int PrimaryDiagonalCostComponent::ComputeCost(const std::vector<unsigned>& a) const
{
  int violations = 0;
  for (int i = 0; i < in - 1; i++)
    for (int j = i + 1; j < in; j++)
      if (j - i == a[j] - a[i])
	violations++;
  return violations;
}


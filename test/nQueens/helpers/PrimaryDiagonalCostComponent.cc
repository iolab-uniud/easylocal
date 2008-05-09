#include "PrimaryDiagonalCostComponent.hh"

bool PrimaryDiagonalCostComponent::Violation(int i, int j, int ai, int aj) 
{
	if (i != j)
		return (j - i == aj - ai);
	else
		return false;
}

int PrimaryDiagonalCostComponent::ComputeCost(const std::vector<int>& a) const
{
  int violations = 0;
  for (int i = 0; i < in - 1; i++)
    for (int j = i + 1; j < in; j++)
      if (Violation(i, j, a[i], a[j]))
				violations++;
  return violations;
}

void PrimaryDiagonalCostComponent::PrintViolations(const std::vector<int>& a, std::ostream& os) const
{
  for (int i = 0; i < in - 1; i++)
    for (int j = i + 1; j < in; j++)
      if (Violation(i, j, a[i], a[j]))
				os << "Queens " << i << " and " << j << " are in primary diagonal conflict" << std::endl;	
}


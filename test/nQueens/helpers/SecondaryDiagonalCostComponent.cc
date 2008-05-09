#include "SecondaryDiagonalCostComponent.hh"

bool SecondaryDiagonalCostComponent::Violation(int i, int j, int ai, int aj) 
{
	if (i != j)
		return (j - i == ai - aj);
	else
		return false;
}

int SecondaryDiagonalCostComponent::ComputeCost(const std::vector<int>& a) const
{
  int violations = 0;
  for (int i = 0; i < in - 1; i++)
    for (int j = i + 1; j < in; j++)
			if (Violation(i, j, a[i], a[j]))
					violations++;
  return violations;
}

void SecondaryDiagonalCostComponent::PrintViolations(const std::vector<int>& a, std::ostream& os) const
{
  for (int i = 0; i < in - 1; i++)
    for (int j = i + 1; j < in; j++)
      if (Violation(i, j, a[i], a[j]))
				os << "Queens " << i << " and " << j << " are in secondary diagonal conflict" << std::endl;	
}

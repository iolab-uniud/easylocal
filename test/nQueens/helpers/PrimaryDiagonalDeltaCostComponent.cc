#include "PrimaryDiagonalDeltaCostComponent.hh"

int PrimaryDiagonalDeltaCostComponent::ComputeDeltaCost(const std::vector<int>& a, const Swap& sw) const {
  int violations = 0;
  for (int i = 0; i < in; i++)
	{
		if (i == sw.from || i == sw.to)
			continue;
		if (PrimaryDiagonalCostComponent::Violation(i, sw.from, a[i], a[sw.from]))
			violations--;
		if (PrimaryDiagonalCostComponent::Violation(i, sw.to, a[i], a[sw.to]))
			violations--;
		if (PrimaryDiagonalCostComponent::Violation(i, sw.from, a[i], a[sw.to]))
			violations++;
		if (PrimaryDiagonalCostComponent::Violation(i, sw.to, a[i], a[sw.from]))
			violations++;		
	}
	if (PrimaryDiagonalCostComponent::Violation(sw.from, sw.to, a[sw.from], a[sw.to]))
		violations--;
	if (PrimaryDiagonalCostComponent::Violation(sw.from, sw.to, a[sw.to], a[sw.from]))
		violations++;
  return violations;
}

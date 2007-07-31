#include "PrimaryDiagonalDeltaCostComponent.hh"

int PrimaryDiagonalDeltaCostComponent::ComputeDeltaCost(const std::vector<unsigned>& a, const Swap& sw) const {
  int violations = 0;
  for (int i = 0; i < in; i++)
    {
      if (i == sw.from || i == sw.to)
	continue;
      if (i - sw.from == a[sw.from] - a[i])
	violations--;
      if (i - sw.to == a[sw.to] - a[i])
	violations--;
      if (i - sw.to == a[sw.from] - a[i])
	violations++;
      if (i - sw.from == a[sw.to] - a[i])
	violations++;
    }
  return violations;
}

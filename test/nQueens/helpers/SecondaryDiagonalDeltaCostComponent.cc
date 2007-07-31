#include "SecondaryDiagonalDeltaCostComponent.hh"

int SecondaryDiagonalDeltaCostComponent::ComputeDeltaCost(const std::vector<unsigned>& a, const Swap& sw) const {
  int violations = 0;
  for (int i = 0; i < in; i++)
    {
      if (i == sw.from || i == sw.to) continue;
      if (i - sw.from == a[i] - a[sw.from])
	violations--;
      if (i - sw.to == a[i] - a[sw.to])
	violations--;
      if (i - sw.to == a[i] - a[sw.from])
	violations++;
      if (i - sw.from == a[i] - a[sw.to])
	violations++;
    }
  return violations;
}

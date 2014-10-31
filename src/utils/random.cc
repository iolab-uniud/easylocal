#include "easylocal/utils/random.hh"

using namespace EasyLocal;
using namespace Core;

// Initialize pseudo-random generation machinery
std::random_device Random::dev;
std::mt19937 Random::g(Random::dev());

// By default the recorded seed is -1 (no seed provided)

int Random::seed = Random::Seed(Random::Int());

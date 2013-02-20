//
//  Random.cc
//  EasyLocal
//
//  Created by Tommaso Urli on 9/24/12.
//  Copyright (c) 2012 University of Udine. All rights reserved.
//

#include "Random.hh"

// Initialize pseudo-random generation machinery
std::random_device Random::dev;
std::mt19937 Random::g(Random::dev());

// By default the recorded seed is -1 (no seed provided)
int Random::seed = -1; 

// Each solver must include: Random::Seed(Random::Int());

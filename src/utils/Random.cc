//
//  Random.cc
//  EasyLocal
//
//  Created by Tommaso Urli on 9/24/12.
//  Copyright (c) 2012 University of Udine. All rights reserved.
//

#include "Random.hh"

std::random_device Random::dev;
std::mt19937 Random::g(Random::dev());

int Random::seed;

// Initialize random seed from default random sequence, so that we can save it
std::uniform_int_distribution<> t;
Random::Seed(t(g));


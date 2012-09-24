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


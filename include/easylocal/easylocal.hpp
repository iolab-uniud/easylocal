#pragma once

// Dependency-free EasyLocal Core umbrella. Optional adapters such as
// ConfigTOML and TextUI deliberately remain opt-in headers/components.
#include <easylocal/aggregation.hpp>
#include <easylocal/app.hpp>
#include <easylocal/check.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/cursor_moves.hpp>
#include <easylocal/neighborhood_concepts.hpp>
#include <easylocal/neighborhood_union.hpp>
#include <easylocal/run_control.hpp>
#include <easylocal/runner.hpp>
#include <easylocal/runner_tag.hpp>
#include <easylocal/service_base.hpp>
#include <easylocal/solver.hpp>
#include <easylocal/tester.hpp>

#include <easylocal/config/cli.hpp>
#include <easylocal/config/file.hpp>
#include <easylocal/config/overrides.hpp>
#include <easylocal/config/parameters.hpp>
#include <easylocal/config/setup.hpp>
#include <easylocal/config/tree.hpp>
#include <easylocal/config/validation.hpp>

#include <easylocal/search/best_improvement.hpp>
#include <easylocal/search/first_improvement.hpp>
#include <easylocal/search/metropolis_acceptance.hpp>
#include <easylocal/search/simulated_annealing.hpp>
#include <easylocal/search/temperature_policy.hpp>

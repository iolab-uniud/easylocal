#pragma once

// Dependency-free EasyLocal Core umbrella. Optional adapters such as
// ConfigTOML and TextUI deliberately remain opt-in headers/components.
#include <easylocal/core/aggregation.hpp>
#include <easylocal/app/app.hpp>
#include <easylocal/app/check.hpp>
#include <easylocal/core/cost.hpp>
#include <easylocal/core/cost_semantics.hpp>
#include <easylocal/helpers/cursor_moves.hpp>
#include <easylocal/core/logging.hpp>
#include <easylocal/helpers/neighborhood_concepts.hpp>
#include <easylocal/helpers/neighborhood_union.hpp>
#include <easylocal/runners/run_control.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/runners/search_run.hpp>
#include <easylocal/helpers/service_base.hpp>
#include <easylocal/helpers/solution_manager_concepts.hpp>
#include <easylocal/solvers/solver.hpp>
#include <easylocal/app/tester.hpp>
#include <easylocal/trace.hpp>

#include <easylocal/config/cli.hpp>
#include <easylocal/config/file.hpp>
#include <easylocal/config/overrides.hpp>
#include <easylocal/config/parameters.hpp>
#include <easylocal/config/setup.hpp>
#include <easylocal/config/tree.hpp>
#include <easylocal/config/validation.hpp>

#include <easylocal/runners/best_improvement.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/runners/metropolis_acceptance.hpp>
#include <easylocal/runners/simulated_annealing.hpp>
#include <easylocal/runners/temperature_policy.hpp>

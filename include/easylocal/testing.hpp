#pragma once

/// \file
/// The contract checks of user components (easylocal::testing), in one header.
///
/// The Core umbrella brings only what check(app) uses (check_options, the
/// reports, run_checks), not the component checks: include this header from
/// test executables.
// IWYU pragma: begin_exports
#include <easylocal/testing/check.hpp>
#include <easylocal/testing/cost_component.hpp>
#include <easylocal/testing/delta_cost_component.hpp>
#include <easylocal/testing/fixture.hpp>
#include <easylocal/testing/neighborhood.hpp>
#include <easylocal/testing/solution_manager.hpp>
// IWYU pragma: end_exports

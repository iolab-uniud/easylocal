#pragma once

// Dependency-free EasyLocal Core umbrella. Optional adapters under
// easylocal/adapters/ deliberately remain opt-in headers/components.
#include <easylocal/core/aggregation.hpp>
#include <easylocal/core/cost.hpp>
#include <easylocal/core/cost_semantics.hpp>
#include <easylocal/core/logging.hpp>

#include <easylocal/config/cli.hpp>
#include <easylocal/config/file.hpp>
#include <easylocal/config/overrides.hpp>
#include <easylocal/config/parameters.hpp>
#include <easylocal/config/setup.hpp>
#include <easylocal/config/tree.hpp>
#include <easylocal/config/validation.hpp>

#include <easylocal/helpers.hpp>
#include <easylocal/runners.hpp>
#include <easylocal/solvers/solver.hpp>
#include <easylocal/trace.hpp>

#include <easylocal/app/app.hpp>
#include <easylocal/app/check.hpp>
#include <easylocal/app/tester.hpp>

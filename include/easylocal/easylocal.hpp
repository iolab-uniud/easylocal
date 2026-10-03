#pragma once

// Dependency-free EasyLocal Core umbrella. Optional adapters under
// easylocal/adapters/ deliberately remain opt-in headers/components.
// IWYU pragma: begin_exports
#include <easylocal/app/app.hpp>
#include <easylocal/app/check.hpp>
#include <easylocal/app/run_parameters.hpp>
#include <easylocal/app/session.hpp>
#include <easylocal/config/cli.hpp>
#include <easylocal/config/file.hpp>
#include <easylocal/config/overrides.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/config/parameters.hpp>
#include <easylocal/config/setup.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/helpers.hpp>
#include <easylocal/runners.hpp>
#include <easylocal/solvers.hpp>
#include <easylocal/trace.hpp>
#include <easylocal/utils/hash.hpp>
#include <easylocal/utils/logging.hpp>
// IWYU pragma: end_exports

#pragma once

/// \file
/// The search tracing (easylocal::trace), in one header.
///
/// Include a header of trace/ to pull in only the events and the tracer
/// protocol, or one recorder.
// IWYU pragma: begin_exports
#include <easylocal/trace/binary.hpp>
#include <easylocal/trace/events.hpp>
#include <easylocal/trace/filter.hpp>
#include <easylocal/trace/jsonl.hpp>
#include <easylocal/trace/memory_recorder.hpp>
#include <easylocal/trace/tracer.hpp>
// IWYU pragma: end_exports

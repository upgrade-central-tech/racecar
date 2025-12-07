#pragma once

#include "../context.hpp"
#include "../gui.hpp"
#include "state.hpp"
#include "task_list.hpp"

namespace racecar::engine {

/// Performed every frame. Calls everything (compute and graphics).
/// If you want to run any call, add it into execute (this will be loooong).
void execute( State& engine, Context& ctx, TaskList& task_list, const gui::Gui& gui );

} // namespace racecar::engine

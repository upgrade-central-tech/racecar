#pragma once

#include "../context.hpp"
#include "../gui.hpp"
#include "state.hpp"
#include "task_list.hpp"

namespace racecar::engine {

void begin_frame();

/// Performed every frame. Calls everything (compute and graphics).
/// If you want to run any call, add it into execute (this will be loooong).
void execute( TaskList& task_list, const gui::Gui& gui );

} // namespace racecar::engine

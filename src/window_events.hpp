#pragma once

#include "atmosphere.hpp"
#include "context.hpp"
#include "engine/state.hpp"
#include "engine/ub_data.hpp"
#include "engine/uniform_buffer.hpp"
#include "gui.hpp"

#include <SDL3/SDL_events.h>

#include <vector>

namespace racecar {

void handle_sdl_window_events( Context& ctx, engine::State& engine, gui::Gui& gui,
    std::vector<UniformBuffer<ub_data::Material>>& material_uniform_buffers,
    atmosphere::Atmosphere& atms, bool& will_quit, bool& stop_drawing, SDL_Event& event );

}

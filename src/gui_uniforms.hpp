#pragma once

#include "atmosphere.hpp"
#include "context.hpp"
#include "engine/state.hpp"
#include "engine/ub_data.hpp"
#include "engine/uniform_buffer.hpp"
#include "gui.hpp"

#include <vector>

namespace racecar {

void update_debug_uniform_buffer( Context& ctx, engine::State& engine, gui::Gui& gui,
    atmosphere::Atmosphere& atms, UniformBuffer<ub_data::Debug>& debug_buffer );

void update_material_uniform_buffers( Context& ctx, engine::State& engine, gui::Gui& gui,
    std::vector<UniformBuffer<ub_data::Material>>& material_uniform_buffers,
    size_t num_materials );

}

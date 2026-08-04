#pragma once

#include "context.hpp"
#include "engine/state.hpp"
#include "engine/ub_data.hpp"
#include "engine/uniform_buffer.hpp"
#include "gui_material.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace racecar::gui {
struct Gui;
}

namespace racecar::atmosphere {
struct Atmosphere;
}

namespace racecar {

/// It's important to keep the types in sync, otherwise bad things may happen.
struct Preset {
    struct MaterialData {
        int slot = 0.f;
        gui::Material data;
    };

    unsigned int version = 0;
    std::string name;

    float sun_zenith = 0.f;
    float sun_azimuth = 0.f;

    float wetness = 0.f;
    float snow = 0.f;
    float scrolling_speed = 0.f;
    float bumpiness = 0.f;

    std::vector<MaterialData> materials;

    glm::vec3 camera_center = {};
    float camera_radius = 0.f;
    float camera_azimuth = 0.f;
    float camera_zenith = 0.f;

    std::optional<float> duration_opt;
};

struct PresetTransition {
    Preset before;
    Preset after;
    float progress = 0.f;
    float duration = 0.f;
};

std::vector<Preset> load_presets();
Preset parse_preset_json( std::filesystem::path json_path );

void update_preset_transition( gui::Gui& gui,
    std::vector<UniformBuffer<ub_data::Material>>& material_uniform_buffers,
    atmosphere::Atmosphere& atms );

}

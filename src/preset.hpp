#pragma once

#include "gui_material.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace racecar {

/// It's important to keep the types in sync, otherwise bad things may happen.
struct Preset {
    unsigned int version = 0;
    std::string name;

    float sun_zenith = 0.f;
    float sun_azimuth = 0.f;

    float wetness = 0.f;
    float snow = 0.f;
    float scrolling_speed = 0.f;
    float bumpiness = 0.f;

    struct MaterialData {
        int slot = 0.f;
        gui::Material data;
    };

    std::vector<MaterialData> materials;
};

std::vector<Preset> load_presets();
Preset parse_preset_json( std::filesystem::path json_path );

}

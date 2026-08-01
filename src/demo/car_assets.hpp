#pragma once

#include <string_view>

namespace racecar {

/// The car model the demo scene loads. The wheel tables in car_animation.cpp are keyed by this
/// path, so both the scene loader and the wheel animation have to agree on it.
constexpr std::string_view GLTF_FILE_PATH = "../assets/porsche.glb";

}

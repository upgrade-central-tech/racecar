#pragma once

enum class RacecarSettings {
    ENABLE_NORMAL_DEBUG_VIEW = 0,
    ENABLE_ALBEDO_DEBUG_VIEW,
    ENABLE_ROUGHNESS_DEBUG_VIEW,
    ENABLE_UV_DEBUG_VIEW,
    ENABLE_BLOOM,
    RACECAR_SETTINGS_LENGTH
};

namespace RuntimeSettings {

bool GetEnabled( RacecarSettings s );

bool GetValid( RacecarSettings s );

void SetEnabled( RacecarSettings s, bool enabled );

}

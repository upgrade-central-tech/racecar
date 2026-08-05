#include "settings.h"

#include <array>

typedef bool ( *validation_fn_t )( void );

namespace RuntimeSettings {

// -------------------------------------------------------------------
// -------------------RUNTIME VALIDATION FUNCTIONS--------------------
// -------------------------------------------------------------------
bool validate_bloom() { return !GetEnabled(RacecarSettings::ENABLE_NORMAL_DEBUG_VIEW); }

// -------------------------------------------------------------------
// -------------------------------------------------------------------
// -------------------------------------------------------------------

std::array<std::pair<validation_fn_t, bool>, (int)RacecarSettings::RACECAR_SETTINGS_LENGTH>
    settings {
        std::pair( nullptr, false ), // ENABLE_NORMAL_DEBUG_VIEW
        std::pair( nullptr, false ), // ENABLE_ALBEDO_DEBUG_VIEW
        std::pair( nullptr, false ), // ENABLE_ROUGHNESS_DEBUG_VIEW
        std::pair( nullptr, false ), // ENABLE_UV_DEBUG_VIEW
        std::pair( &validate_bloom, false ), // ENABLE_BLOOM
    };

bool GetEnabled( RacecarSettings s )
{
    bool resolver = settings[(size_t)s].first == nullptr || settings[(size_t)s].first();

    if ( !resolver ) {
        settings[(size_t)s].second = false;
    }

    return resolver && settings[(size_t)s].second;
}

bool GetValid( RacecarSettings s )
{
    bool resolver = settings[(size_t)s].first == nullptr || settings[(size_t)s].first();

    if ( !resolver ) {
        settings[(size_t)s].second = false;
    }

    return resolver;
}

void SetEnabled( RacecarSettings s, bool enabled )
{
    bool resolver = settings[(size_t)s].first == nullptr || settings[(size_t)s].first();

    if ( resolver ) {
        settings[(size_t)s].second = enabled;
    }
}

};

#include "settings.h"

#include <array>
#include <iterator>

namespace RuntimeSettings {

namespace {

typedef bool ( *validation_fn_t )( void );
typedef bool ( *option_validation_fn_t )( int option );

struct Setting {
    // Null => always usable
    validation_fn_t validate = nullptr;

    // Null => every option is usable
    option_validation_fn_t validate_option = nullptr;

    // Null => this is a bool setting
    const char* const* options = nullptr;
    int option_count = 0;

    // What the user desired
    int desired = 0;

    // If desired choice is invalid
    int fallback = 0;

    // Non-empty range means this is a float setting
    float min = 0.f;
    float max = 0.f;
    float desired_float = 0.f;
    float fallback_float = 0.f;
};

// -------------------------------------------------------------------
// ---------------------------OPTION LABELS---------------------------
// -------------------------------------------------------------------
constexpr const char* DEBUG_VIEW_OPTIONS[] = {
    "None",         "Albedo map only", "Normal map only",           "Roughness + metallic map only",
    "Normals only", "Albedo only",     "Roughness + metallic only",
};

constexpr const char* AA_OPTIONS[] = {
    "None",
    "TAA",
};

constexpr const char* TONEMAPPING_OPTIONS[] = {
    "None", "GT7 SDR", "GT7 HDR", "Reinhard", "ACES",
};

constexpr const char* DEBUG_TEXTURE_OPTIONS[] = {
    "None",
    "Reflections",
};

// -------------------------------------------------------------------
// -------------------RUNTIME VALIDATION FUNCTIONS--------------------
// -------------------------------------------------------------------
bool debug_view_is_off() { return Get<RacecarSettings::DEBUG_VIEW>() == DebugView::NONE; }

bool validate_aa() { return debug_view_is_off(); }

bool validate_bloom() { return debug_view_is_off(); }

bool validate_tonemapping()
{
    return debug_view_is_off() && Get<RacecarSettings::DEBUG_TEXTURE>() == DebugTexture::NONE;
}

bool validate_debug_texture_option( int option )
{
    if ( option == (int)DebugTexture::REFLECTIONS ) {
        return RACECAR_RAY_TRACING != 0;
    }

    return true;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
// -------------------------------------------------------------------

std::array<Setting, (size_t)RacecarSettings::RACECAR_SETTINGS_LENGTH> settings {
    Setting {
        .options = DEBUG_VIEW_OPTIONS,
        .option_count = (int)std::size( DEBUG_VIEW_OPTIONS ),
    }, // DEBUG_VIEW

    Setting { .validate = &validate_bloom }, // ENABLE_BLOOM

    Setting { .validate = &validate_aa,
              .options = AA_OPTIONS,
              .option_count = (int)std::size( AA_OPTIONS ),
              .desired = (int)AAMode::TAA,
              .fallback = (int)AAMode::NONE }, // AA_MODE

    Setting { .validate = &validate_tonemapping,
              .options = TONEMAPPING_OPTIONS,
              .option_count = (int)std::size( TONEMAPPING_OPTIONS ),
              .desired = (int)TonemappingMode::GT7_SDR,
              .fallback = (int)TonemappingMode::NONE }, // TONEMAPPING_MODE

    Setting { .validate_option = &validate_debug_texture_option,
              .options = DEBUG_TEXTURE_OPTIONS,
              .option_count = (int)std::size( DEBUG_TEXTURE_OPTIONS ),
              .desired = (int)DebugTexture::NONE,
              .fallback = (int)DebugTexture::NONE }, // DEBUG_TEXTURE

    Setting { .min = 0.f,
              .max = 2.f,
              .desired_float = 1.f,
              .fallback_float = 1.f }, // TERRAIN_IRRADIANCE_STRENGTH

    Setting { .min = 0.f,
              .max = 2.f,
              .desired_float = 1.f,
              .fallback_float = 1.f }, // CAR_IRRADIANCE_STRENGTH
};

}

bool GetValid( RacecarSettings s )
{
    const Setting& setting = settings[(size_t)s];
    return setting.validate == nullptr || setting.validate();
}

bool GetOptionValid( RacecarSettings s, int value )
{
    const Setting& setting = settings[(size_t)s];
    return setting.validate_option == nullptr || setting.validate_option( value );
}

int GetValue( RacecarSettings s )
{
    const Setting& setting = settings[(size_t)s];

    if ( !GetValid( s ) || !GetOptionValid( s, setting.desired ) ) {
        return setting.fallback;
    }

    return setting.desired;
}

void SetValue( RacecarSettings s, int value ) { settings[(size_t)s].desired = value; }

float GetFloat( RacecarSettings s )
{
    const Setting& setting = settings[(size_t)s];
    return GetValid( s ) ? setting.desired_float : setting.fallback_float;
}

void SetFloat( RacecarSettings s, float value )
{
    Setting& setting = settings[(size_t)s];
    setting.desired_float = value < setting.min ? setting.min
        : value > setting.max                   ? setting.max
                                                : value;
}

float GetFloatMin( RacecarSettings s ) { return settings[(size_t)s].min; }

float GetFloatMax( RacecarSettings s ) { return settings[(size_t)s].max; }

bool GetEnabled( RacecarSettings s ) { return GetValue( s ) != 0; }

void SetEnabled( RacecarSettings s, bool enabled ) { SetValue( s, enabled ? 1 : 0 ); }

int GetOptionCount( RacecarSettings s )
{
    const Setting& setting = settings[(size_t)s];
    return setting.options == nullptr ? 2 : setting.option_count;
}

const char* GetOptionLabel( RacecarSettings s, int value )
{
    const Setting& setting = settings[(size_t)s];

    if ( setting.options == nullptr || value < 0 || value >= setting.option_count ) {
        return "";
    }

    return setting.options[value];
}

}

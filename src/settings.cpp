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
};

// -------------------------------------------------------------------
// ---------------------------OPTION LABELS---------------------------
// -------------------------------------------------------------------
constexpr const char* AA_OPTIONS[] = {
    "None",
    "TAA",
};

// -------------------------------------------------------------------
// -------------------RUNTIME VALIDATION FUNCTIONS--------------------
// -------------------------------------------------------------------
bool validate_bloom() { return !GetEnabled( RacecarSettings::ENABLE_NORMAL_DEBUG_VIEW ); }

bool validate_aa() { return !GetEnabled( RacecarSettings::ENABLE_NORMAL_DEBUG_VIEW ); }

bool validate_options_aa( int option )
{
    return option == (int)AAMode::NONE || !GetEnabled( RacecarSettings::ENABLE_NORMAL_DEBUG_VIEW );
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
// -------------------------------------------------------------------

std::array<Setting, (size_t)RacecarSettings::RACECAR_SETTINGS_LENGTH> settings {
    Setting { }, // ENABLE_NORMAL_DEBUG_VIEW
    Setting { }, // ENABLE_ALBEDO_DEBUG_VIEW
    Setting { }, // ENABLE_ROUGHNESS_DEBUG_VIEW
    Setting { }, // ENABLE_UV_DEBUG_VIEW
    Setting { .validate = &validate_bloom }, // ENABLE_BLOOM
    Setting {
        .validate = &validate_aa,
        .validate_option = &validate_options_aa,
        .options = AA_OPTIONS,
        .option_count = (int)std::size( AA_OPTIONS ),
        .desired = (int)AAMode::TAA,
    }, // AA_MODE
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

#pragma once

// -------------------------------------------------------------------
// -----------------------ENUM SETTING DOMAINS------------------------
// -------------------------------------------------------------------

enum class AAMode : int {
    NONE = 0,
    TAA,
};

// -------------------------------------------------------------------
// -------------------------------------------------------------------
// -------------------------------------------------------------------

enum class RacecarSettings {
    ENABLE_NORMAL_DEBUG_VIEW = 0,
    ENABLE_ALBEDO_DEBUG_VIEW,
    ENABLE_ROUGHNESS_DEBUG_VIEW,
    ENABLE_UV_DEBUG_VIEW,
    ENABLE_BLOOM,
    AA_MODE,
    RACECAR_SETTINGS_LENGTH
};

namespace RuntimeSettings {

bool GetEnabled( RacecarSettings s );
void SetEnabled( RacecarSettings s, bool enabled );

int GetValue( RacecarSettings s );
void SetValue( RacecarSettings s, int value );

bool GetValid( RacecarSettings s );

bool GetOptionValid( RacecarSettings s, int value );

int GetOptionCount( RacecarSettings s );
const char* GetOptionLabel( RacecarSettings s, int value );

template <RacecarSettings S>
struct SettingValue;

template <>
struct SettingValue<RacecarSettings::AA_MODE> {
    using type = AAMode;
};

template <RacecarSettings S>
typename SettingValue<S>::type Get()
{
    return static_cast<typename SettingValue<S>::type>( GetValue( S ) );
}

template <RacecarSettings S>
void Set( typename SettingValue<S>::type value )
{
    SetValue( S, static_cast<int>( value ) );
}

}

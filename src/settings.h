#pragma once

// -------------------------------------------------------------------
// -----------------------ENUM SETTING DOMAINS------------------------
// -------------------------------------------------------------------

enum class AAMode : int {
    NONE = 0,
    TAA,
};

enum class DebugView : int {
    NONE = 0,
    
    // Not wired up yet
    ALBEDO_MAP,
    NORMAL_MAP,
    ROUGHNESS_METAL_MAP,

    NORMALS,
    ALBEDO,
    ROUGHNESS_METAL,
};

enum class TonemappingMode : int {
    NONE = 0,
    GT7_SDR,
    GT7_HDR,
    REINHARD,
    ACES
};

// -------------------------------------------------------------------
// -------------------------------------------------------------------
// -------------------------------------------------------------------

enum class RacecarSettings {
    DEBUG_VIEW = 0,
    ENABLE_BLOOM,
    AA_MODE,
    TONEMAPPING_MODE,
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

template <>
struct SettingValue<RacecarSettings::DEBUG_VIEW> {
    using type = DebugView;
};

template <>
struct SettingValue<RacecarSettings::TONEMAPPING_MODE> {
    using type = TonemappingMode;
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

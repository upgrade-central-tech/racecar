#pragma once

#include <type_traits>

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

enum class TonemappingMode : int { NONE = 0, GT7_SDR, GT7_HDR, REINHARD, ACES };

enum class DebugTexture : int {
    NONE = 0,
    REFLECTIONS,
};

// -------------------------------------------------------------------
// -------------------------------------------------------------------
// -------------------------------------------------------------------

enum class RacecarSettings {
    DEBUG_VIEW = 0,
    ENABLE_BLOOM,
    AA_MODE,
    TONEMAPPING_MODE,
    DEBUG_TEXTURE,
    TERRAIN_IRRADIANCE_STRENGTH,
    CAR_IRRADIANCE_STRENGTH,
    RACECAR_SETTINGS_LENGTH
};

namespace RuntimeSettings {

bool GetEnabled( RacecarSettings s );
void SetEnabled( RacecarSettings s, bool enabled );

int GetValue( RacecarSettings s );
void SetValue( RacecarSettings s, int value );

float GetFloat( RacecarSettings s );
void SetFloat( RacecarSettings s, float value );

float GetFloatMin( RacecarSettings s );
float GetFloatMax( RacecarSettings s );

bool GetValid( RacecarSettings s );

bool GetOptionValid( RacecarSettings s, int value );

int GetOptionCount( RacecarSettings s );
const char* GetOptionLabel( RacecarSettings s, int value );

template <RacecarSettings S> struct SettingValue;

template <> struct SettingValue<RacecarSettings::AA_MODE> {
    using type = AAMode;
};

template <> struct SettingValue<RacecarSettings::DEBUG_VIEW> {
    using type = DebugView;
};

template <> struct SettingValue<RacecarSettings::TONEMAPPING_MODE> {
    using type = TonemappingMode;
};

template <> struct SettingValue<RacecarSettings::DEBUG_TEXTURE> {
    using type = DebugTexture;
};

template <> struct SettingValue<RacecarSettings::TERRAIN_IRRADIANCE_STRENGTH> {
    using type = float;
};

template <> struct SettingValue<RacecarSettings::CAR_IRRADIANCE_STRENGTH> {
    using type = float;
};

template <RacecarSettings S> typename SettingValue<S>::type Get()
{
    if constexpr ( std::is_same_v<typename SettingValue<S>::type, float> ) {
        return GetFloat( S );
    } else {
        return static_cast<typename SettingValue<S>::type>( GetValue( S ) );
    }
}

template <RacecarSettings S> void Set( typename SettingValue<S>::type value )
{
    if constexpr ( std::is_same_v<typename SettingValue<S>::type, float> ) {
        SetFloat( S, value );
    } else {
        SetValue( S, static_cast<int>( value ) );
    }
}

}

#include "preset.hpp"

#include "exception.hpp"
#include "log.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string_view>

namespace racecar {

namespace fs = std::filesystem;

static constexpr unsigned int CURRENT_VERSION = 1;
static constexpr std::string_view PRESETS_FOLDER_PATH = "../presets";

std::vector<Preset> load_presets()
{

    std::vector<Preset> presets;

    log::info( "[preset] Loading presets from: {}", fs::absolute( PRESETS_FOLDER_PATH ).string() );

    for ( const auto& entry : fs::directory_iterator( PRESETS_FOLDER_PATH ) ) {
        const fs::path entry_path = entry.path();

        if ( entry_path.has_extension() && entry_path.extension() == ".json" ) {
            log::info( "[preset] Found preset: {}", entry_path.filename().string() );

            try {
                presets.push_back( parse_preset_json( entry_path ) );
            } catch ( const std::exception& ex ) {
                log::error( "[preset] Failed to parse preset: {}", ex.what() );
                throw;
            }
        }
    }

    return presets;
}

Preset parse_preset_json( fs::path json_path )
{
    using json = nlohmann::json;

    fs::path absolute = fs::absolute( json_path );
    std::ifstream file( absolute );

    if ( !file.is_open() ) {
        throw Exception( std::format(
            "[preset] Could not open preset JSON file \"{}\"", absolute.filename().string() ) );
    }

    json data = json::parse( file );
    int version = data["version"].get<int>();

    if ( version != CURRENT_VERSION ) {
        throw Exception(
            std::format( "[preset] \"version\" key must be set to {}!", CURRENT_VERSION ) );
    }

    Preset preset = {
        .version = static_cast<unsigned int>( version ),
        .name = absolute.stem().string(),

        .sun_zenith = data["sunZenith"].get<decltype( Preset::sun_zenith )>(),
        .sun_azimuth = data["sunAzimuth"].get<decltype( Preset::sun_azimuth )>(),

        .wetness = data["wetness"].get<decltype( Preset::wetness )>(),
        .snow = data["snow"].get<decltype( Preset::snow )>(),
        .scrolling_speed = data["scrollingSpeed"].get<decltype( Preset::scrolling_speed )>(),
        .bumpiness = data["bumpiness"].get<decltype( Preset::bumpiness )>(),
    };

    for ( const auto& material : data["materials"] ) {
        const json& material_color = material["color"];

        Preset::MaterialData material_data = {
            .slot = material["slot"].get<int>(),
            .data = gui::Material {
                .color = glm::vec4(material_color[0], material_color[1], material_color[2], material_color[3]),
                .roughness = material["roughness"].get<float>(),
                .metallic =  material["metallic"].get<float>(),
                .clearcoat_roughness =  material["clearcoatRoughness"].get<float>(),
                .clearcoat_weight =  material["clearcoatWeight"].get<float>(),
                .glintiness =  material["glintiness"].get<float>(),
                .glint_log_density =  material["glintLogDensity"].get<float>(),
                .glint_roughness =  material["glintRoughness"].get<float>(),
                .glint_randomness =  material["glintRandomness"].get<float>(),
            },
        };

        preset.materials.push_back( std::move( material_data ) );
    }

    return preset;
}

}

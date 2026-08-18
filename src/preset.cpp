#include "preset.hpp"

#include "atmosphere.hpp"
#include "gui.hpp"

#define GLM_ENABLE_EXPERIMENTAL // Necessary for glm::lerp
#include "exception.hpp"
#include "log.hpp"

#include <glm/gtx/compatibility.hpp>
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
            try {
                presets.push_back( parse_preset_json( entry_path ) );
            } catch ( const std::exception& ex ) {
                log::error( "[preset] Failed to parse preset: {}", ex.what() );
                throw;
            }
        }
    }

    log::info( "[preset] Loaded {} presets", presets.size() );

    return presets;
}

Preset parse_preset_json( fs::path json_path )
{
    using json = nlohmann::json;

    fs::path absolute = fs::absolute( json_path );
    std::ifstream file( absolute );

    if ( !file.is_open() ) {
        throw Exception(
            std::format(
                "[preset] Could not open preset JSON file \"{}\"",
                absolute.filename().string()
            )
        );
    }

    json data = json::parse( file );
    int version = data["version"].get<int>();

    if ( version != CURRENT_VERSION ) {
        throw Exception(
            std::format( "[preset] \"version\" key must be set to {}!", CURRENT_VERSION )
        );
    }

    const json& camera = data["camera"];
    const json& camera_center = camera["center"];

    Preset preset = {
        .version = static_cast<unsigned int>( version ),
        .name = absolute.stem().string(),

        .sun_zenith = data["sunZenith"].get<decltype( Preset::sun_zenith )>(),
        .sun_azimuth = data["sunAzimuth"].get<decltype( Preset::sun_azimuth )>(),

        .wetness = data["wetness"].get<decltype( Preset::wetness )>(),
        .snow = data["snow"].get<decltype( Preset::snow )>(),
        .scrolling_speed = data["scrollingSpeed"].get<decltype( Preset::scrolling_speed )>(),
        .bumpiness = data["bumpiness"].get<decltype( Preset::bumpiness )>(),

        .camera_center = { camera_center[0], camera_center[1], camera_center[2] },
        .camera_radius = camera["radius"].get<decltype( Preset::camera_radius )>(),
        .camera_azimuth = camera["azimuth"].get<decltype( Preset::camera_azimuth )>(),
        .camera_zenith = camera["zenith"].get<decltype( Preset::camera_zenith )>(),

        .duration_opt = data.contains( "duration" )
            ? std::make_optional( data["duration"].get<float>() )
            : std::nullopt,
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

void update_preset_transition(
    gui::Gui& gui,
    std::vector<UniformBuffer<ub_data::Material>>& material_uniform_buffers,
    atmosphere::Atmosphere& atms
)
{
    engine::State& engine = engine::State::GetMut();
    PresetTransition& transition = gui.preset.transition.value();

    float t = std::invoke( [&]() -> float {
        if ( transition.duration == 0.f ) {
            // Instantly complete transition if duration is zero
            return 1.f;
        }

        // Have to clamp it because progress might be greater than 1 after
        // adding the delta time
        return glm::saturate( transition.progress / transition.duration );
    } );

    {
        using enum gui::Gui::PresetData::Easing;

        switch ( gui.preset.easing ) {
        case LINEAR:
            break;

        case EASE_OUT_QUAD:
            t = glm::saturate( 1.f - ( 1.f - t ) * ( 1.f - t ) );
            break;

        case EASE_OUT_QUINT:
            t = glm::saturate( 1.f - std::pow( 1.f - t, 5.f ) );
            break;

        case EASE_IN_OUT_QUAD:
            t = glm::saturate(
                t < 0.5f ? 2.f * t * t : 1.f - std::pow( -2.f * t + 2.f, 2.f ) * 0.5f
            );
            break;

        case EASE_IN_OUT_QUINT:
            t = glm::saturate(
                t < 0.5f ? 16.f * t * t * t * t * t : 1.f - std::pow( -2.f * t + 2.f, 5.f ) * 0.5f
            );
            break;

        default:
            throw Exception( "[preset] Unhandled easing type" );
        }
    }

    // Initial and final
    const Preset& i = transition.before;
    const Preset& f = transition.after;

    atms.sun_zenith = glm::mix( i.sun_zenith, f.sun_zenith, t );
    atms.sun_azimuth = glm::mix( i.sun_azimuth, f.sun_azimuth, t );

    gui.terrain.wetness = glm::mix( i.wetness, f.wetness, t );
    gui.terrain.snow = glm::mix( i.snow, f.snow, t );
    engine.gamestate.speed = glm::mix( i.scrolling_speed, f.scrolling_speed, t );
    engine.gamestate.speed_lock = true;
    gui.demo.bumpiness = glm::mix( i.bumpiness, f.bumpiness, t );

    for ( size_t idx = 0; idx < i.materials.size(); ++idx ) {
        const gui::Material& i_mat = i.materials[idx].data;
        const gui::Material& f_mat = f.materials[idx].data;

        glm::vec4 color = glm::mix( i_mat.color, f_mat.color, t );
        float roughness = glm::mix( i_mat.roughness, f_mat.roughness, t );
        float metallic = glm::mix( i_mat.metallic, f_mat.metallic, t );
        float clearcoat = glm::mix( i_mat.clearcoat_weight, f_mat.clearcoat_weight, t );
        float clearcoat_roughness
            = glm::mix( i_mat.clearcoat_roughness, f_mat.clearcoat_roughness, t );
        float glintiness = glm::mix( i_mat.glintiness, f_mat.glintiness, t );
        float glint_log_density = glm::mix( i_mat.glint_log_density, f_mat.glint_log_density, t );
        float glint_roughness = glm::mix( i_mat.glint_roughness, f_mat.glint_roughness, t );
        float glint_randomness = glm::mix( i_mat.glint_randomness, f_mat.glint_randomness, t );

        size_t material_idx = static_cast<size_t>( i.materials[idx].slot );
        auto mat_data = material_uniform_buffers[material_idx].get_data();

        mat_data.base_color = color;
        mat_data.roughness = roughness;
        mat_data.metallic = metallic;
        mat_data.clearcoat = clearcoat;
        mat_data.clearcoat_roughness = clearcoat_roughness;
        mat_data.glintiness = glintiness;
        mat_data.glint_log_density = glint_log_density;
        mat_data.glint_roughness = glint_roughness;
        mat_data.glint_randomness = glint_randomness;

        gui.debug.color = color;
        gui.debug.roughness = roughness;
        gui.debug.metallic = metallic;
        gui.debug.clearcoat_weight = clearcoat;
        gui.debug.clearcoat_roughness = clearcoat_roughness;
        gui.debug.glintiness = glintiness;
        gui.debug.glint_log_density = glint_log_density;
        gui.debug.glint_roughness = glint_roughness;
        gui.debug.glint_randomness = glint_randomness;

        material_uniform_buffers[material_idx].set_data( mat_data );
        material_uniform_buffers[material_idx].update( engine.get_frame_index() );
    }

    engine.camera.center = glm::mix( i.camera_center, f.camera_center, t );
    engine.camera.radius = glm::mix( i.camera_radius, f.camera_radius, t );
    engine.camera.azimuth = glm::mix( i.camera_azimuth, f.camera_azimuth, t );
    engine.camera.zenith = glm::mix( i.camera_zenith, f.camera_zenith, t );

    transition.progress += static_cast<float>( engine.delta );

    if ( transition.progress >= transition.duration ) {
        // Finished the transition to the current preset
        gui.preset.transition = std::nullopt;
    }
}

}

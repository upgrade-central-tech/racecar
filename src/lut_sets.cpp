#include "lut_sets.hpp"

#include "engine/descriptors.hpp"
#include "engine/images.hpp"
#include "geometry/procedural.hpp"

namespace racecar {

namespace {

constexpr std::string_view BRDF_LUT_PATH = "../assets/LUT/brdf.png";

}

void create_lut_sets(
    Context& ctx,
    engine::State& engine,
    engine::DescriptorSet* lut_sets,
    vk::mem::AllocatedImage* lut_brdf,
    vk::mem::AllocatedImage* glint_noise
)
{
    // TODO: Add blue noise for future features
    *lut_sets = engine::generate_descriptor_set(
        ctx.vulkan,
        engine,
        {
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // BRDF LUT
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // Glint noise
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // Octahedral sky
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // Octahedral sky irradiance
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // Octahedral sky with mips
        },
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
    );

    *lut_brdf = engine::load_image(
        BRDF_LUT_PATH,
        ctx.vulkan,
        engine,
        2,
        VK_FORMAT_R16G16_SFLOAT,
        false
    );

    *glint_noise = geometry::generate_glint_noise( ctx.vulkan, engine );

    engine::update_descriptor_set_image(
        ctx.vulkan,
        engine,
        *lut_sets,
        *lut_brdf,
        LUT_INDEX::BRDF
    );
    engine::update_descriptor_set_image(
        ctx.vulkan,
        engine,
        *lut_sets,
        *glint_noise,
        LUT_INDEX::GLINT
    );
}

}

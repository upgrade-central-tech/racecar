#include "lut_sets.hpp"

#include "engine/descriptors.hpp"
#include "engine/image.h"
#include "geometry/procedural.hpp"

namespace racecar {

namespace {

constexpr std::string_view BRDF_LUT_PATH = "../assets/LUT/brdf.png";

}

void create_lut_sets(
    engine::DescriptorSet* lut_sets, engine::Image* lut_brdf, engine::Image* glint_noise
)
{
    // TODO: Add blue noise for future features
    *lut_sets = engine::generate_descriptor_set(
        {
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // BRDF LUT
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // Glint noise
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // Octahedral sky
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // Octahedral sky with mips
        },
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
    );

    *lut_brdf = engine::create_image_from_load(
        BRDF_LUT_PATH,
        2,
        VK_FORMAT_R16G16_SFLOAT,
        false
    );

    *glint_noise = geometry::generate_glint_noise();

    engine::update_descriptor_set_image(
        *lut_sets,
        *lut_brdf,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        LUT_INDEX::BRDF
    );
    engine::update_descriptor_set_image(
        *lut_sets,
        *glint_noise,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        LUT_INDEX::GLINT
    );
}

}

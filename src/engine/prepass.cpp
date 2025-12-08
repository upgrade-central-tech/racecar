#include "prepass.hpp"

namespace racecar::engine {

void PushDepthPrepassMS(
    DepthPrepassMS& depth_prepass_ms, engine::DrawResourceDescriptor draw_descriptor )
{
    depth_prepass_ms.depth_ms_gfx_task.draw_tasks.push_back( {
        .draw_resource_descriptor = draw_descriptor,
        .descriptor_sets = depth_prepass_ms.descriptor_sets,
        .pipeline = depth_prepass_ms.pipeline,
    } );
}

}

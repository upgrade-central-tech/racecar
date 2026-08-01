#include "window_events.hpp"

namespace racecar {

void handle_sdl_window_events(
    Context& ctx,
    engine::State& engine,
    gui::Gui& gui,
    std::vector<UniformBuffer<ub_data::Material>>& material_uniform_buffers,
    atmosphere::Atmosphere& atms,
    bool& will_quit,
    bool& stop_drawing,
    SDL_Event& event
)
{
    while ( SDL_PollEvent( &event ) ) {
        gui::process_event( gui, &event, atms, engine.camera, material_uniform_buffers );
        camera::process_event( ctx, &event, engine.camera, gui.show_window );

        if ( event.type == SDL_EVENT_QUIT ) {
            will_quit = true;
        }

        if ( event.type == SDL_EVENT_WINDOW_MINIMIZED ) {
            stop_drawing = true;
        } else if ( event.type == SDL_EVENT_WINDOW_RESTORED ) {
            stop_drawing = false;
        }
    }
}

}

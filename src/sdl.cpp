#include "sdl.hpp"

#include "exception.hpp"
#include "log.hpp"

namespace racecar::sdl {

SDL_Window* initialize( int screen_w, int screen_h, bool fullscreen )
{
    if ( !SDL_Init( SDL_INIT_VIDEO ) ) {
        log::error( "SDL_Init failed: {}", SDL_GetError() );
        throw Exception( "Failed to initialize SDL" );
    }

    SDL_WindowFlags flags = SDL_WINDOW_VULKAN;

    if ( fullscreen ) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }

    if ( SDL_Window* window = SDL_CreateWindow( "RACECAR", screen_w, screen_h, flags ); !window ) {
        log::error( "SDL_CreateWindow failed: {}", SDL_GetError() );
        throw Exception( "failed to initialize SDL" );
    } else {
        return window;
    }
}

void free( SDL_Window* window )
{
    SDL_DestroyWindow( window );
    SDL_Quit();
}

} // namespace racecar::sdl

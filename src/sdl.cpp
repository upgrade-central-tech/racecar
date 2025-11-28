#include "sdl.hpp"

#include "exception.hpp"
#include "log.hpp"

#if RACECAR_MACOS
#include <SDL3/SDL_vulkan.h>
#endif

namespace racecar::sdl {

#if RACECAR_MACOS
static constexpr std::string_view LIBVULKAN_PATH = "/usr/local/lib/libvulkan.1.dylib";
#endif

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

#if RACECAR_MACOS
    // When compiling on macOS, it is assumed you've installed the latest Vulkan SDK, and
    // made them available globally on your system (this should have been an option during SDK
    // setup). RACECAR does not bundle libvulkan with the binary; see the README for more info.
    if ( !SDL_Vulkan_LoadLibrary( LIBVULKAN_PATH.data() ) ) {
        log::error( "SDL_Vulkan_LoadLibrary failed: {}", SDL_GetError() );
        throw Exception( "Failed to dynamically load Vulkan on macOS" );
    }
#endif

    if ( SDL_Window* window = SDL_CreateWindow( "RACECAR", screen_w, screen_h, flags ); !window ) {
        log::error( "SDL_CreateWindow failed: {}", SDL_GetError() );
        throw Exception( "Failed to initialize SDL" );
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

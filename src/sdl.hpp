#pragma once

#include <SDL3/SDL.h>

namespace racecar::sdl {

/// Initializes SDL and creates a window.
SDL_Window* initialize( bool fullscreen );
void free( SDL_Window* window );

} // namespace racecar::sdl

#pragma once

#include <SDL3/SDL.h>

namespace racecar::sdl {

/// Initializes SDL and creates a window.
SDL_Window* initialize( int screen_w, int screen_h, bool fullscreen );
void free( SDL_Window* window );

} // namespace racecar::sdl

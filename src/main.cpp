#include "exception.hpp"
#include "log.hpp"
#include "racecar.hpp"

#include <SDL3/SDL_main.h>

#include <cstdlib>

/// Ideally we pass this in as a flag when running the program.
static constexpr bool USE_FULLSCREEN = true;

int main( int, char*[] )
{
    try {
        racecar::run( USE_FULLSCREEN );
    } catch ( const racecar::Exception& ex ) {
        racecar::log::error( "[RACECAR] {}", ex.what() );
        return EXIT_FAILURE;
    } catch ( const std::exception& ex ) {
        racecar::log::error( "[RACECAR] Caught standard exception: {}", ex.what() );
        return EXIT_FAILURE;
    } catch ( ... ) {
        racecar::log::error( "[RACECAR] Caught an unknown exception" );
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

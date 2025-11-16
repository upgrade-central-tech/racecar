#pragma once

#include <SDL3/SDL_log.h>

#include <format>
#include <source_location>
#include <utility>

/// Logging-related functionality. Use the functions in this namespace to print stuff
/// to the screen across various categories.
namespace racecar::log {

/// "INFO" is prepended to every message.
template <typename... Args> void info( std::format_string<Args...> format, Args&&... args )
{
    std::string message = std::format( format, std::forward<Args>( args )... );
    SDL_LogInfo( SDL_LOG_CATEGORY_APPLICATION, "INFO: %s", message.c_str() );
}

/// SDL will prepend "WARNING" to every message.
template <typename... Args> void warn( std::format_string<Args...> format, Args&&... args )
{
    std::string message = std::format( format, std::forward<Args>( args )... );
    SDL_LogWarn( SDL_LOG_CATEGORY_APPLICATION, "%s", message.c_str() );
}

/// SDL will prepend "ERROR" to every message.
template <typename... Args> struct error {
    error( std::format_string<Args...> format, Args&&... args,
        const std::source_location& loc = std::source_location::current() )
    {
        std::string message = std::format( format, std::forward<Args>( args )... );
        SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "[%s(%d:%d)] %s", loc.file_name(), loc.line(),
            loc.column(), message.c_str() );
    }
};

/// Deduction guide to make source location work after the variadic templates.
/// Taken from https://stackoverflow.com/a/57548488.
template <typename... Args>
error( std::format_string<Args...> format, Args&&... args ) -> error<Args...>;

}

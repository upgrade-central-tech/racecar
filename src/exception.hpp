#pragma once

#include <format>
#include <stdexcept>
#include <string>
#include <utility>

namespace racecar {

/// Differentiates RACECAR-thrown exceptions from other types,
class Exception : public std::runtime_error {
public:
    explicit Exception( const std::string& message )
        : std::runtime_error( message )
    {
    }

    template <typename... Args>
    Exception( std::format_string<Args...> format, Args&&... args )
        : std::runtime_error( std::format( format, std::forward<Args>( args )... ) )
    {
    }
};

}

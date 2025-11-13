#pragma once

#include <stdexcept>
#include <string>

namespace racecar {

/// Differentiates RACECAR-thrown exceptions from other types,
class Exception : public std::runtime_error {
public:
    explicit Exception( const std::string& message )
        : std::runtime_error( message )
    {
    }
};

}

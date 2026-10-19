#pragma once

#include <stdexcept>
#include <string>

namespace cuby {

// Error raised for anything that stops the program from running (SDL init,
// window creation, ...). Carries a human readable message.
class Exception : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

}  // namespace cuby

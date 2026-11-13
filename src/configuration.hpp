#pragma once
#include "Constants.hpp"

#define DEBUG_SERIAL_PORT Serial

// Guards allow build flags (e.g. the native test env) to override these.
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL (DEBUG_ANY)
#endif

#ifndef INFO_DISPLAY
#define INFO_DISPLAY 0
#endif

#pragma once

#include "Configuration.hpp"

#if DEBUG_LEVEL > 0
#define LOG(level, format, ...) logv((level), (F(format)), ##__VA_ARGS__)
String formatArg(const char* input, va_list args);
String format(const char* input, ...);
void logv(int levelFlags, String input, ...);

#else  // DEBUG_LEVEL>0
#define LOG(level, format, ...)
#endif  // DEBUG_LEVEL>0

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

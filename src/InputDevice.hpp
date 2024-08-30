#pragma once
#include <Arduino.h>
#include <WString.h>

#include "Utility.hpp"

enum class KeyPress {
    None,
    Up,
    Down,
    Left,
    Right,
};

enum class Capabilities {
    RawInput = 1,
    KeyPress = 2,
};

inline Capabilities operator|(Capabilities lhs, Capabilities rhs) {
    return static_cast<Capabilities>(
        static_cast<std::underlying_type<Capabilities>::type>(lhs) |
        static_cast<std::underlying_type<Capabilities>::type>(rhs)
        );
}

inline Capabilities& operator|=(Capabilities& lhs, Capabilities rhs) {
    lhs = lhs | rhs;
    return lhs;
}

inline Capabilities operator&(Capabilities lhs, Capabilities rhs) {
    return static_cast<Capabilities>(
        static_cast<std::underlying_type<Capabilities>::type>(lhs) &
        static_cast<std::underlying_type<Capabilities>::type>(rhs)
        );
}

inline Capabilities& operator&=(Capabilities& lhs, Capabilities rhs) {
    lhs = lhs & rhs;
    return lhs;
}

class IInputDevice {
public:
    virtual void timeSlice() = 0;
    virtual Capabilities supportedCapabilities() = 0;
    virtual bool isKeyPressed(KeyPress& pressed) = 0;
    virtual float rawY() = 0;
    virtual float rawX() = 0;
};

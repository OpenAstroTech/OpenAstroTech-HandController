#pragma once
#include <Arduino.h>

#include "Display/InfoDisplayRender.hpp"  // OledMenu library
#include <SSD1306Wire.h>

// This class renders the hand controller UI to a 128x64 pixel display
// controlled by a SSD1306 chip, on top of the OledMenu drawing surface.
class SDD1306OLED128x64 : public InfoDisplayRender
{
public:
    SDD1306OLED128x64(byte addr, int sda, int scl) : InfoDisplayRender()
    {
        _display = new SSD1306Wire(addr, sda, scl, GEOMETRY_128_64);
    }
};

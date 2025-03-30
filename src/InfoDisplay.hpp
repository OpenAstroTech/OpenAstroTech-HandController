#pragma once
#include <Arduino.h>
#include "SSD1306_128x64_Display.hpp" 

// Base class to implement a
class InfoDisplay : public SDD1306OLED128x64
{
public:
  InfoDisplay() : SDD1306OLED128x64(0x3c, SDA, SCL) {};

  virtual void drawContent()
  {

  };
};

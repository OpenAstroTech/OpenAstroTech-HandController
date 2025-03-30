#pragma once
#include <Arduino.h>

// Base class to implement a
class InfoDisplayRender
{
public:
  InfoDisplayRender() {};

  virtual void init() { Serial.println("InfoDisplayRender::init()"); };
  virtual void render() {};
  virtual void drawContent() = 0;
};

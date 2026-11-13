#pragma once
#include <Arduino.h>
#include "SSD1306_128x64_Display.hpp" 
#include "MenuSystem.hpp"

// Base class to implement a
class InfoDisplay : public SDD1306OLED128x64
{
private:
    MenuSystem* menuSystem;

public:
    InfoDisplay() : SDD1306OLED128x64(0x3c, SDA, SCL) {
        menuSystem = new MenuSystem(display);
    };

    virtual void drawContent() {
        menuSystem->render();
    };

    void initMenu() {
        menuSystem->init();
    }

    void addMenuItem(const char* text, void (*action)()) {
        menuSystem->addMenuItem(menuSystem->getRootItem(), text, action);
    }

    void addNumberMenuItem(const char* text, float min, float max, float step, float initial, void (*action)(float)) {
        menuSystem->addNumberMenuItem(menuSystem->getRootItem(), text, min, max, step, initial, action);
    }

    void navigateUp() {
        menuSystem->navigateUp();
    }

    void navigateDown() {
        menuSystem->navigateDown();
    }

    void select() {
        menuSystem->select();
    }

    void navigateLeft() {
        menuSystem->navigateLeft();
    }
};

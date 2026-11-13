#pragma once
#include <Arduino.h>
#include <WString.h>
#include "SSD1306_128x64_Display.hpp"
#include "InputDevice.hpp"

// Menu item structure
struct MenuItem {
    const char* text;
    void (*action)();
    void (*numberAction)(float);
    MenuItem* parent;
    MenuItem* next;
    MenuItem* prev;
    MenuItem* child;
    float minValue;
    float maxValue;
    float stepSize;
    float currentValue;
};

enum class MenuState {
    Normal,
    NumberInput
};

class MenuSystem {
private:
    SSD1306Wire* display;
    MenuItem* currentItem;
    MenuItem* rootItem;
    int selectedIndex;
    int visibleItems;
    int maxVisibleItems;
    int itemHeight;
    int startY;
    MenuState currentState;
    int cursorPosition;
    bool decimalPoint;
    String numberBuffer;

public:
    MenuSystem(SSD1306Wire* display);
    void init();
    void addMenuItem(MenuItem* parent, const char* text, void (*action)());
    void addNumberMenuItem(MenuItem* parent, const char* text, float min, float max, float step, float initial, void (*action)(float));
    void navigateUp();
    void navigateDown();
    void navigateLeft();
    void navigateRight();
    void select();
    void render();
    MenuItem* getRootItem() { return rootItem; }
    void handleNumberInput(KeyPress key);
    void enterNumberInput();
    void exitNumberInput();
    void updateNumberValue();
}; 
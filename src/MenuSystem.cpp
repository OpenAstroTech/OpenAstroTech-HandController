#include "MenuSystem.hpp"

MenuSystem::MenuSystem(SSD1306Wire* display) {
    this->display = display;
    this->rootItem = nullptr;
    this->currentItem = nullptr;
    this->selectedIndex = 0;
    this->visibleItems = 0;
    this->maxVisibleItems = 4;  // Number of items that can fit on screen
    this->itemHeight = 16;      // Height of each menu item in pixels
    this->startY = 0;           // Starting Y position for menu items
    this->currentState = MenuState::Normal;
    this->cursorPosition = 0;
    this->decimalPoint = false;
}

void MenuSystem::init() {
    // Create root menu item
    rootItem = new MenuItem();
    rootItem->text = "Main Menu";
    rootItem->action = nullptr;
    rootItem->numberAction = nullptr;
    rootItem->parent = nullptr;
    rootItem->next = nullptr;
    rootItem->prev = nullptr;
    rootItem->child = nullptr;
    rootItem->minValue = 0;
    rootItem->maxValue = 0;
    rootItem->stepSize = 0;
    rootItem->currentValue = 0;
    
    currentItem = rootItem;
}

void MenuSystem::addMenuItem(MenuItem* parent, const char* text, void (*action)()) {
    MenuItem* newItem = new MenuItem();
    newItem->text = text;
    newItem->action = action;
    newItem->numberAction = nullptr;
    newItem->parent = parent;
    newItem->next = nullptr;
    newItem->prev = nullptr;
    newItem->child = nullptr;
    newItem->minValue = 0;
    newItem->maxValue = 0;
    newItem->stepSize = 0;
    newItem->currentValue = 0;

    if (parent->child == nullptr) {
        parent->child = newItem;
    } else {
        MenuItem* lastChild = parent->child;
        while (lastChild->next != nullptr) {
            lastChild = lastChild->next;
        }
        lastChild->next = newItem;
        newItem->prev = lastChild;
    }
}

void MenuSystem::addNumberMenuItem(MenuItem* parent, const char* text, float min, float max, float step, float initial, void (*action)(float)) {
    MenuItem* newItem = new MenuItem();
    newItem->text = text;
    newItem->action = nullptr;
    newItem->numberAction = action;
    newItem->parent = parent;
    newItem->next = nullptr;
    newItem->prev = nullptr;
    newItem->child = nullptr;
    newItem->minValue = min;
    newItem->maxValue = max;
    newItem->stepSize = step;
    newItem->currentValue = initial;

    if (parent->child == nullptr) {
        parent->child = newItem;
    } else {
        MenuItem* lastChild = parent->child;
        while (lastChild->next != nullptr) {
            lastChild = lastChild->next;
        }
        lastChild->next = newItem;
        newItem->prev = lastChild;
    }
}

void MenuSystem::navigateUp() {
    if (currentState == MenuState::NumberInput) {
        // Increase current digit
        if (cursorPosition < numberBuffer.length()) {
            char c = numberBuffer.charAt(cursorPosition);
            if (c >= '0' && c < '9') {
                numberBuffer.setCharAt(cursorPosition, c + 1);
            } else if (c == '9') {
                numberBuffer.setCharAt(cursorPosition, '0');
            }
        }
    } else if (currentItem->prev != nullptr) {
        currentItem = currentItem->prev;
        selectedIndex = max(0, selectedIndex - 1);
    }
}

void MenuSystem::navigateDown() {
    if (currentState == MenuState::NumberInput) {
        // Decrease current digit
        if (cursorPosition < numberBuffer.length()) {
            char c = numberBuffer.charAt(cursorPosition);
            if (c > '0' && c <= '9') {
                numberBuffer.setCharAt(cursorPosition, c - 1);
            } else if (c == '0') {
                numberBuffer.setCharAt(cursorPosition, '9');
            }
        }
    } else if (currentItem->next != nullptr) {
        currentItem = currentItem->next;
        selectedIndex = min(visibleItems - 1, selectedIndex + 1);
    }
}

void MenuSystem::navigateLeft() {
    if (currentState == MenuState::NumberInput) {
        // Move cursor left
        if (cursorPosition > 0) {
            cursorPosition--;
        }
    } else if (currentItem->parent != nullptr) {
        currentItem = currentItem->parent;
        selectedIndex = 0;
    }
}

void MenuSystem::navigateRight() {
    if (currentState == MenuState::NumberInput) {
        // Move cursor right
        if (cursorPosition < numberBuffer.length() - 1) {
            cursorPosition++;
        }
    } else if (currentItem->child != nullptr) {
        currentItem = currentItem->child;
        selectedIndex = 0;
    }
}

void MenuSystem::select() {
    if (currentState == MenuState::NumberInput) {
        exitNumberInput();
    } else if (currentItem->numberAction != nullptr) {
        enterNumberInput();
    } else if (currentItem->action != nullptr) {
        currentItem->action();
    }
}

void MenuSystem::enterNumberInput() {
    currentState = MenuState::NumberInput;
    numberBuffer = String(currentItem->currentValue, 2);
    cursorPosition = 0;
    decimalPoint = numberBuffer.indexOf('.') != -1;
}

void MenuSystem::exitNumberInput() {
    currentState = MenuState::Normal;
    updateNumberValue();
    if (currentItem->numberAction != nullptr) {
        currentItem->numberAction(currentItem->currentValue);
    }
}

void MenuSystem::updateNumberValue() {
    float value = numberBuffer.toFloat();
    // Clamp value to min/max range
    value = max(currentItem->minValue, min(currentItem->maxValue, value));
    // Round to nearest step
    value = round(value / currentItem->stepSize) * currentItem->stepSize;
    currentItem->currentValue = value;
}

void MenuSystem::render() {
    display->clear();
    
    if (currentState == MenuState::NumberInput) {
        // Draw number input interface
        display->setFont(Bitmap5x7);
        display->setColor(WHITE);
        
        // Draw title
        display->drawString(0, 0, currentItem->text);
        
        // Draw number with cursor
        int y = itemHeight;
        for (int i = 0; i < numberBuffer.length(); i++) {
            if (i == cursorPosition) {
                display->drawString(i * 6, y, ">");
            }
            display->drawString(i * 6, y, String(numberBuffer.charAt(i)));
        }
        
        // Draw min/max values
        display->drawString(0, y + itemHeight, String(currentItem->minValue, 2));
        display->drawString(64, y + itemHeight, String(currentItem->maxValue, 2));
    } else {
        // Draw normal menu
        display->setFont(Bitmap5x7);
        display->setColor(WHITE);
        display->drawString(0, 0, currentItem->text);
        
        MenuItem* item = currentItem->child;
        int y = startY + itemHeight;
        int index = 0;
        
        while (item != nullptr && index < maxVisibleItems) {
            if (index == selectedIndex) {
                display->drawString(0, y, ">");
            }
            
            String itemText = String(item->text);
            if (item->numberAction != nullptr) {
                itemText += ": " + String(item->currentValue, 2);
            }
            display->drawString(10, y, itemText);
            
            item = item->next;
            y += itemHeight;
            index++;
        }
    }
    
    display->display();
} 
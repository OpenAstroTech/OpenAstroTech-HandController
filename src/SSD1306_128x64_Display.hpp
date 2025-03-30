#pragma once
#include "SSD1306Wire.h"
#include "Utility.hpp"
#include "fonts128x64.h"
#include "InfoDisplayRender.hpp"

const float sineSize = 18.0;
const uint8_t sineTable[] PROGMEM = { 0, 22, 44, 66, 87, 108, 128, 146, 164, 180, 195, 209, 221, 231, 240, 246, 251, 254, 255, 255 };

// This class renders the mount status to a 128x64 pixel display controlled by a SSD1306 chip.
class SDD1306OLED128x64 : public InfoDisplayRender
{
    SSD1306Wire* display;
    int yMaxStatus;
    int _addr;
    int _sda;
    int _scl;

public:
    SDD1306OLED128x64(byte addr, int sda, int scl) : InfoDisplayRender()
    {
        Serial.println("Instantiate display.");
        _addr = addr;
        _sda = sda;
        _scl = scl;
        yMaxStatus = 63;
    }

    // Initialize the display
    virtual void init()
    {
        Serial.println("Creating display.");
        display = new SSD1306Wire(_addr, _sda, _scl, GEOMETRY_128_64);
        Serial.println("Init display.");
        if (!display->init()) {
            Serial.println("Failed to init display.");
            return;
        }
        Serial.println("Clear display.");
        display->clear();
        Serial.println("Turn on display.");
        display->displayOn();
        Serial.println("Init complete.");
    };

    // Build the display content and display
    virtual void render()
    {
        display->clear();
        drawStatus();
        drawContent();
        display->display();
    };



    float sinLookup(float deg)
    {
        while (deg < 0.0f)
            deg += 360.0f;
        while (deg > 360.0f)
            deg -= 360.0f;

        if (deg <= 90)
        {
            int index = (int)roundf(sineSize * deg / 90.0f);
            return 1.0f * pgm_read_byte(sineTable + index) / 255.0;
        }
        else if (deg <= 180)
        {
            int index = (int)roundf(sineSize * (180.0f - deg) / 90.0f);
            return 1.0f * pgm_read_byte(sineTable + index) / 255.0;
        }
        else if (deg <= 270)
        {
            int index = (int)roundf(sineSize * (deg - 180.0f) / 90.0f);
            return -1.0f * pgm_read_byte(sineTable + index) / 255.0;
        }
        else if (deg <= 360)
        {
            int index = (int)roundf(sineSize * (360.0f - deg) / 90.0f);
            return -1.0f * pgm_read_byte(sineTable + index) / 255.0;
        }
        return 0.0f;
    }

    // Display the mount status string
    void drawStatus()
    {
        display->setColor(WHITE);
        display->setFont(Bitmap5x7);

        // Bouncing pixel (bounce frequency every 1.5s). 180 degrees is one cap.
        float deg = 180.0f * (millis() % 1500) / 1500.0f;
        int pixPos = (int)round(1.0f * yMaxStatus * sinLookup(deg));
        display->setPixel(0, 11 + yMaxStatus - pixPos);
    }
};

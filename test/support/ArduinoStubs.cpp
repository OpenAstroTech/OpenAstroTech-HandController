// Host-side implementations backing the Arduino.h test double.
#include "Arduino.h"

#include <stdio.h>

unsigned long fake_millis = 0;

static int default_analogRead(pin_size_t pin)
{
    (void) pin;
    return 512;  // centered stick
}

int (*fake_analogRead)(pin_size_t pin) = default_analogRead;

StubSerial Serial;

unsigned long millis(void)
{
    return fake_millis;
}

void delay(unsigned long ms)
{
    fake_millis += ms;
}

void pinMode(pin_size_t pin, PinMode mode)
{
    (void) pin;
    (void) mode;
}

int analogRead(pin_size_t pin)
{
    return fake_analogRead(pin);
}

// Non-ISO conversions String.cpp expects from the AVR world.
extern "C" {

char* itoa(int value, char* str, int radix)
{
    sprintf(str, radix == 10 ? "%d" : "%x", value);
    return str;
}

char* ltoa(long value, char* str, int radix)
{
    sprintf(str, radix == 10 ? "%ld" : "%lx", value);
    return str;
}

char* ultoa(unsigned long value, char* str, int radix)
{
    sprintf(str, radix == 10 ? "%lu" : "%lx", value);
    return str;
}

char* utoa(unsigned value, char* str, int radix)
{
    sprintf(str, radix == 10 ? "%u" : "%x", value);
    return str;
}

char* dtostrf(double val, signed char width, unsigned char prec, char* sout)
{
    sprintf(sout, "%*.*f", width, prec, val);
    return sout;
}

}  // extern "C"

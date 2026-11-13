// Host-side Arduino test double for the native (pio test) environment.
// Provides the real ArduinoCore-API String implementation plus injectable
// fakes for millis()/analogRead() and a capture-capable Serial stub, so the
// firmware's pure logic can be unit-tested on the development PC.
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <type_traits>

#include "api/Common.h"
#include "api/String.h"

// Fake clock, advanced by tests (and by delay()).
extern unsigned long fake_millis;

// Fake ADC, replaced by tests to feed analog readings per pin.
extern int (*fake_analogRead)(pin_size_t pin);

// Serial test double: feed() supplies input bytes, output captures printed bytes.
class StubSerial
{
public:
    void begin(long baudrate) { (void) baudrate; }

    void feed(const String& input) { _input = input; _pos = 0; }
    void clearOutput() { output = ""; }

    int available() { return (int) (_input.length() - _pos); }
    int read() { return _pos < _input.length() ? (int) _input[_pos++] : -1; }

    void print(const String& s) { output += s; }
    void print(const char* s) { output += s; }
    void print(char c) { output += c; }
    void println(const String& s) { output += s; output += '\n'; }
    void println(const char* s) { output += s; output += '\n'; }
    void flush() {}

    String output;

private:
    String _input;
    unsigned int _pos = 0;
};

extern StubSerial Serial;

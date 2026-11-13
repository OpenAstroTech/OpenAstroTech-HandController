// Test harness aggregator: include this from every test file INSTEAD of
// <unity.h>/<Arduino.h>.
//
// PlatformIO's Unity runner links only src/ (see build_src_filter in
// platformio.ini) plus the single test translation unit, so the support
// sources below must be instantiated into that test TU directly. Including
// them from exactly one TU per test program avoids duplicate symbols.
#pragma once

#include "Arduino.h"

// Real ArduinoCore-API String implementation (vendored from the arduino-pico
// core so tests exercise the same String the firmware runs).
#include "api/String.cpp"

// Host-side millis()/analogRead()/Serial test doubles.
#include "ArduinoStubs.cpp"

// NOTE: each test file must #include <unity.h> DIRECTLY (not via this
// header) so PlatformIO's library dependency finder resolves the Unity
// package for that test's build.

// Unity hooks required by the bundled Unity version (C linkage, as declared
// in unity.h). Defined here since each test program includes this header
// from exactly one translation unit.
extern "C" {
void setUp(void) {}
void tearDown(void) {}
}

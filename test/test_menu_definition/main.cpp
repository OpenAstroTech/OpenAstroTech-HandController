// MenuDefinition (src/MenuDefinition.hpp): building the menu definition
// string and gating its hardware-dependent blocks on the mount's :XGM#
// reply. Field matching is exact per comma-separated field - "FOC" is a
// substring of "NO_FOC" and "AUTO_AZ" of "AUTO_AZ_ALT", so substring
// matching would enable the wrong blocks. OledMenu headers are not on the
// native include path, so the def is asserted structurally; the real parse
// is exercised by the OledMenu simulator (Task 5 of the plan).
#include "TestHarness.hpp"
#include <unity.h>

#include "MenuDefinition.hpp"
#include <string.h>

// The bench mount's hardware profile, verbatim from the design doc.
static const char* const BENCH_XGM =
    "Mega,NEMA|16|3600,NEMA|16|3600,NO_GPS,AUTO_AZ_ALT,NO_GYRO,NO_LCD,"
    "INFO_I2C_SSD1306_128x64,NO_FOC,HSAH,HSAV,ENDSW_DEC";

// A plausible mount without Hall sensors and without AZ/ALT steppers.
static const char* const PLAIN_XGM =
    "Mega,NEMA|16|3600,NEMA|16|3600,NO_GPS,NO_GYRO,NO_LCD,"
    "INFO_I2C_SSD1306_128x64,NO_FOC,ENDSW_DEC";

static bool contains(const String& def, const char* needle)
{
    return strstr(def.c_str(), needle) != nullptr;
}

void test_null_or_empty_xgm_builds_everything()
{
    String def = buildMenuDef(nullptr);
    TEST_ASSERT_TRUE(contains(def, "Focus"));
    TEST_ASSERT_TRUE(contains(def, "Autohome RA"));
    TEST_ASSERT_TRUE(contains(def, "AZ +1 arcmin"));
    TEST_ASSERT_TRUE(contains(def, "ALT -1 arcmin"));

    String defEmpty = buildMenuDef("");
    TEST_ASSERT_TRUE(contains(defEmpty, "Focus"));
    TEST_ASSERT_TRUE(contains(defEmpty, "Autohome RA"));
}

void test_xgm_fields_are_matched_exactly()
{
    // Whole-field match only: NO_FOC must not satisfy FOC, AUTO_AZ_ALT
    // must not satisfy AUTO_AZ.
    TEST_ASSERT_FALSE(xgmHasField(BENCH_XGM, "FOC"));
    TEST_ASSERT_TRUE(xgmHasField(BENCH_XGM, "NO_FOC"));
    TEST_ASSERT_TRUE(xgmHasField(BENCH_XGM, "HSAH"));
    TEST_ASSERT_TRUE(xgmHasField(BENCH_XGM, "HSAV"));
    TEST_ASSERT_TRUE(xgmHasField("Mega,FOC,ENDSW_DEC", "FOC"));
    TEST_ASSERT_FALSE(xgmHasField(BENCH_XGM, "AUTO_AZ"));
    TEST_ASSERT_TRUE(xgmHasField(BENCH_XGM, "AUTO_AZ_ALT"));
    TEST_ASSERT_FALSE(xgmHasField(nullptr, "FOC"));
    TEST_ASSERT_FALSE(xgmHasField("", "FOC"));
    TEST_ASSERT_FALSE(xgmHasField("FOCFOC,NO_FOCX", "FOC"));  // not whole fields
}

void test_no_foc_does_not_enable_focus()
{
    String def = buildMenuDef(BENCH_XGM);
    TEST_ASSERT_FALSE(contains(def, "Focus"));
    TEST_ASSERT_FALSE(contains(def, "FOC"));

    String plain = buildMenuDef(PLAIN_XGM);
    TEST_ASSERT_FALSE(contains(plain, "Focus"));

    // A mount with a focuser gets the submenu.
    String withFoc = buildMenuDef(
        "Mega,NEMA|16|3600,NEMA|16|3600,NO_GPS,NO_GYRO,NO_LCD,FOC,ENDSW_DEC");
    TEST_ASSERT_TRUE(contains(withFoc, "Focus"));
}

void test_hall_sensors_gate_autohome()
{
    // The bench has HSAH and HSAV: both autohome blocks stay.
    String def = buildMenuDef(BENCH_XGM);
    TEST_ASSERT_TRUE(contains(def, "Autohome RA"));
    TEST_ASSERT_TRUE(contains(def, "Autohome DEC"));

    // Without the Hall-sensor fields the blocks are omitted.
    String plain = buildMenuDef(PLAIN_XGM);
    TEST_ASSERT_FALSE(contains(plain, "Autohome RA"));
    TEST_ASSERT_FALSE(contains(plain, "Autohome DEC"));

    // Only RA homing: DEC block omitted, RA block kept.
    String raOnly = buildMenuDef(
        "Mega,NEMA|16|3600,NEMA|16|3600,NO_GPS,NO_GYRO,NO_LCD,NO_FOC,HSAH,ENDSW_DEC");
    TEST_ASSERT_TRUE(contains(raOnly, "Autohome RA"));
    TEST_ASSERT_FALSE(contains(raOnly, "Autohome DEC"));
}

void test_az_alt_gate_nudges()
{
    // AUTO_AZ_ALT covers both axes.
    String def = buildMenuDef(BENCH_XGM);
    TEST_ASSERT_TRUE(contains(def, "AZ +1 arcmin"));
    TEST_ASSERT_TRUE(contains(def, "AZ -1 arcmin"));
    TEST_ASSERT_TRUE(contains(def, "ALT +1 arcmin"));
    TEST_ASSERT_TRUE(contains(def, "ALT -1 arcmin"));

    // AUTO_AZ only: AZ stays, ALT goes.
    String azOnly = buildMenuDef(
        "Mega,NEMA|16|3600,NEMA|16|3600,NO_GPS,AUTO_AZ,NO_GYRO,NO_LCD,NO_FOC,ENDSW_DEC");
    TEST_ASSERT_TRUE(contains(azOnly, "AZ +1 arcmin"));
    TEST_ASSERT_FALSE(contains(azOnly, "ALT +1 arcmin"));

    // AUTO_ALT only: ALT stays, AZ goes.
    String altOnly = buildMenuDef(
        "Mega,NEMA|16|3600,NEMA|16|3600,NO_GPS,AUTO_ALT,NO_GYRO,NO_LCD,NO_FOC,ENDSW_DEC");
    TEST_ASSERT_FALSE(contains(altOnly, "AZ +1 arcmin"));
    TEST_ASSERT_TRUE(contains(altOnly, "ALT +1 arcmin"));

    // Neither: both go.
    String plain = buildMenuDef(PLAIN_XGM);
    TEST_ASSERT_FALSE(contains(plain, "AZ +1 arcmin"));
    TEST_ASSERT_FALSE(contains(plain, "ALT +1 arcmin"));
}

void test_def_parses_structurally()
{
    String def = buildMenuDef(nullptr);
    const char* s = def.c_str();

    TEST_ASSERT_TRUE(strncmp(s, "Menu, 1, ", 9) == 0);

    // All four confirm submenus with their No/Yes action pairs.
    TEST_ASSERT_TRUE(contains(def, "Submenu, 110, \"Home mount?\""));
    TEST_ASSERT_TRUE(contains(def, "Action, 111, \"No\""));
    TEST_ASSERT_TRUE(contains(def, "Action, 112, \"Yes\""));
    TEST_ASSERT_TRUE(contains(def, "Submenu, 120, \"Park mount?\""));
    TEST_ASSERT_TRUE(contains(def, "Action, 121, \"No\""));
    TEST_ASSERT_TRUE(contains(def, "Action, 122, \"Yes\""));
    TEST_ASSERT_TRUE(contains(def, "Submenu, 310, \"Autohome RA?\""));
    TEST_ASSERT_TRUE(contains(def, "Action, 311, \"No\""));
    TEST_ASSERT_TRUE(contains(def, "Action, 312, \"Yes\""));
    TEST_ASSERT_TRUE(contains(def, "Submenu, 320, \"Autohome DEC?\""));
    TEST_ASSERT_TRUE(contains(def, "Action, 321, \"No\""));
    TEST_ASSERT_TRUE(contains(def, "Action, 322, \"Yes\""));

    // Nine Submenu lines: root children Goto/Align/Setup/Focus, nested
    // Target, four confirm screens (ten menus counting the root Menu line
    // - well inside the builder's MAX_MENUS of 14).
    int submenus = 0;
    for (const char* p = s; (p = strstr(p, "Submenu, ")) != nullptr; p++)
    {
        submenus++;
    }
    TEST_ASSERT_EQUAL_INT(9, submenus);
}

// A reflexive press on a confirm screen must be the safe one: "No" first.
void test_confirm_cursors_land_on_no()
{
    String def = buildMenuDef(nullptr);

    TEST_ASSERT_TRUE(strstr(def.c_str(), "Action, 111, \"No\"")
                     < strstr(def.c_str(), "Action, 112, \"Yes\""));
    TEST_ASSERT_TRUE(strstr(def.c_str(), "Action, 121, \"No\"")
                     < strstr(def.c_str(), "Action, 122, \"Yes\""));
    TEST_ASSERT_TRUE(strstr(def.c_str(), "Action, 311, \"No\"")
                     < strstr(def.c_str(), "Action, 312, \"Yes\""));
    TEST_ASSERT_TRUE(strstr(def.c_str(), "Action, 321, \"No\"")
                     < strstr(def.c_str(), "Action, 322, \"Yes\""));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_null_or_empty_xgm_builds_everything);
    RUN_TEST(test_xgm_fields_are_matched_exactly);
    RUN_TEST(test_no_foc_does_not_enable_focus);
    RUN_TEST(test_hall_sensors_gate_autohome);
    RUN_TEST(test_az_alt_gate_nudges);
    RUN_TEST(test_def_parses_structurally);
    RUN_TEST(test_confirm_cursors_land_on_no);
    return UNITY_END();
}

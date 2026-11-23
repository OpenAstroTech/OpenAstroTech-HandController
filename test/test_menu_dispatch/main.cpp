// MenuCommands (src/MenuCommands.hpp): the menu-id -> LX200 dispatch for the
// trimmed interaction tree (docs/superpowers/specs/2026-11-22-display-
// interaction-design.md). Every mount-relevant menu activity maps to a
// command the MessageJob lookup table knows; composites (slews, stores) are
// built with the compose helpers; menuActionClosesMenu marks the actions
// that drop the user back to the idle screen.
#include "TestHarness.hpp"
#include <unity.h>

#include "MenuCommands.hpp"
#include <string.h>

void test_direct_commands()
{
    TEST_ASSERT_EQUAL_STRING(":hF#", menuIdToCommand(ID_GOTO_HOME_YES));
    TEST_ASSERT_EQUAL_STRING(":hP#", menuIdToCommand(ID_GOTO_PARK_YES));
    // The firmware's Unpark merely restarts tracking; :hU# does exactly that
    // and answers "1".
    TEST_ASSERT_EQUAL_STRING(":hU#", menuIdToCommand(ID_GOTO_UNPARK));
    // Direction letters are the builder-tunable defaults and mandatory: a
    // bare :MHR# is rejected by the mount's movement parser.
    TEST_ASSERT_EQUAL_STRING(":MHRr#", menuIdToCommand(ID_ALIGN_AUTOHOME_RA_YES));
    TEST_ASSERT_EQUAL_STRING(":MHDd#", menuIdToCommand(ID_ALIGN_AUTOHOME_DEC_YES));
    TEST_ASSERT_EQUAL_STRING(":SHP#", menuIdToCommand(ID_ALIGN_SET_HOME));
    TEST_ASSERT_EQUAL_STRING(":MAZ+1.0#", menuIdToCommand(ID_ALIGN_AZ_PLUS));
    TEST_ASSERT_EQUAL_STRING(":MAZ-1.0#", menuIdToCommand(ID_ALIGN_AZ_MINUS));
    TEST_ASSERT_EQUAL_STRING(":MAL+1.0#", menuIdToCommand(ID_ALIGN_ALT_PLUS));
    TEST_ASSERT_EQUAL_STRING(":MAL-1.0#", menuIdToCommand(ID_ALIGN_ALT_MINUS));
    TEST_ASSERT_EQUAL_STRING(":F+#", menuIdToCommand(ID_FOCUS_IN));
    TEST_ASSERT_EQUAL_STRING(":F-#", menuIdToCommand(ID_FOCUS_OUT));
    TEST_ASSERT_EQUAL_STRING(":FQ#", menuIdToCommand(ID_FOCUS_STOP));
    // Stop-all includes tracking; :Qa# (manual moves only) is not this.
    TEST_ASSERT_EQUAL_STRING(":Q#", menuIdToCommand(ID_STOP_ALL));
}

void test_non_mount_ids_map_to_null()
{
    TEST_ASSERT_NULL(menuIdToCommand(ID_MENU_ROOT));
    TEST_ASSERT_NULL(menuIdToCommand(ID_TRACKING));
    TEST_ASSERT_NULL(menuIdToCommand(ID_JOG));
    TEST_ASSERT_NULL(menuIdToCommand(ID_SUB_GOTO));
    TEST_ASSERT_NULL(menuIdToCommand(ID_GOTO_OBJECT));  // composite
    TEST_ASSERT_NULL(menuIdToCommand(ID_GOTO_HOME_Q));
    TEST_ASSERT_NULL(menuIdToCommand(ID_GOTO_HOME_NO));
    TEST_ASSERT_NULL(menuIdToCommand(ID_GOTO_PARK_Q));
    TEST_ASSERT_NULL(menuIdToCommand(ID_GOTO_PARK_NO));
    TEST_ASSERT_NULL(menuIdToCommand(ID_SUB_TARGET));
    TEST_ASSERT_NULL(menuIdToCommand(ID_TARGET_RA_HOURS));
    TEST_ASSERT_NULL(menuIdToCommand(ID_TARGET_SLEW));  // composite
    TEST_ASSERT_NULL(menuIdToCommand(ID_SUB_ALIGN));
    TEST_ASSERT_NULL(menuIdToCommand(ID_ALIGN_AUTOHOME_RA_Q));
    TEST_ASSERT_NULL(menuIdToCommand(ID_ALIGN_AUTOHOME_RA_NO));
    TEST_ASSERT_NULL(menuIdToCommand(ID_ALIGN_AUTOHOME_DEC_Q));
    TEST_ASSERT_NULL(menuIdToCommand(ID_ALIGN_AUTOHOME_DEC_NO));
    TEST_ASSERT_NULL(menuIdToCommand(ID_SUB_SETUP));
    TEST_ASSERT_NULL(menuIdToCommand(ID_SETUP_HA_HOURS));
    TEST_ASSERT_NULL(menuIdToCommand(ID_SETUP_HA_STORE));  // composite
    TEST_ASSERT_NULL(menuIdToCommand(ID_SETUP_DATE_STORE));
    TEST_ASSERT_NULL(menuIdToCommand(ID_SETUP_TIME_STORE));
    TEST_ASSERT_NULL(menuIdToCommand(ID_SETUP_UTC));
    TEST_ASSERT_NULL(menuIdToCommand(ID_SETUP_UTC_STORE));  // composite
    TEST_ASSERT_NULL(menuIdToCommand(ID_SUB_FOCUS));
    TEST_ASSERT_NULL(menuIdToCommand(ID_FOCUS_RATE));
    TEST_ASSERT_NULL(menuIdToCommand(ID_MENU_EXIT));
    TEST_ASSERT_NULL(menuIdToCommand(999));
}

void test_tracking_toggle_maps_to_start_and_stop()
{
    TEST_ASSERT_EQUAL_STRING(":MT1#", trackingToggleToCommand("ON"));
    TEST_ASSERT_EQUAL_STRING(":MT0#", trackingToggleToCommand("OFF"));
    TEST_ASSERT_NULL(trackingToggleToCommand("1"));
    TEST_ASSERT_NULL(trackingToggleToCommand(""));
    TEST_ASSERT_NULL(trackingToggleToCommand(nullptr));
}

void test_rate_options_map_to_rate_commands()
{
    TEST_ASSERT_EQUAL_STRING(":RG#", rateOptionToCommand("Guide"));
    TEST_ASSERT_EQUAL_STRING(":RC#", rateOptionToCommand("Center"));
    TEST_ASSERT_EQUAL_STRING(":RM#", rateOptionToCommand("Find"));
    TEST_ASSERT_EQUAL_STRING(":RS#", rateOptionToCommand("Max"));
    TEST_ASSERT_NULL(rateOptionToCommand("Sidereal"));
    TEST_ASSERT_NULL(rateOptionToCommand(nullptr));
}

void test_focus_rate_options_map_to_speed_presets()
{
    TEST_ASSERT_EQUAL_STRING(":F1#", focusRateOptionToCommand("1 (smallest)"));
    TEST_ASSERT_EQUAL_STRING(":F2#", focusRateOptionToCommand("2 (small)"));
    TEST_ASSERT_EQUAL_STRING(":F3#", focusRateOptionToCommand("3 (large)"));
    TEST_ASSERT_EQUAL_STRING(":F4#", focusRateOptionToCommand("4 (largest)"));
    TEST_ASSERT_NULL(focusRateOptionToCommand("2"));
    TEST_ASSERT_NULL(focusRateOptionToCommand(nullptr));
}

// Actions that end in a motion or a session boundary drop the user back to
// the idle screen; settings, editors and the confirm screens' "No" do not.
void test_close_after_action_set()
{
    TEST_ASSERT_TRUE(menuActionClosesMenu(ID_GOTO_OBJECT));
    TEST_ASSERT_TRUE(menuActionClosesMenu(ID_TARGET_SLEW));
    TEST_ASSERT_TRUE(menuActionClosesMenu(ID_GOTO_HOME_YES));
    TEST_ASSERT_TRUE(menuActionClosesMenu(ID_GOTO_PARK_YES));
    TEST_ASSERT_TRUE(menuActionClosesMenu(ID_GOTO_UNPARK));
    TEST_ASSERT_TRUE(menuActionClosesMenu(ID_ALIGN_AUTOHOME_RA_YES));
    TEST_ASSERT_TRUE(menuActionClosesMenu(ID_ALIGN_AUTOHOME_DEC_YES));
    TEST_ASSERT_TRUE(menuActionClosesMenu(ID_SETUP_HA_STORE));
    TEST_ASSERT_TRUE(menuActionClosesMenu(ID_STOP_ALL));

    TEST_ASSERT_FALSE(menuActionClosesMenu(ID_TRACKING));
    TEST_ASSERT_FALSE(menuActionClosesMenu(ID_JOG));
    TEST_ASSERT_FALSE(menuActionClosesMenu(ID_MENU_EXIT));
    TEST_ASSERT_FALSE(menuActionClosesMenu(ID_GOTO_HOME_NO));
    TEST_ASSERT_FALSE(menuActionClosesMenu(ID_GOTO_PARK_NO));
    TEST_ASSERT_FALSE(menuActionClosesMenu(ID_ALIGN_SET_HOME));
    TEST_ASSERT_FALSE(menuActionClosesMenu(ID_SETUP_DATE_STORE));
    TEST_ASSERT_FALSE(menuActionClosesMenu(ID_SETUP_TIME_STORE));
    TEST_ASSERT_FALSE(menuActionClosesMenu(ID_SETUP_UTC_STORE));
    TEST_ASSERT_FALSE(menuActionClosesMenu(999));
}

void test_poi_options_resolve_to_coordinates()
{
    SkyCoordinate coord;

    TEST_ASSERT_TRUE(poiCoordinates("Polaris", &coord));
    TEST_ASSERT_EQUAL_UINT8(3, coord.raHours);
    TEST_ASSERT_EQUAL_UINT8(0, coord.raMinutes);
    TEST_ASSERT_EQUAL_UINT8(8, coord.raSeconds);
    TEST_ASSERT_FALSE(coord.decNegative);
    TEST_ASSERT_EQUAL_UINT8(89, coord.decDegrees);
    TEST_ASSERT_EQUAL_UINT8(21, coord.decMinutes);
    TEST_ASSERT_EQUAL_UINT8(6, coord.decSeconds);

    TEST_ASSERT_TRUE(poiCoordinates("M42 Orion Nebula", &coord));
    TEST_ASSERT_EQUAL_UINT8(5, coord.raHours);
    TEST_ASSERT_TRUE(coord.decNegative);
    TEST_ASSERT_EQUAL_UINT8(5, coord.decDegrees);

    TEST_ASSERT_TRUE(poiCoordinates("Veil Nebula", &coord));
    TEST_ASSERT_EQUAL_UINT8(20, coord.raHours);
    TEST_ASSERT_EQUAL_UINT8(51, coord.raMinutes);
    TEST_ASSERT_EQUAL_UINT8(28, coord.raSeconds);

    TEST_ASSERT_FALSE(poiCoordinates("Polaris ", &coord));  // name must match exactly
    TEST_ASSERT_FALSE(poiCoordinates("Krypton", &coord));
    TEST_ASSERT_FALSE(poiCoordinates("", &coord));
    TEST_ASSERT_FALSE(poiCoordinates(nullptr, &coord));
}

// The Target form: editor confirms update the state, Slew composes from it.
void test_target_state_collects_editor_values()
{
    TargetState state;
    initTargetState(&state);

    TEST_ASSERT_TRUE(updateTargetState(&state, ID_TARGET_RA_HOURS, "2"));
    TEST_ASSERT_TRUE(updateTargetState(&state, ID_TARGET_RA_MINUTES, "30"));
    TEST_ASSERT_TRUE(updateTargetState(&state, ID_TARGET_RA_SECONDS, "5"));
    TEST_ASSERT_TRUE(updateTargetState(&state, ID_TARGET_DEC_SIGN, "ON"));
    TEST_ASSERT_TRUE(updateTargetState(&state, ID_TARGET_DEC_DEGREES, "45"));
    TEST_ASSERT_TRUE(updateTargetState(&state, ID_TARGET_DEC_MINUTES, "30"));
    TEST_ASSERT_TRUE(updateTargetState(&state, ID_TARGET_DEC_SECONDS, "0"));

    SkyCoordinate coord;
    targetCoordinate(&state, &coord);
    TEST_ASSERT_EQUAL_UINT8(2, coord.raHours);
    TEST_ASSERT_EQUAL_UINT8(30, coord.raMinutes);
    TEST_ASSERT_EQUAL_UINT8(5, coord.raSeconds);
    TEST_ASSERT_TRUE(coord.decNegative);
    TEST_ASSERT_EQUAL_UINT8(45, coord.decDegrees);
    TEST_ASSERT_EQUAL_UINT8(30, coord.decMinutes);
    TEST_ASSERT_EQUAL_UINT8(0, coord.decSeconds);

    TEST_ASSERT_TRUE(updateTargetState(&state, ID_TARGET_DEC_SIGN, "OFF"));
    targetCoordinate(&state, &coord);
    TEST_ASSERT_FALSE(coord.decNegative);

    // Setup ids are not part of the Target form.
    TEST_ASSERT_FALSE(updateTargetState(&state, ID_SETUP_HA_HOURS, "7"));
    TEST_ASSERT_FALSE(updateTargetState(&state, 999, "1"));
}

void test_setup_state_collects_editor_values()
{
    SetupState state;
    initSetupState(&state);

    TEST_ASSERT_TRUE(updateSetupState(&state, ID_SETUP_HA_HOURS, "7"));
    TEST_ASSERT_TRUE(updateSetupState(&state, ID_SETUP_HA_MINUTES, "30"));
    TEST_ASSERT_TRUE(updateSetupState(&state, ID_SETUP_DATE_MONTH, "12"));
    TEST_ASSERT_TRUE(updateSetupState(&state, ID_SETUP_DATE_DAY, "31"));
    TEST_ASSERT_TRUE(updateSetupState(&state, ID_SETUP_DATE_YEAR, "26"));
    TEST_ASSERT_TRUE(updateSetupState(&state, ID_SETUP_TIME_HOURS, "23"));
    TEST_ASSERT_TRUE(updateSetupState(&state, ID_SETUP_TIME_MINUTES, "59"));
    TEST_ASSERT_TRUE(updateSetupState(&state, ID_SETUP_TIME_SECONDS, "59"));
    TEST_ASSERT_TRUE(updateSetupState(&state, ID_SETUP_UTC, "-5"));
    TEST_ASSERT_EQUAL_INT8(7, state.haHours);
    TEST_ASSERT_EQUAL_INT8(30, state.haMinutes);
    TEST_ASSERT_EQUAL_INT8(12, state.dateMonth);
    TEST_ASSERT_EQUAL_INT8(31, state.dateDay);
    TEST_ASSERT_EQUAL_INT8(26, state.dateYear);
    TEST_ASSERT_EQUAL_INT8(23, state.timeHours);
    TEST_ASSERT_EQUAL_INT8(59, state.timeMinutes);
    TEST_ASSERT_EQUAL_INT8(59, state.timeSeconds);
    TEST_ASSERT_EQUAL_INT32(-5, state.utcOffset);

    // Target ids are not part of Setup.
    TEST_ASSERT_FALSE(updateSetupState(&state, ID_TARGET_RA_HOURS, "2"));
    TEST_ASSERT_FALSE(updateSetupState(&state, 999, "1"));
}

// Formats exactly as the mount's Set parser reads them: :SH has no sign,
// :SL is colon-separated, :SC is the LX200 MM/DD/YY form, :SG needs the
// sign spelled out.
void test_setup_composes()
{
    SetupState state;
    initSetupState(&state);
    updateSetupState(&state, ID_SETUP_HA_HOURS, "7");
    updateSetupState(&state, ID_SETUP_HA_MINUTES, "30");
    updateSetupState(&state, ID_SETUP_TIME_HOURS, "23");
    updateSetupState(&state, ID_SETUP_TIME_MINUTES, "59");
    updateSetupState(&state, ID_SETUP_TIME_SECONDS, "59");
    updateSetupState(&state, ID_SETUP_DATE_MONTH, "12");
    updateSetupState(&state, ID_SETUP_DATE_DAY, "31");
    updateSetupState(&state, ID_SETUP_DATE_YEAR, "26");
    updateSetupState(&state, ID_SETUP_UTC, "2");

    char buf[24];
    composeShCommand(&state, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING(":SH07:30#", buf);
    composeSlCommand(&state, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING(":SL23:59:59#", buf);
    composeScCommand(&state, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING(":SC12/31/26#", buf);
    composeSgCommand(&state, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING(":SG+2#", buf);

    updateSetupState(&state, ID_SETUP_UTC, "-5");
    composeSgCommand(&state, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING(":SG-5#", buf);
}

// Formats exactly as the mount's Set parser reads them: :SrHH:MM:SS# and
// :Sd+DD*MM:SS# (sign mandatory, seconds required).
void test_compose_slew_commands()
{
    SkyCoordinate coord = {2, 30, 5, true, 45, 30, 0};
    char buf[24];

    composeSrCommand(&coord, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING(":Sr02:30:05#", buf);
    composeSdCommand(&coord, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING(":Sd-45*30:00#", buf);

    SkyCoordinate m31 = {0, 43, 52, false, 41, 22, 53};
    composeSrCommand(&m31, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING(":Sr00:43:52#", buf);
    composeSdCommand(&m31, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING(":Sd+41*22:53#", buf);
}

void test_parse_payload_splits_id_and_value()
{
    uint16_t id = 0;
    const char* value = nullptr;

    TEST_ASSERT_TRUE(parseMenuPayload("207|x", &id, &value));
    TEST_ASSERT_EQUAL_UINT16(ID_TARGET_SLEW, id);
    TEST_ASSERT_EQUAL_STRING("x", value);

    TEST_ASSERT_TRUE(parseMenuPayload("112|", &id, &value));
    TEST_ASSERT_EQUAL_UINT16(ID_GOTO_HOME_YES, id);
    TEST_ASSERT_EQUAL_STRING("", value);
}

void test_parse_payload_rejects_malformed_input()
{
    uint16_t id = 0;
    const char* value = nullptr;

    TEST_ASSERT_FALSE(parseMenuPayload("no-pipe", &id, &value));
    TEST_ASSERT_FALSE(parseMenuPayload(nullptr, &id, &value));
    TEST_ASSERT_FALSE(parseMenuPayload("|value", &id, &value));
    TEST_ASSERT_FALSE(parseMenuPayload("abc|value", &id, &value));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_direct_commands);
    RUN_TEST(test_non_mount_ids_map_to_null);
    RUN_TEST(test_tracking_toggle_maps_to_start_and_stop);
    RUN_TEST(test_rate_options_map_to_rate_commands);
    RUN_TEST(test_focus_rate_options_map_to_speed_presets);
    RUN_TEST(test_close_after_action_set);
    RUN_TEST(test_poi_options_resolve_to_coordinates);
    RUN_TEST(test_target_state_collects_editor_values);
    RUN_TEST(test_setup_state_collects_editor_values);
    RUN_TEST(test_setup_composes);
    RUN_TEST(test_compose_slew_commands);
    RUN_TEST(test_parse_payload_splits_id_and_value);
    RUN_TEST(test_parse_payload_rejects_malformed_input);
    return UNITY_END();
}

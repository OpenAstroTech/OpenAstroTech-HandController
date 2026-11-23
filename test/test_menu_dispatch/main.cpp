// MenuCommands (src/MenuCommands.hpp): the menu-id -> LX200 dispatch used by
// the on-device menu. The menu mirrors the mount firmware's LCD menu
// (OpenAstroTracker-Firmware c70-c78), so each item maps to the command its
// LCD counterpart performs: direct commands for actions, option lookups for
// Selects, :Sr/:Sd/:MS composites for the Goto and Target forms, and set
// commands composed from confirmed values for the Cal items. Payloads from
// the menu library ("id|value") must split cleanly.
#include "TestHarness.hpp"
#include <unity.h>

#include "MenuCommands.hpp"
#include <string.h>

void test_move_ids_map_to_lx200_move_commands()
{
    TEST_ASSERT_EQUAL_STRING(":Mn#", menuIdToCommand(ID_MOVE_NORTH));
    TEST_ASSERT_EQUAL_STRING(":Ms#", menuIdToCommand(ID_MOVE_SOUTH));
    TEST_ASSERT_EQUAL_STRING(":Me#", menuIdToCommand(ID_MOVE_EAST));
    TEST_ASSERT_EQUAL_STRING(":Mw#", menuIdToCommand(ID_MOVE_WEST));
    // Stop ends the manual moves only; :Q# would also stop tracking.
    TEST_ASSERT_EQUAL_STRING(":Qa#", menuIdToCommand(ID_MOVE_STOP));
}

void test_goto_ids_map_to_lx200_commands()
{
    TEST_ASSERT_EQUAL_STRING(":hF#", menuIdToCommand(ID_GOTO_HOME));
    // :hU# is what the firmware's Unpark does (restarts tracking, replies "1").
    TEST_ASSERT_EQUAL_STRING(":hU#", menuIdToCommand(ID_GOTO_UNPARK));
    TEST_ASSERT_EQUAL_STRING(":hP#", menuIdToCommand(ID_GOTO_PARK));
}

void test_set_home_maps_to_set_home_point()
{
    TEST_ASSERT_EQUAL_STRING(":SHP#", menuIdToCommand(ID_MOVE_SET_HOME));
}

// Clearing a DEC limit is the lowercase leaf (:XSDLl#), setting it the
// uppercase one with no payload (= current position).
void test_dec_limit_ids_map_to_set_and_clear_commands()
{
    TEST_ASSERT_EQUAL_STRING(":XSDLL#", menuIdToCommand(ID_CAL_SET_LOWER));
    TEST_ASSERT_EQUAL_STRING(":XSDLl#", menuIdToCommand(ID_CAL_CLEAR_LOWER));
    TEST_ASSERT_EQUAL_STRING(":XSDLU#", menuIdToCommand(ID_CAL_SET_UPPER));
    TEST_ASSERT_EQUAL_STRING(":XSDLu#", menuIdToCommand(ID_CAL_CLEAR_UPPER));
}

void test_az_alt_nudges_map_to_arcminute_moves()
{
    TEST_ASSERT_EQUAL_STRING(":MAZ+1.0#", menuIdToCommand(ID_CAL_AZ_PLUS));
    TEST_ASSERT_EQUAL_STRING(":MAZ-1.0#", menuIdToCommand(ID_CAL_AZ_MINUS));
    TEST_ASSERT_EQUAL_STRING(":MAL+1.0#", menuIdToCommand(ID_CAL_ALT_PLUS));
    TEST_ASSERT_EQUAL_STRING(":MAL-1.0#", menuIdToCommand(ID_CAL_ALT_MINUS));
}

void test_focus_ids_map_to_focus_commands()
{
    TEST_ASSERT_EQUAL_STRING(":F+#", menuIdToCommand(ID_FOCUS_IN));
    TEST_ASSERT_EQUAL_STRING(":F-#", menuIdToCommand(ID_FOCUS_OUT));
    TEST_ASSERT_EQUAL_STRING(":FQ#", menuIdToCommand(ID_FOCUS_STOP));
}

void test_non_mount_ids_map_to_null()
{
    TEST_ASSERT_NULL(menuIdToCommand(ID_MENU_ROOT));
    TEST_ASSERT_NULL(menuIdToCommand(ID_SUB_GOTO));
    TEST_ASSERT_NULL(menuIdToCommand(ID_SUB_TARGET));
    TEST_ASSERT_NULL(menuIdToCommand(ID_SUB_MOVE));
    TEST_ASSERT_NULL(menuIdToCommand(ID_SUB_CAL));
    TEST_ASSERT_NULL(menuIdToCommand(ID_SUB_FOCUS));
    TEST_ASSERT_NULL(menuIdToCommand(ID_SUB_HA));
    TEST_ASSERT_NULL(menuIdToCommand(ID_SUB_INFO));
    TEST_ASSERT_NULL(menuIdToCommand(ID_GOTO_OBJECT));  // composite, not a single command
    TEST_ASSERT_NULL(menuIdToCommand(ID_TARGET_RA_HOURS));
    TEST_ASSERT_NULL(menuIdToCommand(ID_TARGET_SLEW));  // composite
    TEST_ASSERT_NULL(menuIdToCommand(ID_HA_STORE));     // composite
    TEST_ASSERT_NULL(menuIdToCommand(ID_INFO_FIRMWARE));
    TEST_ASSERT_NULL(menuIdToCommand(ID_MENU_EXIT));
    TEST_ASSERT_NULL(menuIdToCommand(1234));
}

void test_rate_options_map_to_rate_commands()
{
    TEST_ASSERT_EQUAL_STRING(":RG#", rateOptionToCommand("Guide"));
    TEST_ASSERT_EQUAL_STRING(":RC#", rateOptionToCommand("Center"));
    TEST_ASSERT_EQUAL_STRING(":RM#", rateOptionToCommand("Find"));
    TEST_ASSERT_EQUAL_STRING(":RS#", rateOptionToCommand("Max"));
    TEST_ASSERT_NULL(rateOptionToCommand("Sidereal"));
    TEST_ASSERT_NULL(rateOptionToCommand(""));
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

// The Cal value items send their set command with the confirmed value. The
// UTC offset needs its sign spelled out (:SG+2#, :SG-5#) - the mount's
// reader rejects an unsigned offset.
void test_cal_values_compose_set_commands()
{
    char buf[24];

    TEST_ASSERT_TRUE(menuValueToCommand(ID_CAL_TRACK_SPEED, "1.023", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING(":XSS1.023#", buf);
    TEST_ASSERT_TRUE(menuValueToCommand(ID_CAL_RA_STEPS, "96.0", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING(":XSR96.0#", buf);
    TEST_ASSERT_TRUE(menuValueToCommand(ID_CAL_DEC_STEPS, "96.0", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING(":XSD96.0#", buf);
    TEST_ASSERT_TRUE(menuValueToCommand(ID_CAL_BACKLASH, "45", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING(":XSB45#", buf);

    TEST_ASSERT_TRUE(menuValueToCommand(ID_CAL_UTC_OFFSET, "2", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING(":SG+2#", buf);
    TEST_ASSERT_TRUE(menuValueToCommand(ID_CAL_UTC_OFFSET, "-5", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING(":SG-5#", buf);

    TEST_ASSERT_FALSE(menuValueToCommand(ID_CAL_SET_LOWER, "", buf, sizeof(buf)));
    TEST_ASSERT_FALSE(menuValueToCommand(1234, "1", buf, sizeof(buf)));
}

// The firmware's point-of-interest table (c722_menuPOI.hpp): the options of
// the Object select resolve to their coordinates.
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

// The Target and HA forms: each editor item's confirm updates the state,
// and the Slew/Store actions compose from it.
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
    TEST_ASSERT_TRUE(updateTargetState(&state, ID_HA_HOURS, "7"));
    TEST_ASSERT_TRUE(updateTargetState(&state, ID_HA_MINUTES, "30"));

    SkyCoordinate coord;
    targetCoordinate(&state, &coord);
    TEST_ASSERT_EQUAL_UINT8(2, coord.raHours);
    TEST_ASSERT_EQUAL_UINT8(30, coord.raMinutes);
    TEST_ASSERT_EQUAL_UINT8(5, coord.raSeconds);
    TEST_ASSERT_TRUE(coord.decNegative);
    TEST_ASSERT_EQUAL_UINT8(45, coord.decDegrees);
    TEST_ASSERT_EQUAL_UINT8(30, coord.decMinutes);
    TEST_ASSERT_EQUAL_UINT8(0, coord.decSeconds);

    // Cal items and unknown ids are not part of the form.
    TEST_ASSERT_FALSE(updateTargetState(&state, ID_CAL_TRACK_SPEED, "1.0"));
    TEST_ASSERT_FALSE(updateTargetState(&state, 1234, "1"));
}

void test_target_state_toggle_off_means_positive()
{
    TargetState state;
    initTargetState(&state);

    TEST_ASSERT_TRUE(updateTargetState(&state, ID_TARGET_DEC_SIGN, "ON"));
    TEST_ASSERT_TRUE(updateTargetState(&state, ID_TARGET_DEC_SIGN, "OFF"));

    SkyCoordinate coord;
    targetCoordinate(&state, &coord);
    TEST_ASSERT_FALSE(coord.decNegative);
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

    SkyCoordinate polaris = {0, 43, 52, false, 41, 22, 53};
    composeSrCommand(&polaris, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING(":Sr00:43:52#", buf);
    composeSdCommand(&polaris, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING(":Sd+41*22:53#", buf);
}

// :SH accepts an unsigned two-digit hour angle and any single separator.
void test_compose_hour_angle_command()
{
    TargetState state;
    initTargetState(&state);
    updateTargetState(&state, ID_HA_HOURS, "7");
    updateTargetState(&state, ID_HA_MINUTES, "30");

    char buf[24];
    composeShCommand(&state, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING(":SH07:30#", buf);
}

void test_parse_payload_splits_id_and_value()
{
    uint16_t id = 0;
    const char* value = nullptr;

    TEST_ASSERT_TRUE(parseMenuPayload("305|Center", &id, &value));
    TEST_ASSERT_EQUAL_UINT16(ID_MOVE_RATE, id);
    TEST_ASSERT_EQUAL_STRING("Center", value);

    TEST_ASSERT_TRUE(parseMenuPayload("304|", &id, &value));
    TEST_ASSERT_EQUAL_UINT16(ID_MOVE_STOP, id);
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
    RUN_TEST(test_move_ids_map_to_lx200_move_commands);
    RUN_TEST(test_goto_ids_map_to_lx200_commands);
    RUN_TEST(test_set_home_maps_to_set_home_point);
    RUN_TEST(test_dec_limit_ids_map_to_set_and_clear_commands);
    RUN_TEST(test_az_alt_nudges_map_to_arcminute_moves);
    RUN_TEST(test_focus_ids_map_to_focus_commands);
    RUN_TEST(test_non_mount_ids_map_to_null);
    RUN_TEST(test_rate_options_map_to_rate_commands);
    RUN_TEST(test_focus_rate_options_map_to_speed_presets);
    RUN_TEST(test_cal_values_compose_set_commands);
    RUN_TEST(test_poi_options_resolve_to_coordinates);
    RUN_TEST(test_target_state_collects_editor_values);
    RUN_TEST(test_target_state_toggle_off_means_positive);
    RUN_TEST(test_compose_slew_commands);
    RUN_TEST(test_compose_hour_angle_command);
    RUN_TEST(test_parse_payload_splits_id_and_value);
    RUN_TEST(test_parse_payload_rejects_malformed_input);
    return UNITY_END();
}

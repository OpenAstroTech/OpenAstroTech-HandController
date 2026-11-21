// MenuCommands (src/MenuCommands.hpp): the menu-id -> LX200 dispatch used by
// the on-device menu. Every mount-relevant menu activity must map to a
// command the MessageJob lookup table actually knows, and payloads from the
// menu library ("id|value") must split cleanly.
#include "TestHarness.hpp"
#include <unity.h>

#include "MenuCommands.hpp"

void test_move_ids_map_to_lx200_move_commands()
{
    TEST_ASSERT_EQUAL_STRING(":Mn#", menuIdToCommand(ID_MOVE_NORTH));
    TEST_ASSERT_EQUAL_STRING(":Ms#", menuIdToCommand(ID_MOVE_SOUTH));
    TEST_ASSERT_EQUAL_STRING(":Me#", menuIdToCommand(ID_MOVE_EAST));
    TEST_ASSERT_EQUAL_STRING(":Mw#", menuIdToCommand(ID_MOVE_WEST));
    TEST_ASSERT_EQUAL_STRING(":Q#", menuIdToCommand(ID_MOVE_STOP));
}

void test_park_unpark_home_map_to_lx200_commands()
{
    TEST_ASSERT_EQUAL_STRING(":hP#", menuIdToCommand(ID_MOUNT_PARK));
    TEST_ASSERT_EQUAL_STRING(":hU#", menuIdToCommand(ID_MOUNT_UNPARK));
    TEST_ASSERT_EQUAL_STRING(":hF#", menuIdToCommand(ID_MOUNT_HOME));
}

void test_non_mount_ids_map_to_null()
{
    TEST_ASSERT_NULL(menuIdToCommand(ID_MENU_ROOT));
    TEST_ASSERT_NULL(menuIdToCommand(ID_SUB_MOVE));
    TEST_ASSERT_NULL(menuIdToCommand(ID_SUB_RATE));
    TEST_ASSERT_NULL(menuIdToCommand(ID_SUB_ABOUT));
    TEST_ASSERT_NULL(menuIdToCommand(ID_ABOUT_VERSION));
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

void test_parse_payload_splits_id_and_value()
{
    uint16_t id = 0;
    const char* value = nullptr;

    TEST_ASSERT_TRUE(parseMenuPayload("21|Center", &id, &value));
    TEST_ASSERT_EQUAL_UINT16(ID_RATE_SLEW, id);
    TEST_ASSERT_EQUAL_STRING("Center", value);

    TEST_ASSERT_TRUE(parseMenuPayload("15|", &id, &value));
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
    RUN_TEST(test_park_unpark_home_map_to_lx200_commands);
    RUN_TEST(test_non_mount_ids_map_to_null);
    RUN_TEST(test_rate_options_map_to_rate_commands);
    RUN_TEST(test_parse_payload_splits_id_and_value);
    RUN_TEST(test_parse_payload_rejects_malformed_input);
    return UNITY_END();
}

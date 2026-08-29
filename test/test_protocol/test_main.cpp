#include <Protocol.h>
#include <unity.h>

#include <cstring>

void test_builds_canonical_controller_registration() {
  char output[160];
  TEST_ASSERT_TRUE(
    mindflayer::protocol::buildRegistration(output, sizeof(output), "controller1")
  );
  TEST_ASSERT_EQUAL_STRING(
    "{\"type\":\"registration\",\"controller-id\": \"controller1\",\"status\":\"connected\",\"receiver\":false}",
    output
  );
}

void test_builds_canonical_key_down_and_up_events() {
  char output[160];
  TEST_ASSERT_TRUE(
    mindflayer::protocol::buildKeyEvent(
      output,
      sizeof(output),
      "controller1",
      "W",
      true
    )
  );
  TEST_ASSERT_EQUAL_STRING(
    "{\"type\":\"key-event\",\"controller-id\": \"controller1\",\"key\":\"W\",\"state\":\"down\"}",
    output
  );

  TEST_ASSERT_TRUE(
    mindflayer::protocol::buildKeyEvent(
      output,
      sizeof(output),
      "controller1",
      "SPC",
      false
    )
  );
  TEST_ASSERT_EQUAL_STRING(
    "{\"type\":\"key-event\",\"controller-id\": \"controller1\",\"key\":\"SPC\",\"state\":\"up\"}",
    output
  );
}

void test_reports_truncated_protocol_messages() {
  char output[16];
  TEST_ASSERT_FALSE(
    mindflayer::protocol::buildRegistration(output, sizeof(output), "controller1")
  );
  TEST_ASSERT_EQUAL_CHAR('\0', output[sizeof(output) - 1]);
}

void test_requires_q_shift_and_space_for_restart() {
  TEST_ASSERT_TRUE(mindflayer::protocol::shouldRestart(true, true, true));
  TEST_ASSERT_FALSE(mindflayer::protocol::shouldRestart(false, true, true));
  TEST_ASSERT_FALSE(mindflayer::protocol::shouldRestart(true, false, true));
  TEST_ASSERT_FALSE(mindflayer::protocol::shouldRestart(true, true, false));
}

void test_parses_canonical_led_configuration() {
  DynamicJsonDocument document(1024);
  mindflayer::protocol::Configuration configuration;
  TEST_ASSERT_TRUE(mindflayer::protocol::parseConfiguration(
    document,
    "{\"type\":\"configuration\",\"controller-id\":\"controller1\","
      "\"led1\":{\"r\":1,\"g\":2,\"b\":3},"
      "\"led2\":{\"r\":4,\"g\":5,\"b\":6}}",
    configuration
  ));
  TEST_ASSERT_EQUAL_UINT8(1, configuration.led1.r);
  TEST_ASSERT_EQUAL_UINT8(2, configuration.led1.g);
  TEST_ASSERT_EQUAL_UINT8(3, configuration.led1.b);
  TEST_ASSERT_EQUAL_UINT8(4, configuration.led2.r);
  TEST_ASSERT_EQUAL_UINT8(5, configuration.led2.g);
  TEST_ASSERT_EQUAL_UINT8(6, configuration.led2.b);
}

void test_rejects_malformed_and_unrelated_configuration_messages() {
  DynamicJsonDocument document(1024);
  mindflayer::protocol::Configuration configuration;
  TEST_ASSERT_FALSE(mindflayer::protocol::parseConfiguration(
    document,
    "{malformed",
    configuration
  ));
  TEST_ASSERT_FALSE(mindflayer::protocol::parseConfiguration(
    document,
    "{\"type\":\"key-event\"}",
    configuration
  ));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_builds_canonical_controller_registration);
  RUN_TEST(test_builds_canonical_key_down_and_up_events);
  RUN_TEST(test_reports_truncated_protocol_messages);
  RUN_TEST(test_requires_q_shift_and_space_for_restart);
  RUN_TEST(test_parses_canonical_led_configuration);
  RUN_TEST(test_rejects_malformed_and_unrelated_configuration_messages);
  return UNITY_END();
}

#include <HealthGate.h>
#include <unity.h>

using mindflayer::health::State;

static State healthy() { return {true,true,true,true,true,true,true,true,true,true}; }

void test_only_complete_temporary_health_promotes() {
  State state=healthy(); TEST_ASSERT_TRUE(mindflayer::health::shouldPromote(state));
  bool* fields[]={&state.provisioning,&state.normalMode,&state.wifi,&state.pinnedTls,&state.wss,
                 &state.hmac,&state.registered,&state.serverAccepted,&state.acceptedVersionMatches};
  for(auto field:fields){*field=false;TEST_ASSERT_FALSE(mindflayer::health::shouldPromote(state));*field=true;}
  state.temporary=false;TEST_ASSERT_FALSE(mindflayer::health::shouldPromote(state));
}

void test_recovery_is_armed_only_for_permanent_boots() {
  TEST_ASSERT_TRUE(mindflayer::health::shouldArmSerialRecovery(false,true));
  TEST_ASSERT_FALSE(mindflayer::health::shouldArmSerialRecovery(true,false));
  TEST_ASSERT_FALSE(mindflayer::health::shouldArmSerialRecovery(true,true));
}

int main(){UNITY_BEGIN();RUN_TEST(test_only_complete_temporary_health_promotes);RUN_TEST(test_recovery_is_armed_only_for_permanent_boots);return UNITY_END();}

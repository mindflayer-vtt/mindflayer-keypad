namespace com {
namespace viromania {
namespace vtt {
namespace wss {
namespace KeyboardMatrix {

struct KeyState {
  KeyState(const char* setKey);
  char key[4];
  bool isDown;
};

KeyState (*getState())[4][3];

void initMatrix();
void detectKeys(void (*callback)(KeyState* key));
} // namespace KeyboardMatrix
} // namespace wss
} // namespace vtt
} // namespace viromania
} // namespace com
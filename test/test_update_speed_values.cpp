#include <Arduino.h>
#include <unity.h>

// --- Dummy global variables for testing ---
int joy_x = 0;
int joy_y = 0;
bool trigger = false;

// --- Mock Nunchuk class ---
class MockNunchuk {
public:
    int x, y;
    bool z;
    bool connect() { return true; }
    void update() {}
    int joyX() { return x; }
    int joyY() { return y; }
    bool buttonZ() { return z; }
};
MockNunchuk mockNunchuk;

// Zamockuj globalny nunchuk
MockNunchuk* nunchuk_ptr = &mockNunchuk;
#define nunchuk (*nunchuk_ptr)

// --- Minimal implementation of update_speed_values for testing ---
void update_speed_values(void* timer) {
    if (!nunchuk.connect()) {
        joy_x = 0;
        joy_y = 0;
        trigger = false;
        return;
    }
    nunchuk.update();
    joy_x = nunchuk.joyX();
    joy_y = nunchuk.joyY();
    trigger = nunchuk.buttonZ();
}

// --- Unit tests ---
void test_update_speed_values_forward() {
    mockNunchuk.x = 128;
    mockNunchuk.y = 255;
    mockNunchuk.z = false;
    update_speed_values(nullptr);
    TEST_ASSERT_EQUAL(128, joy_x);
    TEST_ASSERT_EQUAL(255, joy_y);
    TEST_ASSERT_FALSE(trigger);
}

void test_update_speed_values_trigger() {
    mockNunchuk.x = 128;
    mockNunchuk.y = 128;
    mockNunchuk.z = true;
    update_speed_values(nullptr);
    TEST_ASSERT_TRUE(trigger);
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_update_speed_values_forward);
    RUN_TEST(test_update_speed_values_trigger);
    UNITY_END();
}

void loop() {}
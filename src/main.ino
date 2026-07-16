#include <Arduino.h>

namespace {

// Change these values to match your wiring. Each receiver controls the LED
// at the same array position (receiver 0 -> LED 0, receiver 1 -> LED 1).
constexpr uint8_t RECEIVER_PINS[] = {25, 26};
constexpr uint8_t LED_PINS[] = {32, 33};
constexpr size_t TARGET_COUNT = sizeof(RECEIVER_PINS) / sizeof(RECEIVER_PINS[0]);

// Most three-pin demodulating IR receiver modules pull their output LOW when
// they detect IR. Change this to HIGH if your module is active-high.
constexpr uint8_t RECEIVER_ACTIVE_LEVEL = LOW;
constexpr uint8_t LED_ON_LEVEL = HIGH;
constexpr uint8_t LED_OFF_LEVEL = LOW;

constexpr uint32_t LED_ON_TIME_MS = 1000;
constexpr uint32_t HIT_LOCKOUT_MS = 250;

static_assert(TARGET_COUNT == sizeof(LED_PINS) / sizeof(LED_PINS[0]),
              "Each IR receiver must have a matching LED");

struct TargetState {
  bool wasActive = false;
  bool ledIsOn = false;
  uint32_t ledOffTime = 0;
  uint32_t nextAllowedHitTime = 0;
};

TargetState targets[TARGET_COUNT];

// This comparison continues to work when millis() wraps around.
bool timeReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

void registerHit(size_t target, uint32_t now) {
  digitalWrite(LED_PINS[target], LED_ON_LEVEL);
  targets[target].ledIsOn = true;
  targets[target].ledOffTime = now + LED_ON_TIME_MS;
  targets[target].nextAllowedHitTime = now + HIT_LOCKOUT_MS;

  Serial.printf("Hit detected on receiver %u (GPIO %u)\n",
                static_cast<unsigned>(target + 1),
                static_cast<unsigned>(RECEIVER_PINS[target]));
}

}  // namespace

void setup() {
  Serial.begin(115200);

  for (size_t target = 0; target < TARGET_COUNT; ++target) {
    pinMode(RECEIVER_PINS[target], INPUT_PULLUP);
    pinMode(LED_PINS[target], OUTPUT);
    digitalWrite(LED_PINS[target], LED_OFF_LEVEL);

    // Do not count a receiver already active during startup as a shot.
    targets[target].wasActive =
        digitalRead(RECEIVER_PINS[target]) == RECEIVER_ACTIVE_LEVEL;
  }

  Serial.println("Duck Hunt receivers ready");
}

void loop() {
  const uint32_t now = millis();

  for (size_t target = 0; target < TARGET_COUNT; ++target) {
    const bool isActive =
        digitalRead(RECEIVER_PINS[target]) == RECEIVER_ACTIVE_LEVEL;

    // Trigger only on the start of a pulse. The lockout prevents the many
    // pulses in one modulated IR transmission from being counted repeatedly.
    if (isActive && !targets[target].wasActive &&
        timeReached(now, targets[target].nextAllowedHitTime)) {
      registerHit(target, now);
    }
    targets[target].wasActive = isActive;

    if (targets[target].ledIsOn &&
        timeReached(now, targets[target].ledOffTime)) {
      digitalWrite(LED_PINS[target], LED_OFF_LEVEL);
      targets[target].ledIsOn = false;
    }
  }
}

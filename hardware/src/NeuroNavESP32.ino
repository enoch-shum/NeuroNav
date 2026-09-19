/*
  NeuroNavESP32.ino
  ESP32 + MPU-6050 (head-mounted) + Pololu VL53L0X + vibration motor.

  I2C: SDA GPIO 21, SCL GPIO 22.  Both sensors share this bus; they have
  different I2C addresses (MPU-6050 0x68, VL53L0X 0x29), so no mux is needed.

  WebSocket telemetry: ws://<ESP32-IP>:81/
*/

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include "I2Cdev.h"
#include "MPU6050.h"
#include <VL53L0X.h>

// ---------- Change these before uploading ----------
const char *WIFI_SSID = "YOUR_WIFI_NAME";
const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ---------- Hardware ----------
constexpr uint8_t SDA_PIN = 21;
constexpr uint8_t SCL_PIN = 22;
constexpr uint8_t MOTOR_PIN = 33; // GPIO -> transistor base/gate, NOT motor directly
constexpr bool SERIAL_PLOTTER_MODE = true;

// ---------- Design limits (millimetres / degrees) ----------
constexpr uint16_t MAX_USEFUL_DISTANCE_MM = 2000;
constexpr uint16_t NO_OBSTACLE_DISTANCE_MM = 2001; // means "2 m or farther"
constexpr uint16_t MIN_VALID_DISTANCE_MM = 30;
constexpr uint16_t HAPTIC_START_DISTANCE_MM = 1000; // begin buzzing inside 1 metre
constexpr uint16_t HAPTIC_FULL_DISTANCE_MM = 100;   // full strength at/inside 10 cm
constexpr uint8_t HAPTIC_START_DUTY = 85;            // enough to start most small motors
constexpr uint32_t RANGE_STALE_MS = 1500;
constexpr float FAINT_FINAL_TILT_DEG = 32.0f;
constexpr float FAINT_GYRO_DPS = 75.0f;
constexpr float FAINT_TILT_SPEED_DPS = 55.0f;
constexpr uint32_t FAINT_HOLD_MS = 15000;
constexpr uint32_t FAINT_REFRACTORY_MS = 5000;

MPU6050 mpu;
VL53L0X tof;
WebSocketsServer webSocket(81);

// ESP32 LEDC PWM API differs between Arduino-ESP32 2.x and 3.x.
void setMotor(uint8_t duty) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(MOTOR_PIN, duty);
#else
  ledcWrite(0, duty);
#endif
}

struct Median5 {
  uint16_t samples[5] = {0};
  uint8_t count = 0;
  uint8_t next = 0;

  void add(uint16_t value) {
    samples[next] = value;
    next = (next + 1) % 5;
    if (count < 5) count++;
  }

  uint16_t median() const {
    uint16_t ordered[5];
    for (uint8_t i = 0; i < count; ++i) ordered[i] = samples[i];
    for (uint8_t i = 1; i < count; ++i) {
      uint16_t key = ordered[i];
      int8_t j = i - 1;
      while (j >= 0 && ordered[j] > key) { ordered[j + 1] = ordered[j]; --j; }
      ordered[j + 1] = key;
    }
    return ordered[count / 2];
  }
};

Median5 distanceFilter;
float axFiltered = 0, ayFiltered = 0, azFiltered = 0;
float pitchDeg = 0, rollDeg = 0, gyroPitchDps = 0;
float baselinePitch = 0, baselineRoll = 0;
float faintTiltDeg = 0, gyroMotionDps = 0, previousFaintTiltDeg = 0;
uint16_t distanceMm = NO_OBSTACLE_DISTANCE_MM;
bool distanceValid = false;
bool calibrated = false;
bool fainted = false;
uint8_t motorDuty = 0;
uint32_t lastGoodRangeMs = 0, faintedAtMs = 0, lastFaintMs = 0;
uint32_t lastMpuMs = 0, lastRangeMs = 0, lastPublishMs = 0;

// A short upright calibration avoids assuming that the sensor is perfectly level.
float calibrationPitchSum = 0, calibrationRollSum = 0;
uint16_t calibrationSamples = 0;
uint32_t calibrationStartedMs = 0;

float lowPass(float previous, float current, float alpha) {
  return previous + alpha * (current - previous);
}

void webSocketEvent(uint8_t client, WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_CONNECTED) {
    Serial.printf("WebSocket client %u connected\n", client);
  } else if (type == WStype_DISCONNECTED) {
    Serial.printf("WebSocket client %u disconnected\n", client);
  }
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Connecting to %s", WIFI_SSID);
  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 20000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi ready. Dashboard WebSocket: ws://");
    Serial.print(WiFi.localIP());
    Serial.println(":81/");
  } else {
    Serial.println("Wi-Fi unavailable; sensors continue, WebSocket clients cannot connect.");
  }
}

void scanI2C() {
  Serial.println("I2C scan (expected: MPU 0x68, VL53L0X 0x29):");
  uint8_t found = 0;
  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  found 0x%02X\n", address);
      ++found;
    }
  }
  if (!found) Serial.println("  no I2C devices found");
}

void readMpuAndDetectFaint(uint32_t now) {
  int16_t ax, ay, az, gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  // MPU-6050 defaults: accelerometer +/-2 g (16384 LSB/g), gyro +/-250 dps.
  const float axG = ax / 16384.0f, ayG = ay / 16384.0f, azG = az / 16384.0f;
  axFiltered = lowPass(axFiltered, axG, 0.18f);
  ayFiltered = lowPass(ayFiltered, ayG, 0.18f);
  azFiltered = lowPass(azFiltered, azG, 0.18f);

  const float measuredPitch = atan2f(-axFiltered, sqrtf(ayFiltered * ayFiltered + azFiltered * azFiltered)) * 180.0f / PI;
  const float measuredRoll = atan2f(ayFiltered, azFiltered) * 180.0f / PI;
  pitchDeg = lowPass(pitchDeg, measuredPitch, 0.20f);
  rollDeg = lowPass(rollDeg, measuredRoll, 0.20f);
  gyroPitchDps = lowPass(gyroPitchDps, gx / 131.0f, 0.25f);

  if (!calibrated) {
    calibrationPitchSum += pitchDeg;
    calibrationRollSum += rollDeg;
    calibrationSamples++;
    if (now - calibrationStartedMs >= 3000 && calibrationSamples > 30) {
      baselinePitch = calibrationPitchSum / calibrationSamples;
      baselineRoll = calibrationRollSum / calibrationSamples;
      previousFaintTiltDeg = 0;
      calibrated = true;
      Serial.printf("Head reference set: pitch %.1f, roll %.1f\n", baselinePitch, baselineRoll);
    }
    return;
  }

  // The MPU can be mounted in either portrait direction or slightly rotated on
  // the headband.  Use total tilt away from its calibrated upright attitude,
  // not one assumed MPU axis. A slow look down will have a low tilt speed.
  const float relativePitch = pitchDeg - baselinePitch;
  const float relativeRoll = rollDeg - baselineRoll;
  faintTiltDeg = sqrtf(relativePitch * relativePitch + relativeRoll * relativeRoll);
  const float rawGyroMagnitude = sqrtf((gx / 131.0f) * (gx / 131.0f) +
                                       (gy / 131.0f) * (gy / 131.0f) +
                                       (gz / 131.0f) * (gz / 131.0f));
  gyroMotionDps = lowPass(gyroMotionDps, rawGyroMagnitude, 0.35f);
  const uint32_t elapsed = now - lastMpuMs;
  const float tiltSpeedDps = elapsed ? (faintTiltDeg - previousFaintTiltDeg) * 1000.0f / elapsed : 0;

  // Alarm requires ALL: substantial tilt, rapid tilt change, and corroborating
  // gyroscope motion. This deliberately ignores slow phone-looking and noise.
  if (!fainted && now - lastFaintMs > FAINT_REFRACTORY_MS &&
      faintTiltDeg >= FAINT_FINAL_TILT_DEG &&
      tiltSpeedDps >= FAINT_TILT_SPEED_DPS &&
      gyroMotionDps >= FAINT_GYRO_DPS) {
    fainted = true;
    faintedAtMs = now;
    lastFaintMs = now;
    Serial.println("FAINT ALERT: rapid head slump detected");
  }
  if (fainted && now - faintedAtMs >= FAINT_HOLD_MS) {
    fainted = false; // reporting continues; another rapid event is required to re-alert
    Serial.println("Faint alert timeout");
  }
  previousFaintTiltDeg = faintTiltDeg;
}

void readRange(uint32_t now) {
  const uint16_t raw = tof.readRangeContinuousMillimeters();
  const bool good = !tof.timeoutOccurred() && raw >= MIN_VALID_DISTANCE_MM && raw <= MAX_USEFUL_DISTANCE_MM;
  if (good) {
    distanceFilter.add(raw);
    distanceMm = distanceFilter.median();
    distanceValid = true;
    lastGoodRangeMs = now;
  } else if (now - lastGoodRangeMs > RANGE_STALE_MS) {
    // 8 m readings/timeouts are treated as no usable obstacle, never as a close object.
    distanceMm = NO_OBSTACLE_DISTANCE_MM;
    distanceValid = false;
  }
}

void updateHaptics(uint32_t now) {
  if (fainted) {
    setMotor(255);                 // full drive through the transistor
    motorDuty = 255;
    return;
  }
  if (!distanceValid || distanceMm >= HAPTIC_START_DISTANCE_MM) {
    setMotor(0);
    motorDuty = 0;
    return;
  }

  // Linear map: 85/255 just inside 1 m, increasing continuously to 255/255
  // by 10 cm. Steady PWM (rather than pulses) makes strength easier to feel.
  const uint16_t cappedDistance = max(HAPTIC_FULL_DISTANCE_MM, distanceMm);
  const float closeness = (float)(HAPTIC_START_DISTANCE_MM - cappedDistance) /
                          (HAPTIC_START_DISTANCE_MM - HAPTIC_FULL_DISTANCE_MM);
  motorDuty = HAPTIC_START_DUTY + (uint8_t)(closeness * (255 - HAPTIC_START_DUTY));
  setMotor(motorDuty);
}

void publishTelemetry(uint32_t now) {
  if (now - lastPublishMs < 50) return; // 20 Hz
  lastPublishMs = now;
  char json[260];
  snprintf(json, sizeof(json),
    "{\"pitch\":%.1f,\"roll\":%.1f,\"tilt\":%.1f,\"gyroMotion\":%.1f,\"distance\":%u,\"motors\":%u,\"motorDuty\":%u,\"fainted\":%s,\"posture\":\"%s\",\"distanceValid\":%s,\"timestampMs\":%lu}",
    pitchDeg - baselinePitch, rollDeg - baselineRoll, faintTiltDeg, gyroMotionDps,
    distanceMm, motorDuty, motorDuty, fainted ? "true" : "false", fainted ? "fainted" : "upright",
    distanceValid ? "true" : "false", (unsigned long)now);
  webSocket.broadcastTXT(json);
  // Arduino Serial Plotter recognizes named numeric fields.  Keep JSON on the
  // WebSocket, while USB serial remains easy to graph during bench testing.
  if (SERIAL_PLOTTER_MODE) {
    Serial.printf("pitch:%.2f,roll:%.2f,tilt:%.2f,gyro_motion:%.2f,distance_mm:%u,motor:%u,fainted:%u\n",
      pitchDeg - baselinePitch, rollDeg - baselineRoll, faintTiltDeg, gyroMotionDps,
      distanceMm, motorDuty, fainted ? 1 : 0);
  } else {
    Serial.println(json);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(SDA_PIN, SCL_PIN);
  // 100 kHz is more tolerant of breadboard leads and two modules' pull-ups.
  // Do not use 400 kHz until both devices are reliable at this speed.
  Wire.setClock(100000);
  delay(100);
  scanI2C();

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(MOTOR_PIN, 20000, 8);
#else
  ledcSetup(0, 20000, 8);
  ledcAttachPin(MOTOR_PIN, 0);
#endif
  setMotor(0);

  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("ERROR: MPU-6050 not found at 0x68. Check wiring/AD0.");
    while (true) delay(1000);
  }

  tof.setTimeout(80);
  if (!tof.init()) {
    Serial.println("ERROR: VL53L0X not found at 0x29. Check wiring.");
    while (true) delay(1000);
  }
  tof.setMeasurementTimingBudget(20000); // enables responsive ~20 Hz obstacle updates
  tof.startContinuous(50);

  calibrationStartedMs = millis();
  connectWiFi();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("Hold head sensor still and upright for the first 3 seconds.");
}

void loop() {
  const uint32_t now = millis();
  webSocket.loop();

  if (now - lastMpuMs >= 20) { // 50 Hz: preserves rapid-slump signal
    readMpuAndDetectFaint(now);
    lastMpuMs = now;
  }
  if (now - lastRangeMs >= 50) {
    readRange(now);
    lastRangeMs = now;
  }
  updateHaptics(now);
  publishTelemetry(now);
}
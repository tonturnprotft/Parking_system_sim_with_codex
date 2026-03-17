#include <Wire.h>
#include <SPI.h>
#include <Adafruit_VL53L0X.h>
#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif

// Pin configuration
const uint8_t I2C_SDA_PIN = 21;
const uint8_t I2C_SCL_PIN = 22;
const uint8_t SENSOR1_XSHUT_PIN = 32;
const uint8_t SENSOR2_XSHUT_PIN = 33;
const uint8_t BUZZER_PIN = 25;

// RGB moved away from ADXL345 SPI pins
const uint8_t RGB_RED_PIN = 16;
const uint8_t RGB_GREEN_PIN = 17;
const uint8_t RGB_BLUE_PIN = 4;
const bool RGB_IS_COMMON_ANODE = false;  // false = common cathode, true = common anode

// ADXL345 SPI pins from the user's working debug sketch
const uint8_t ADXL_CS_PIN = 5;
const uint8_t ADXL_SCK_PIN = 18;
const uint8_t ADXL_MISO_PIN = 19;
const uint8_t ADXL_MOSI_PIN = 23;

// New I2C addresses after XSHUT sequencing
const uint8_t SENSOR1_I2C_ADDRESS = 0x30;
const uint8_t SENSOR2_I2C_ADDRESS = 0x31;
const uint8_t TEST_ONLY_SENSOR = 0;  // 0 = both, 1 = only sensor 1, 2 = only sensor 2
const bool IGNORE_SIGNAL_FAIL = true;

// Distance thresholds in centimeters
const float DISTANCE_OFF_CM = 60.0f;
const float DISTANCE_SLOW_CM = 40.0f;
const float DISTANCE_MEDIUM_CM = 20.0f;
const float DISTANCE_FAST_CM = 10.0f;
const float ADXL_CONFIRM_ALERT_DISTANCE_CM = 5.0f;

// Timing
const unsigned long SENSOR_READ_INTERVAL_MS = 70;
const unsigned long SERIAL_PRINT_INTERVAL_MS = 100;
const unsigned long XSHUT_SETTLE_MS = 10;
const unsigned long RGB_YELLOW_BLINK_INTERVAL_MS = 300;
const unsigned long RGB_RED_BLINK_INTERVAL_MS = 120;
const unsigned long ADXL_READ_INTERVAL_MS = 40;
const unsigned long ADXL_PRIORITY_ALERT_MS = 5000;
const unsigned long ADXL_PRIORITY_FLASH_MS = 120;
const uint8_t ADXL_CALIBRATION_SAMPLES = 50;
const bool SERIAL_DEBUG_VERBOSE = false;  // false = status normal, true = debug many lines

// ADXL345 registers
const uint8_t REG_DEVID = 0x00;
const uint8_t REG_POWER_CTL = 0x2D;
const uint8_t REG_DATA_FORMAT = 0x31;
const uint8_t REG_DATAX0 = 0x32;
const uint8_t ADXL_EXPECTED_DEVID = 0xE5;

// ADXL345 tuning
const bool ADXL_FRONT_IS_POSITIVE_X = true;
const float ADXL_X_TRIGGER_G = 0.75f;
const float ADXL_DELTA_TRIGGER_G = 0.55f;
const float ADXL_BRAKE_TRIGGER_G = -0.65f;  // sudden deceleration when front is positive X

// Buzzer mode: set to false later if you switch to a passive buzzer
const bool BUZZER_IS_ACTIVE = true;
const bool BUZZER_ACTIVE_HIGH = true;
const uint8_t BUZZER_LEDC_CHANNEL = 0;
const uint8_t BUZZER_LEDC_RESOLUTION = 8;
const uint16_t BUZZER_PASSIVE_TONE_HZ = 400;

// Reverse parking style beep timing
const unsigned long SLOW_BEEP_ON_MS = 10;
const unsigned long SLOW_BEEP_OFF_MS = 400;
const unsigned long MEDIUM_BEEP_ON_MS = 10;
const unsigned long MEDIUM_BEEP_OFF_MS = 200;
const unsigned long FAST_BEEP_ON_MS = 10;
const unsigned long FAST_BEEP_OFF_MS = 100;

SPISettings adxlSPI(1000000, MSBFIRST, SPI_MODE3);

Adafruit_VL53L0X sensor1;
Adafruit_VL53L0X sensor2;

bool sensor1Ready = false;
bool sensor2Ready = false;
bool adxlReady = false;

bool sensor1Valid = false;
bool sensor2Valid = false;

uint8_t sensor1Status = 255;
uint8_t sensor2Status = 255;

uint16_t sensor1RawMm = 0;
uint16_t sensor2RawMm = 0;

float sensor1DistanceCm = -1.0f;
float sensor2DistanceCm = -1.0f;
float nearestDistanceCm = -1.0f;

int16_t adxlRawX = 0;
int16_t adxlRawY = 0;
int16_t adxlRawZ = 0;
float adxlXg = 0.0f;
float adxlYg = 0.0f;
float adxlZg = 0.0f;
float adxlBaselineXg = 0.0f;
float adxlFrontAxisG = 0.0f;
float adxlPrevFrontAxisG = 0.0f;
float adxlDeltaG = 0.0f;

unsigned long lastSensorReadMs = 0;
unsigned long lastSerialPrintMs = 0;
unsigned long lastBuzzerToggleMs = 0;
unsigned long lastAdxlReadMs = 0;
unsigned long lastPriorityToggleMs = 0;
unsigned long priorityAlertUntilMs = 0;

bool buzzerOutputOn = false;
bool rgbBlinkOn = false;
bool priorityBluePhase = false;

enum BuzzerMode {
  BUZZER_MODE_OFF,
  BUZZER_MODE_SLOW,
  BUZZER_MODE_MEDIUM,
  BUZZER_MODE_FAST,
  BUZZER_MODE_CONTINUOUS
};

enum RgbMode {
  RGB_MODE_OFF,
  RGB_MODE_GREEN,
  RGB_MODE_YELLOW_BLINK,
  RGB_MODE_YELLOW,
  RGB_MODE_RED_BLINK,
  RGB_MODE_RED,
  RGB_MODE_PRIORITY
};

struct BuzzerPattern {
  BuzzerMode mode;
  bool enabled;
  bool continuous;
  unsigned long onMs;
  unsigned long offMs;
};

BuzzerMode currentBuzzerMode = BUZZER_MODE_OFF;
RgbMode currentRgbMode = RGB_MODE_OFF;
unsigned long lastRgbToggleMs = 0;

void initSensors();
bool initSingleSensor(Adafruit_VL53L0X &sensor, uint8_t xshutPin, uint8_t newAddress, const char *label);
void initAdxl345();
bool calibrateAdxl345();
void updateAdxl345();
void triggerPriorityAlert();
bool isPriorityAlertActive();
bool isAdxlAlertConfirmedByVl53();
void csLow();
void csHigh();
void writeAdxlRegister(uint8_t reg, uint8_t value);
uint8_t readAdxlRegister(uint8_t reg);
void readAdxlMulti(uint8_t reg, uint8_t *buf, uint8_t len);
bool readAdxlRawXYZ(int16_t &x, int16_t &y, int16_t &z);
float readDistance1();
float readDistance2();
float readDistance(Adafruit_VL53L0X &sensor, bool isReady, uint8_t &statusOut, uint16_t &rawMmOut);
void updateNearestDistance();
void setupBuzzerHardware();
void setupRgbHardware();
void updateBuzzer();
void updateRgb();
void setBuzzerOutput(bool turnOn);
void setRgbColor(bool redOn, bool greenOn, bool blueOn);
BuzzerPattern getBuzzerPattern(float distanceCm);
RgbMode getRgbMode(float distanceCm);
const char *buzzerModeText(BuzzerMode mode);
const char *rgbModeText(RgbMode mode);
const char *rangeStatusText(uint8_t status);
void printSystemStatus();
void printDistanceField(float distanceCm, uint16_t rawMm, uint8_t status, bool isReady);
String buildDistanceFieldText(float distanceCm, uint16_t rawMm, uint8_t status, bool isReady);

void setup() {
  Serial.begin(115200);

  setupBuzzerHardware();
  setupRgbHardware();

  pinMode(SENSOR1_XSHUT_PIN, OUTPUT);
  pinMode(SENSOR2_XSHUT_PIN, OUTPUT);

  initSensors();
  initAdxl345();

  Serial.println();
  Serial.println("ESP32 + 2x VL53L0X reverse warning system");
  Serial.println("Sensor1 address: 0x30");
  Serial.println("Sensor2 address: 0x31");
  Serial.print("RGB pins: R=");
  Serial.print(RGB_RED_PIN);
  Serial.print(" G=");
  Serial.print(RGB_GREEN_PIN);
  Serial.print(" B=");
  Serial.println(RGB_BLUE_PIN);
  Serial.print("ADXL345 SPI: CS=");
  Serial.print(ADXL_CS_PIN);
  Serial.print(" SCK=");
  Serial.print(ADXL_SCK_PIN);
  Serial.print(" MISO=");
  Serial.print(ADXL_MISO_PIN);
  Serial.print(" MOSI=");
  Serial.println(ADXL_MOSI_PIN);
  if (TEST_ONLY_SENSOR == 1) {
    Serial.println("Test mode: only Sensor 1");
  } else if (TEST_ONLY_SENSOR == 2) {
    Serial.println("Test mode: only Sensor 2");
  } else {
    Serial.println("Test mode: both sensors");
  }
}

void loop() {
  const unsigned long now = millis();

  if (now - lastSensorReadMs >= SENSOR_READ_INTERVAL_MS) {
    lastSensorReadMs = now;
    sensor1DistanceCm = readDistance1();
    sensor2DistanceCm = readDistance2();
    updateNearestDistance();
  }

  if (now - lastAdxlReadMs >= ADXL_READ_INTERVAL_MS) {
    lastAdxlReadMs = now;
    updateAdxl345();
  }

  updateBuzzer();
  updateRgb();

  if (now - lastSerialPrintMs >= SERIAL_PRINT_INTERVAL_MS) {
    lastSerialPrintMs = now;
    printSystemStatus();
  }
}

void initSensors() {
  digitalWrite(SENSOR1_XSHUT_PIN, LOW);
  digitalWrite(SENSOR2_XSHUT_PIN, LOW);
  delay(XSHUT_SETTLE_MS);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);
  delay(XSHUT_SETTLE_MS);

  if (TEST_ONLY_SENSOR != 2) {
    sensor1Ready = initSingleSensor(sensor1, SENSOR1_XSHUT_PIN, SENSOR1_I2C_ADDRESS, "Sensor 1");
  } else {
    Serial.println("Sensor 1 disabled by TEST_ONLY_SENSOR");
  }

  if (TEST_ONLY_SENSOR != 1) {
    sensor2Ready = initSingleSensor(sensor2, SENSOR2_XSHUT_PIN, SENSOR2_I2C_ADDRESS, "Sensor 2");
  } else {
    Serial.println("Sensor 2 disabled by TEST_ONLY_SENSOR");
  }
}

bool initSingleSensor(Adafruit_VL53L0X &sensor, uint8_t xshutPin, uint8_t newAddress, const char *label) {
  digitalWrite(xshutPin, HIGH);
  delay(XSHUT_SETTLE_MS);

  if (!sensor.begin(newAddress, false, &Wire)) {
    Serial.print(label);
    Serial.println(" init failed");
    digitalWrite(xshutPin, LOW);
    delay(XSHUT_SETTLE_MS);
    return false;
  }

  Serial.print(label);
  Serial.print(" ready at 0x");
  Serial.println(newAddress, HEX);
  return true;
}

void initAdxl345() {
  pinMode(ADXL_CS_PIN, OUTPUT);
  csHigh();
  SPI.begin(ADXL_SCK_PIN, ADXL_MISO_PIN, ADXL_MOSI_PIN, ADXL_CS_PIN);

  Serial.println("[ADXL] init start");
  Serial.print("[ADXL] DEVID=0x");
  const uint8_t devid = readAdxlRegister(REG_DEVID);
  if (devid < 16) {
    Serial.print('0');
  }
  Serial.println(devid, HEX);

  if (devid != ADXL_EXPECTED_DEVID) {
    Serial.println("[ADXL] NOT detected");
    adxlReady = false;
    return;
  }

  writeAdxlRegister(REG_DATA_FORMAT, 0x08); // full resolution, +/-2g
  writeAdxlRegister(REG_POWER_CTL, 0x08);   // measurement mode

  const uint8_t fmt = readAdxlRegister(REG_DATA_FORMAT);
  const uint8_t pwr = readAdxlRegister(REG_POWER_CTL);
  Serial.print("[ADXL] DATA_FORMAT=0x");
  Serial.println(fmt, HEX);
  Serial.print("[ADXL] POWER_CTL=0x");
  Serial.println(pwr, HEX);

  adxlReady = (fmt == 0x08 && (pwr & 0x08) != 0);

  if (!adxlReady) {
    Serial.println("[ADXL] register verification failed");
    return;
  }

  if (!calibrateAdxl345()) {
    Serial.println("[ADXL] calibration failed");
    adxlReady = false;
    return;
  }

  Serial.println("[ADXL] ready");
}

bool calibrateAdxl345() {
  float sumXg = 0.0f;
  uint8_t validSamples = 0;

  for (uint8_t i = 0; i < ADXL_CALIBRATION_SAMPLES; i++) {
    int16_t x;
    int16_t y;
    int16_t z;
    if (readAdxlRawXYZ(x, y, z)) {
      sumXg += x / 256.0f;
      validSamples++;
    }
    delay(10);
  }

  if (validSamples == 0) {
    return false;
  }

  adxlBaselineXg = sumXg / validSamples;
  adxlFrontAxisG = 0.0f;
  adxlPrevFrontAxisG = 0.0f;
  adxlDeltaG = 0.0f;

  Serial.print("[ADXL] baselineXg=");
  Serial.println(adxlBaselineXg, 3);
  return true;
}

void updateAdxl345() {
  if (!adxlReady) {
    return;
  }

  int16_t x;
  int16_t y;
  int16_t z;
  if (!readAdxlRawXYZ(x, y, z)) {
    return;
  }

  adxlRawX = x;
  adxlRawY = y;
  adxlRawZ = z;
  adxlXg = x / 256.0f;
  adxlYg = y / 256.0f;
  adxlZg = z / 256.0f;

  adxlFrontAxisG = (adxlXg - adxlBaselineXg) * (ADXL_FRONT_IS_POSITIVE_X ? 1.0f : -1.0f);
  adxlDeltaG = adxlFrontAxisG - adxlPrevFrontAxisG;

  const bool frontShock = adxlFrontAxisG >= ADXL_X_TRIGGER_G;
  const bool rearShock = adxlFrontAxisG <= -ADXL_X_TRIGGER_G;
  const bool suddenBrake = adxlFrontAxisG <= ADXL_BRAKE_TRIGGER_G;
  const bool fastChange = fabs(adxlDeltaG) >= ADXL_DELTA_TRIGGER_G;

  if ((frontShock || rearShock || suddenBrake || fastChange) && isAdxlAlertConfirmedByVl53()) {
    triggerPriorityAlert();
  }

  adxlPrevFrontAxisG = adxlFrontAxisG;
}

void triggerPriorityAlert() {
  priorityAlertUntilMs = millis() + ADXL_PRIORITY_ALERT_MS;
}

bool isPriorityAlertActive() {
  return static_cast<long>(priorityAlertUntilMs - millis()) > 0;
}

bool isAdxlAlertConfirmedByVl53() {
  sensor1DistanceCm = readDistance1();
  sensor2DistanceCm = readDistance2();
  updateNearestDistance();
  lastSensorReadMs = millis();

  return nearestDistanceCm >= 0.0f && nearestDistanceCm < ADXL_CONFIRM_ALERT_DISTANCE_CM;
}

void csLow() {
  digitalWrite(ADXL_CS_PIN, LOW);
}

void csHigh() {
  digitalWrite(ADXL_CS_PIN, HIGH);
}

void writeAdxlRegister(uint8_t reg, uint8_t value) {
  SPI.beginTransaction(adxlSPI);
  csLow();
  SPI.transfer(reg);
  SPI.transfer(value);
  csHigh();
  SPI.endTransaction();
}

uint8_t readAdxlRegister(uint8_t reg) {
  SPI.beginTransaction(adxlSPI);
  csLow();
  SPI.transfer(reg | 0x80);
  const uint8_t value = SPI.transfer(0x00);
  csHigh();
  SPI.endTransaction();
  return value;
}

void readAdxlMulti(uint8_t reg, uint8_t *buf, uint8_t len) {
  SPI.beginTransaction(adxlSPI);
  csLow();
  SPI.transfer(reg | 0x80 | 0x40);
  for (uint8_t i = 0; i < len; i++) {
    buf[i] = SPI.transfer(0x00);
  }
  csHigh();
  SPI.endTransaction();
}

bool readAdxlRawXYZ(int16_t &x, int16_t &y, int16_t &z) {
  uint8_t buf[6];
  readAdxlMulti(REG_DATAX0, buf, 6);

  x = static_cast<int16_t>((buf[1] << 8) | buf[0]);
  y = static_cast<int16_t>((buf[3] << 8) | buf[2]);
  z = static_cast<int16_t>((buf[5] << 8) | buf[4]);
  return true;
}

float readDistance1() {
  return readDistance(sensor1, sensor1Ready, sensor1Status, sensor1RawMm);
}

float readDistance2() {
  return readDistance(sensor2, sensor2Ready, sensor2Status, sensor2RawMm);
}

float readDistance(Adafruit_VL53L0X &sensor, bool isReady, uint8_t &statusOut, uint16_t &rawMmOut) {
  statusOut = 255;
  rawMmOut = 0;

  if (!isReady) {
    return -1.0f;
  }

  VL53L0X_RangingMeasurementData_t measure;
  sensor.rangingTest(&measure, false);
  statusOut = measure.RangeStatus;
  rawMmOut = measure.RangeMilliMeter;

  if (measure.RangeMilliMeter == 0) {
    return -1.0f;
  }

  if (IGNORE_SIGNAL_FAIL && measure.RangeStatus == 2) {
    statusOut = 0;
    return measure.RangeMilliMeter / 10.0f;
  }

  if (measure.RangeStatus != 0) {
    return -1.0f;
  }

  return measure.RangeMilliMeter / 10.0f;
}

void updateNearestDistance() {
  sensor1Valid = sensor1DistanceCm >= 0.0f;
  sensor2Valid = sensor2DistanceCm >= 0.0f;

  if (sensor1Valid && sensor2Valid) {
    nearestDistanceCm = min(sensor1DistanceCm, sensor2DistanceCm);
  } else if (sensor1Valid) {
    nearestDistanceCm = sensor1DistanceCm;
  } else if (sensor2Valid) {
    nearestDistanceCm = sensor2DistanceCm;
  } else {
    nearestDistanceCm = -1.0f;
  }
}

void setupBuzzerHardware() {
  pinMode(BUZZER_PIN, OUTPUT);

  if (!BUZZER_IS_ACTIVE) {
    #if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcAttach(BUZZER_PIN, BUZZER_PASSIVE_TONE_HZ, BUZZER_LEDC_RESOLUTION);
    #else
    ledcSetup(BUZZER_LEDC_CHANNEL, BUZZER_PASSIVE_TONE_HZ, BUZZER_LEDC_RESOLUTION);
    ledcAttachPin(BUZZER_PIN, BUZZER_LEDC_CHANNEL);
    #endif
  }

  setBuzzerOutput(false);
}

void setupRgbHardware() {
  pinMode(RGB_RED_PIN, OUTPUT);
  pinMode(RGB_GREEN_PIN, OUTPUT);
  pinMode(RGB_BLUE_PIN, OUTPUT);
  setRgbColor(false, false, false);
}

void updateBuzzer() {
  const unsigned long now = millis();
  const BuzzerPattern pattern = getBuzzerPattern(nearestDistanceCm);

  if (pattern.mode != currentBuzzerMode) {
    currentBuzzerMode = pattern.mode;
    lastBuzzerToggleMs = now;

    if (!pattern.enabled) {
      setBuzzerOutput(false);
      return;
    }

    if (pattern.continuous) {
      setBuzzerOutput(true);
      return;
    }

    setBuzzerOutput(true);
    return;
  }

  if (!pattern.enabled) {
    setBuzzerOutput(false);
    return;
  }

  if (pattern.continuous) {
    setBuzzerOutput(true);
    return;
  }

  const unsigned long interval = buzzerOutputOn ? pattern.onMs : pattern.offMs;
  if (now - lastBuzzerToggleMs >= interval) {
    setBuzzerOutput(!buzzerOutputOn);
    lastBuzzerToggleMs = now;
  }
}

void setBuzzerOutput(bool turnOn) {
  if (BUZZER_IS_ACTIVE) {
    digitalWrite(BUZZER_PIN, (turnOn == BUZZER_ACTIVE_HIGH) ? HIGH : LOW);
  } else {
    #if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcWriteTone(BUZZER_PIN, turnOn ? BUZZER_PASSIVE_TONE_HZ : 0);
    #else
    ledcWriteTone(BUZZER_LEDC_CHANNEL, turnOn ? BUZZER_PASSIVE_TONE_HZ : 0);
    #endif
  }

  buzzerOutputOn = turnOn;
}

void updateRgb() {
  const unsigned long now = millis();

  if (isPriorityAlertActive()) {
    currentRgbMode = RGB_MODE_PRIORITY;
    if (now - lastPriorityToggleMs >= ADXL_PRIORITY_FLASH_MS) {
      lastPriorityToggleMs = now;
      priorityBluePhase = !priorityBluePhase;
    }
    setRgbColor(!priorityBluePhase, false, priorityBluePhase);
    return;
  }

  const RgbMode targetMode = getRgbMode(nearestDistanceCm);

  if (targetMode != currentRgbMode) {
    currentRgbMode = targetMode;
    lastRgbToggleMs = now;
    rgbBlinkOn = true;
  }

  switch (currentRgbMode) {
    case RGB_MODE_GREEN:
      setRgbColor(false, true, false);
      break;

    case RGB_MODE_YELLOW_BLINK:
      if (now - lastRgbToggleMs >= RGB_YELLOW_BLINK_INTERVAL_MS) {
        rgbBlinkOn = !rgbBlinkOn;
        lastRgbToggleMs = now;
      }
      setRgbColor(rgbBlinkOn, rgbBlinkOn, false);
      break;

    case RGB_MODE_YELLOW:
      setRgbColor(true, true, false);
      break;

    case RGB_MODE_RED_BLINK:
      if (now - lastRgbToggleMs >= RGB_RED_BLINK_INTERVAL_MS) {
        rgbBlinkOn = !rgbBlinkOn;
        lastRgbToggleMs = now;
      }
      setRgbColor(rgbBlinkOn, false, false);
      break;

    case RGB_MODE_RED:
      setRgbColor(true, false, false);
      break;

    case RGB_MODE_OFF:
    default:
      setRgbColor(false, false, false);
      break;
  }
}

void setRgbColor(bool redOn, bool greenOn, bool blueOn) {
  digitalWrite(RGB_RED_PIN, redOn ? (RGB_IS_COMMON_ANODE ? LOW : HIGH) : (RGB_IS_COMMON_ANODE ? HIGH : LOW));
  digitalWrite(RGB_GREEN_PIN, greenOn ? (RGB_IS_COMMON_ANODE ? LOW : HIGH) : (RGB_IS_COMMON_ANODE ? HIGH : LOW));
  digitalWrite(RGB_BLUE_PIN, blueOn ? (RGB_IS_COMMON_ANODE ? LOW : HIGH) : (RGB_IS_COMMON_ANODE ? HIGH : LOW));
}

BuzzerPattern getBuzzerPattern(float distanceCm) {
  if (distanceCm < 0.0f || distanceCm > DISTANCE_OFF_CM) {
    return { BUZZER_MODE_OFF, false, false, 0, 0 };
  }

  if (distanceCm > DISTANCE_SLOW_CM) {
    return { BUZZER_MODE_SLOW, true, false, SLOW_BEEP_ON_MS, SLOW_BEEP_OFF_MS };
  }

  if (distanceCm > DISTANCE_MEDIUM_CM) {
    return { BUZZER_MODE_MEDIUM, true, false, MEDIUM_BEEP_ON_MS, MEDIUM_BEEP_OFF_MS };
  }

  if (distanceCm > DISTANCE_FAST_CM) {
    return { BUZZER_MODE_FAST, true, false, FAST_BEEP_ON_MS, FAST_BEEP_OFF_MS };
  }

  return { BUZZER_MODE_CONTINUOUS, true, true, 0, 0 };
}

RgbMode getRgbMode(float distanceCm) {
  if (distanceCm < 0.0f) {
    return RGB_MODE_OFF;
  }

  if (distanceCm > DISTANCE_OFF_CM) {
    return RGB_MODE_GREEN;
  }

  if (distanceCm > DISTANCE_SLOW_CM) {
    return RGB_MODE_YELLOW_BLINK;
  }

  if (distanceCm > DISTANCE_MEDIUM_CM) {
    return RGB_MODE_YELLOW;
  }

  if (distanceCm > DISTANCE_FAST_CM) {
    return RGB_MODE_RED_BLINK;
  }

  return RGB_MODE_RED;
}

const char *buzzerModeText(BuzzerMode mode) {
  switch (mode) {
    case BUZZER_MODE_SLOW:
      return "SLOW";
    case BUZZER_MODE_MEDIUM:
      return "MEDIUM";
    case BUZZER_MODE_FAST:
      return "FAST";
    case BUZZER_MODE_CONTINUOUS:
      return "CONTINUOUS";
    case BUZZER_MODE_OFF:
    default:
      return "OFF";
  }
}

const char *rgbModeText(RgbMode mode) {
  switch (mode) {
    case RGB_MODE_GREEN:
      return "GREEN";
    case RGB_MODE_YELLOW_BLINK:
      return "YELLOW_BLINK";
    case RGB_MODE_YELLOW:
      return "YELLOW";
    case RGB_MODE_RED_BLINK:
      return "RED_BLINK";
    case RGB_MODE_RED:
      return "RED";
    case RGB_MODE_PRIORITY:
      return "PRIORITY";
    case RGB_MODE_OFF:
    default:
      return "OFF";
  }
}

const char *rangeStatusText(uint8_t status) {
  switch (status) {
    case 0:
      return "Range Valid";
    case 1:
      return "Sigma Fail";
    case 2:
      return "Signal Fail";
    case 3:
      return "Min Range Fail";
    case 4:
      return "Out of Range";
    case 5:
      return "Phase Fail";
    case 6:
      return "Hardware Fail";
    case 7:
      return "Wrap Target Fail";
    case 255:
      return "Not Ready";
    default:
      return "Unknown";
  }
}

void printSystemStatus() {
  if (!SERIAL_DEBUG_VERBOSE) {
    String output;
    output.reserve(220);
    output += "S1=";
    output += sensor1Valid ? String(sensor1DistanceCm, 1) : (sensor1Ready ? "N/A" : "NR");
    output += " S2=";
    output += sensor2Valid ? String(sensor2DistanceCm, 1) : (sensor2Ready ? "N/A" : "NR");
    output += " Near=";
    output += nearestDistanceCm >= 0.0f ? String(nearestDistanceCm, 1) : "N/A";
    output += " Buzz=";
    output += buzzerModeText(currentBuzzerMode);
    output += " RGB=";
    output += rgbModeText(currentRgbMode);
    output += " AX=";
    output += adxlReady ? String(adxlFrontAxisG, 3) : "NR";
    output += " dA=";
    output += adxlReady ? String(adxlDeltaG, 3) : "NR";
    output += " Alert=";
    output += isPriorityAlertActive() ? "ON" : "OFF";
    Serial.println(output);
    return;
  }

  String output;
  output.reserve(320);
  output += "Sensor 1: ";
  output += buildDistanceFieldText(sensor1DistanceCm, sensor1RawMm, sensor1Status, sensor1Ready);
  output += "\n";

  output += "Sensor 2: ";
  output += buildDistanceFieldText(sensor2DistanceCm, sensor2RawMm, sensor2Status, sensor2Ready);
  output += "\n";

  output += "Nearest : ";
  if (nearestDistanceCm >= 0.0f) {
    output += String(nearestDistanceCm, 1);
    output += " cm\n";
  } else {
    output += "N/A\n";
  }

  output += "Buzzer  : ";
  output += buzzerModeText(currentBuzzerMode);
  output += "\n";
  output += "RGB     : ";
  output += rgbModeText(currentRgbMode);
  output += "\n";

  output += "ADXL345 : ";
  if (!adxlReady) {
    output += "NOT READY\n";
  } else {
    output += "X=";
    output += String(adxlRawX);
    output += " (";
    output += String(adxlXg, 3);
    output += " g) Y=";
    output += String(adxlRawY);
    output += " (";
    output += String(adxlYg, 3);
    output += " g) Z=";
    output += String(adxlRawZ);
    output += " (";
    output += String(adxlZg, 3);
    output += " g) axis=";
    output += String(adxlFrontAxisG, 3);
    output += " delta=";
    output += String(adxlDeltaG, 3);
    output += " alert=";
    output += isPriorityAlertActive() ? "ON\n" : "OFF\n";
  }

  output += "-----------------------------\n";
  Serial.print(output);
}

void printDistanceField(float distanceCm, uint16_t rawMm, uint8_t status, bool isReady) {
  if (!isReady) {
    Serial.print("NOT READY");
    return;
  }

  if (distanceCm >= 0.0f) {
    Serial.print(distanceCm, 1);
    Serial.print(" cm");
  } else {
    Serial.print("N/A");
  }

  Serial.print(" | raw=");
  Serial.print(rawMm);
  Serial.print(" mm");
  Serial.print(" | status=");
  Serial.print(status);
  Serial.print(" (");
  Serial.print(rangeStatusText(status));
  Serial.print(")");
}

String buildDistanceFieldText(float distanceCm, uint16_t rawMm, uint8_t status, bool isReady) {
  if (!isReady) {
    return "NOT READY";
  }

  String text;
  text.reserve(48);

  if (distanceCm >= 0.0f) {
    text += String(distanceCm, 1);
    text += " cm";
  } else {
    text += "N/A";
  }

  text += " | raw=";
  text += String(rawMm);
  text += " mm | status=";
  text += String(status);
  text += " (";
  text += rangeStatusText(status);
  text += ")";
  return text;
}

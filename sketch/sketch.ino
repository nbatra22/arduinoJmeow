#include <Wire.h>
#include <Arduino_RouterBridge.h>
#include <math.h>

#define IMU_LED_PIN 5
#define TOUCH_LED_PIN 2
#define TOUCH_PIN 13
#define IMU_ADDR 0x69   // ICM20600 IMU address

// MAX31875 config masks
static const uint16_t RES_MASK  = 0x0060;  // bits 6:5
static const uint16_t DF_MASK   = 0x0010;  // bit 4
static const uint16_t SHDN_MASK = 0x0001;  // bit 0

uint8_t foundAddrs[16];
uint8_t foundCount = 0;

// ---------------- I2C helpers ----------------

bool i2cRead16(uint8_t addr, uint8_t reg, uint16_t &val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;

  if (Wire.requestFrom((int)addr, 2) != 2) return false;

  val = ((uint16_t)Wire.read() << 8) | Wire.read();
  return true;
}

bool i2cWrite16(uint8_t addr, uint8_t reg, uint16_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write((uint8_t)(val >> 8));
  Wire.write((uint8_t)(val & 0xFF));
  return (Wire.endTransmission() == 0);
}

// ---------------- MAX31875 ----------------

bool configureMAX31875(uint8_t addr) {
  uint16_t cfgBefore;
  if (!i2cRead16(addr, 0x01, cfgBefore)) return false;

  uint16_t cfg = cfgBefore;
  cfg |= RES_MASK;     // 12-bit resolution
  cfg &= ~DF_MASK;     // normal format
  cfg &= ~SHDN_MASK;   // continuous conversion

  if (!i2cWrite16(addr, 0x01, cfg)) return false;

  uint16_t cfgAfter;
  if (!i2cRead16(addr, 0x01, cfgAfter)) return false;

  return true;
}

bool readTemperatureC(uint8_t addr, float &tempC) {
  Wire.beginTransmission(addr);
  Wire.write(0x00);  // temperature register
  if (Wire.endTransmission(false) != 0) return false;

  if (Wire.requestFrom((int)addr, 2) != 2) return false;

  uint16_t w = ((uint16_t)Wire.read() << 8) | Wire.read();

  int16_t raw = (int16_t)w;
  raw >>= 4;

  tempC = raw * 0.0625f + 10;
  return true;
}

// ---------------- IMU ----------------

bool initIMU() {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  return (Wire.endTransmission() == 0);
}

bool readGyro(int16_t &gx, int16_t &gy, int16_t &gz) {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x43);
  if (Wire.endTransmission(false) != 0) return false;

  if (Wire.requestFrom((int)IMU_ADDR, 6) != 6) return false;
  if (Wire.available() != 6) return false;

  gx = (Wire.read() << 8) | Wire.read();
  gy = (Wire.read() << 8) | Wire.read();
  gz = (Wire.read() << 8) | Wire.read();

  return true;
}

// ---------------- Scan ----------------

void scanI2CBus() {
  foundCount = 0;

  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();

    if (err == 0 && foundCount < 16) {
      foundAddrs[foundCount++] = addr;
    }
  }
}

void configureTempSensorsOnly() {
  for (uint8_t i = 0; i < foundCount; i++) {
    uint8_t addr = foundAddrs[i];

    if (addr == IMU_ADDR || addr == 0x0C) continue;
    configureMAX31875(addr);
  }
}

// ---------------- Setup ----------------

void setup() {
  Bridge.begin();
  Monitor.begin(115200);
  delay(1000);

  Wire.begin();

  pinMode(IMU_LED_PIN, OUTPUT);
  pinMode(TOUCH_LED_PIN, OUTPUT);
  pinMode(TOUCH_PIN, INPUT);

  scanI2CBus();
  initIMU();
  configureTempSensorsOnly();
}

// ---------------- Loop ----------------

void loop() {
  int touchState = digitalRead(TOUCH_PIN);

  if (touchState == HIGH) {
    digitalWrite(TOUCH_LED_PIN, HIGH);
  } else {
    digitalWrite(TOUCH_LED_PIN, LOW);
  }

  int16_t gx = 0, gy = 0, gz = 0;
  float speed = 0.0;
  int brightness = 0;
  bool imuOk = readGyro(gx, gy, gz);

  if (imuOk) {
    speed = sqrt((float)gx * gx + (float)gy * gy + (float)gz * gz);
    brightness = map((int)speed, 0, 20000, 0, 255);
    brightness = constrain(brightness, 0, 255);
    analogWrite(IMU_LED_PIN, brightness);
  }

  // Build temperature array as a JSON fragment
  String tempsJson = "[";
  bool firstTemp = true;

  for (uint8_t i = 0; i < foundCount; i++) {
    uint8_t addr = foundAddrs[i];
    if (addr == IMU_ADDR || addr == 0x0C) continue;

    float tempC;
    if (readTemperatureC(addr, tempC)) {
      if (!firstTemp) tempsJson += ",";
      firstTemp = false;

      tempsJson += "{\"addr\":\"0x";
      if (addr < 0x10) tempsJson += "0";
      tempsJson += String(addr, HEX);
      tempsJson += "\",\"tempC\":";
      tempsJson += String(tempC, 4);
      tempsJson += "}";
    }
  }

  tempsJson += "]";

  String payload = "{";
  payload += "\"touch\":";
  payload += String(touchState);
  payload += ",\"imu_ok\":";
  payload += (imuOk ? "true" : "false");
  payload += ",\"gx\":";
  payload += String(gx);
  payload += ",\"gy\":";
  payload += String(gy);
  payload += ",\"gz\":";
  payload += String(gz);
  payload += ",\"speed\":";
  payload += String(speed, 2);
  payload += ",\"imu_led\":";
  payload += String(brightness);
  payload += ",\"temps\":";
  payload += tempsJson;
  payload += "}";

  Bridge.notify("sensor_packet", payload);

  // keep monitor output minimal
  Monitor.println(payload);

  delay(500);
}
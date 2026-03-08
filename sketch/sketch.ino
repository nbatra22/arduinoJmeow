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

  Monitor.print("Configured MAX31875 at 0x");
  if (addr < 0x10) Monitor.print('0');
  Monitor.print(addr, HEX);
  Monitor.print(" | before=0x");
  Monitor.print(cfgBefore, HEX);
  Monitor.print(" after=0x");
  Monitor.print(cfgAfter, HEX);
  Monitor.print(" | RES=");
  Monitor.print((cfgAfter >> 5) & 0x3, BIN);
  Monitor.print(" DF=");
  Monitor.print((cfgAfter >> 4) & 0x1);
  Monitor.print(" SHDN=");
  Monitor.println(cfgAfter & 0x1);

  delay(100);
  return true;
}

bool readTemperatureC(uint8_t addr, float &tempC) {
  Wire.beginTransmission(addr);
  Wire.write(0x00);  // temperature register
  if (Wire.endTransmission(false) != 0) return false;

  if (Wire.requestFrom((int)addr, 2) != 2) return false;

  uint16_t w = ((uint16_t)Wire.read() << 8) | Wire.read();

  // MAX31875 temp is upper 12 bits, signed
  int16_t raw = (int16_t)w;
  raw >>= 4;

  tempC = raw * 0.0625f + 10;
  return true;
}

// ---------------- IMU ----------------

bool initIMU() {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x6B);   // power management register
  Wire.write(0x00);   // wake device
  return (Wire.endTransmission() == 0);
}

bool readGyro(int16_t &gx, int16_t &gy, int16_t &gz) {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x43);   // gyro register start
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

  Monitor.println();
  Monitor.println("Scanning I2C bus...");

  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();

    if (err == 0) {
      Monitor.print("Found device at 0x");
      if (addr < 0x10) Monitor.print('0');
      Monitor.println(addr, HEX);

      if (foundCount < 16) {
        foundAddrs[foundCount++] = addr;
      }
    }
  }

  Monitor.print("Scan complete. foundCount = ");
  Monitor.println(foundCount);

  if (foundCount == 0) {
    Monitor.println("No I2C devices found.");
  }
}

void configureTempSensorsOnly() {
  for (uint8_t i = 0; i < foundCount; i++) {
    uint8_t addr = foundAddrs[i];

    // Skip the IMU address and skip 0x0C
    if (addr == IMU_ADDR || addr == 0x0C) continue;

    if (!configureMAX31875(addr)) {
      Monitor.print("Warning: MAX31875 config failed at 0x");
      if (addr < 0x10) Monitor.print('0');
      Monitor.println(addr, HEX);
    }
  }
}

// ---------------- Setup ----------------

void setup() {
  Monitor.begin(115200);
  delay(1000);

  Monitor.println();
  Monitor.println("UNO Q | Combined IMU + Temp + Touch Test");

  Wire.begin();

  pinMode(IMU_LED_PIN, OUTPUT);
  pinMode(TOUCH_LED_PIN, OUTPUT);
  pinMode(TOUCH_PIN, INPUT);

  scanI2CBus();

  if (initIMU()) {
    Monitor.println("IMU initialized at 0x69");
  } else {
    Monitor.println("IMU init failed at 0x69");
  }

  configureTempSensorsOnly();

  Monitor.println("Setup complete.");
}

// ---------------- Loop ----------------

void loop() {
  // ----- Touch sensor and touch LED -----
  int touchState = digitalRead(TOUCH_PIN);

  if (touchState == HIGH) {
    digitalWrite(TOUCH_LED_PIN, HIGH);
    Monitor.println("Touch: HIGH");
  } else {
    digitalWrite(TOUCH_LED_PIN, LOW);
    Monitor.println("Touch: LOW");
  }

  // ----- Read IMU gyro and drive IMU LED -----
  int16_t gx, gy, gz;
  if (readGyro(gx, gy, gz)) {
    float speed = sqrt((float)gx * gx + (float)gy * gy + (float)gz * gz);

    int brightness = map((int)speed, 0, 20000, 0, 255);
    brightness = constrain(brightness, 0, 255);
    analogWrite(IMU_LED_PIN, brightness);

    Monitor.print("GX: ");
    Monitor.print(gx);
    Monitor.print("  GY: ");
    Monitor.print(gy);
    Monitor.print("  GZ: ");
    Monitor.print(gz);
    Monitor.print("  | Speed: ");
    Monitor.print(speed);
    Monitor.print("  | IMU LED: ");
    Monitor.println(brightness);
  } else {
    Monitor.println("IMU read failed");
  }

  // ----- Read all temperature sensors except IMU and 0x0C -----
  for (uint8_t i = 0; i < foundCount; i++) {
    uint8_t addr = foundAddrs[i];

    if (addr == IMU_ADDR || addr == 0x0C) continue;

    float tempC;
    if (readTemperatureC(addr, tempC)) {
      Monitor.print("addr 0x");
      if (addr < 0x10) Monitor.print('0');
      Monitor.print(addr, HEX);
      Monitor.print(" | Temperature: ");
      Monitor.print(tempC, 4);
      Monitor.println(" C");
    } else {
      Monitor.print("Read failed at 0x");
      if (addr < 0x10) Monitor.print('0');
      Monitor.println(addr, HEX);
    }
  }

  Monitor.println("------------------------");
  delay(500);
}
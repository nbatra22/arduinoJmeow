#include <Wire.h>
#include <Arduino_RouterBridge.h>

static const uint16_t RES_MASK  = 0x0060;
static const uint16_t DF_MASK   = 0x0010;
static const uint16_t SHDN_MASK = 0x0001;

uint8_t foundAddrs[16];
uint8_t foundCount = 0;

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

bool configureMAX31875(uint8_t addr) {
  uint16_t cfgBefore;
  if (!i2cRead16(addr, 0x01, cfgBefore)) return false;

  uint16_t cfg = cfgBefore;
  cfg |= RES_MASK;
  cfg &= ~DF_MASK;
  cfg &= ~SHDN_MASK;

  if (!i2cWrite16(addr, 0x01, cfg)) return false;

  uint16_t cfgAfter;
  if (!i2cRead16(addr, 0x01, cfgAfter)) return false;

  Monitor.print("Configured 0x");
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

  delay(250);

  return (((cfgAfter >> 5) & 0x3) == 0b11) && (((cfgAfter >> 4) & 0x1) == 0);
}

bool readTemperatureC(uint8_t addr, float &tempC) {
  Wire.beginTransmission(addr);
  Wire.write(0x00);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)addr, 2) != 2) return false;

  uint16_t w = ((uint16_t)Wire.read() << 8) | Wire.read();
  int16_t raw = ((int16_t)w) >> 4;
  tempC = raw * 0.0625f;
  return true;
}

void setup() {
  Monitor.begin(9600);
  Wire.begin();

  Monitor.println();
  Monitor.println("Scanning I2C bus...");

  foundCount = 0;
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Monitor.print("Found device at 0x");
      if (addr < 0x10) Monitor.print('0');
      Monitor.println(addr, HEX);
      if (foundCount < 16) foundAddrs[foundCount++] = addr;
    }
  }

  if (foundCount == 0) {
    Monitor.println("No I2C devices found.");
    return;
  }

  Monitor.println("Configuring found devices as MAX31875 (12-bit, continuous)...");
  for (uint8_t i = 0; i < foundCount; i++) {
    if (!configureMAX31875(foundAddrs[i])) {
      Monitor.print("Warning: could not verify config at 0x");
      if (foundAddrs[i] < 0x10) Monitor.print('0');
      Monitor.println(foundAddrs[i], HEX);
    }
  }

  Monitor.println("Starting temperature readout once per second...");
}

void loop() {
  Monitor.println("hello");  // sanity check
  
  for (uint8_t i = 0; i < foundCount; i++) {
    float tempC;
    if (readTemperatureC(foundAddrs[i], tempC)) {
      Monitor.print("addr 0x");
      if (foundAddrs[i] < 0x10) Monitor.print('0');
      Monitor.print(foundAddrs[i], HEX);
      Monitor.print(" | Temperature: ");
      Monitor.print(tempC, 4);
      Monitor.println(" C");
    } else {
      Monitor.print("Read failed at 0x");
      if (foundAddrs[i] < 0x10) Monitor.print('0');
      Monitor.println(foundAddrs[i], HEX);
    }
  }
  delay(1000);
}
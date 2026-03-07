#include <Wire.h>

// MAX31875 config register bit masks (register 0x01)
// Byte layout: [MSB (bits 15:8)] [LSB (bits 7:0)]
// Bits 6:5 = Resolution (11 = 12-bit)
// Bit  4   = Data Format (0 = normal, 1 = extended — must be 0 for 12-bit >> 4)
// Bit  0   = Shutdown (0 = continuous, 1 = shutdown)
static const uint16_t RES_MASK  = 0x0060; // bits 6:5
static const uint16_t DF_MASK   = 0x0010; // bit 4 — data format
static const uint16_t SHDN_MASK = 0x0001; // bit 0

// Store found device addresses
uint8_t foundAddrs[16];
uint8_t foundCount = 0;

// Read a 16-bit register
bool i2cRead16(uint8_t addr, uint8_t reg, uint16_t &val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)addr, 2) != 2) return false;

  val = ((uint16_t)Wire.read() << 8) | Wire.read();
  return true;
}

// Write a 16-bit register
bool i2cWrite16(uint8_t addr, uint8_t reg, uint16_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write((uint8_t)(val >> 8));   // MSB
  Wire.write((uint8_t)(val & 0xFF)); // LSB
  return (Wire.endTransmission() == 0);
}

// Force device to 12-bit, normal data format, continuous conversion
bool configureMAX31875(uint8_t addr) {
  uint16_t cfgBefore;
  if (!i2cRead16(addr, 0x01, cfgBefore)) {
    return false;
  }

  uint16_t cfg = cfgBefore;

  // Set 12-bit resolution: bits 6:5 = 11
  cfg |= RES_MASK;

  // Normal data format: bit 4 = 0 (critical — extended format shifts data by 1 bit)
  cfg &= ~DF_MASK;

  // Continuous conversion: bit 0 = 0
  cfg &= ~SHDN_MASK;

  if (!i2cWrite16(addr, 0x01, cfg)) {
    return false;
  }

  uint16_t cfgAfter;
  if (!i2cRead16(addr, 0x01, cfgAfter)) {
    return false;
  }

  Serial.print("Configured 0x");
  if (addr < 0x10) Serial.print('0');
  Serial.print(addr, HEX);
  Serial.print(" | before=0x");
  Serial.print(cfgBefore, HEX);
  Serial.print(" after=0x");
  Serial.print(cfgAfter, HEX);
  Serial.print(" | RES=");
  Serial.print((cfgAfter >> 5) & 0x3, BIN);
  Serial.print(" DF=");
  Serial.print((cfgAfter >> 4) & 0x1);
  Serial.print(" SHDN=");
  Serial.println(cfgAfter & 0x1);

  // Wait for first 12-bit conversion (~250 ms worst case)
  delay(250);

  // Verify resolution is 12-bit and data format is normal
  return (((cfgAfter >> 5) & 0x3) == 0b11) && (((cfgAfter >> 4) & 0x1) == 0);
}

// Read temperature register in 12-bit normal format
bool readTemperatureC(uint8_t addr, float &tempC) {
  Wire.beginTransmission(addr);
  Wire.write(0x00); // temperature register
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)addr, 2) != 2) return false;

  uint16_t w = ((uint16_t)Wire.read() << 8) | Wire.read();

  // 12-bit normal format: data is left-justified in bits [15:4]
  // Arithmetic right shift on int16_t sign-extends automatically
  int16_t raw = ((int16_t)w) >> 4;

  tempC = raw * 0.0625f;
  return true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  Wire.begin();

  Serial.println();
  Serial.println("Scanning I2C bus...");

  foundCount = 0;

  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found device at 0x");
      if (addr < 0x10) Serial.print('0');
      Serial.println(addr, HEX);

      if (foundCount < 16) {
        foundAddrs[foundCount++] = addr;
      }
    }
  }

  if (foundCount == 0) {
    Serial.println("No I2C devices found.");
    return;
  }

  Serial.println("Configuring found devices as MAX31875 (12-bit, continuous)...");

  for (uint8_t i = 0; i < foundCount; i++) {
    uint8_t addr = foundAddrs[i];
    if (!configureMAX31875(addr)) {
      Serial.print("Warning: could not verify config at 0x");
      if (addr < 0x10) Serial.print('0');
      Serial.println(addr, HEX);
    }
  }

  Serial.println("Starting temperature readout once per second...");
}

void loop() {
  for (uint8_t i = 0; i < foundCount; i++) {
    uint8_t addr = foundAddrs[i];
    float tempC;

    if (readTemperatureC(addr, tempC)) {
      Serial.print("addr 0x");
      if (addr < 0x10) Serial.print('0');
      Serial.print(addr, HEX);
      Serial.print(" | Temperature: ");
      Serial.print(tempC, 4);
      Serial.println(" C");
    } else {
      Serial.print("Read failed at 0x");
      if (addr < 0x10) Serial.print('0');
      Serial.println(addr, HEX);
    }
  }

  delay(1000);
}

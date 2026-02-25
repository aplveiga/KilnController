// For a connection via I2C using the Arduino Wire include:
#include <Wire.h>               
#include "HT_SSD1306Wire.h"

static SSD1306Wire  display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED); // addr , freq , i2c group , resolution , rst

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();
  VextON();
  delay(100);

  // Initialising the UI will init the display too.
  display.init();
  display.setFont(ArialMT_Plain_10);
  display.size(2);

  // Set the resolution of the analog-to-digital converter (ADC) to 12 bits (0-4095):
  analogReadResolution(12);

  // Set pin 37 as an output pin (used for ADC control):
  pinMode(37, OUTPUT);

  // Set pin 37 to HIGH (enable ADC control):
  digitalWrite(37, HIGH);
}

void loop() {
  // clear the display
  display.clear();

  // draw the current demo method
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(0, 0, String(millis()));


  // Read the raw analog value from pin 1 (range: 0-4095 for 12-bit resolution):
  int analogValue = analogRead(1);

  // Read the analog voltage in millivolts from pin 1:
  int analogVolts = analogReadMilliVolts(1);

  float aValue = analogValue * 490 / 100
  float aVolts = analogVolts * 490 / 100
  
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.printf("ADC analog value = %d\n", aValue);
  display.printf("ADC millivolts value = %d\n", aVolts);
  
  // write the buffer to the display
  display.display();

  // Print the scaled analog value (scaled by a factor of 490/100):
  Serial.printf("ADC analog value = %d\n", aValue);

  // Print the scaled millivolts value (scaled by a factor of 490/100):
  Serial.printf("ADC millivolts value = %d\n", aVolts);

  // Add a delay of 1 second between readings for clear serial output:
  delay(1000);
}
#include <Adafruit_CircuitPlayground.h>
#include <Adafruit_I2CDevice.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>


#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3c ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define EEPROM_ADDRESS 0x50

RTC_DS1307 rtc;     // RTC address 0x68


void setup() {
    CircuitPlayground.begin();
    
    Wire.begin();
    Wire1.begin();
    Serial.begin(9600);
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.println("Boot");

    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("display.begin() failed");
        for(;;);
    }
    display.clearDisplay();
    display.display();

    if (!rtc.begin()) {
        Serial.println("rtc.begin() failed");
        for(;;);
    }
    if (!rtc.isrunning()) {
        Serial.println("RTC is NOT running");
        rtc.adjust(DateTime(2012, 4, 14, 12, 0, 0));
        if (! rtc.isrunning()) {
            Serial.println("RTC is still NOT running despite an attempt to start it");
            for(;;);
        }
    }

    Serial.println("memory dump (NVRAM)");

    for (int address = 0; address < 56; address++) {
        Serial.print(rtc.readnvram(address), HEX);
        Serial.print(" ");
    }
    Serial.println();
    Serial.println("memory dump (EEPROM)");

    for (int address = 0; address < 4096; address++) {
        Serial.print(eeprom_read(address), HEX);
        Serial.print(" ");
    }
    Serial.println();
}
uint8_t eeprom_read(uint16_t address)
{
	uint8_t first,second,data;
	Wire.beginTransmission(EEPROM_ADDRESS);
	
	first = highByte(address);
	second = lowByte(address);
	
	Wire.write(first);      //First Word Address
	Wire.write(second);      //Second Word Address
	
	Wire.endTransmission();
	delay(10);

	Wire.requestFrom(EEPROM_ADDRESS, 1);
	delay(10);
	
	data = Wire.read();
	delay(10);
	
	return data;
}

void eeprom_write(uint16_t address, uint8_t value)
{
	uint8_t first,second;
	Wire.beginTransmission(EEPROM_ADDRESS);

	first = highByte(address);
	second = lowByte(address);

	Wire.write(first);      //First Word Address
	Wire.write(second);      //Second Word Address

	Wire.write(value);     

	delay(10);

	Wire.endTransmission();
	delay(10);
}

// the loop function runs over and over again forever
void loop() {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);

/*
      DateTime now = rtc.now();

    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(" (");
    Serial.print(now.dayOfTheWeek());
    Serial.print(") ");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();
*/
}

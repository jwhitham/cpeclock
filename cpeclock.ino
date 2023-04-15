#include <Adafruit_CircuitPlayground.h>
#include <Adafruit_I2CDevice.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>


#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define BLUE_AREA_Y (SCREEN_HEIGHT / 4)
#define LINE_1_Y ((SCREEN_HEIGHT / 2) + 2)
#define LINE_2_Y (SCREEN_HEIGHT - 5)

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
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("Boot " __DATE__);
    Serial.flush();
    sercom5.initMasterWIRE(100000);

    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("display.begin() failed");
        Serial.flush();
        for(;;);
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setFont(&FreeSans12pt7b);
    display.setCursor(0, LINE_1_Y);
    display.println("Boot");
    display.setCursor(0, LINE_2_Y);
    display.println(__DATE__);
    display.display();

    sercom5.initMasterWIRE(100000);
    if (!rtc.begin()) {
        Serial.println("rtc.begin() failed");
        Serial.flush();
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
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
    display.clearDisplay();
    display.display();
    Serial.println("OK");
    Serial.flush();
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
    char tmp[16];

    // Wait for RTC to advance
    DateTime now = rtc.now();
    uint8_t save = now.second();
    do {
        delay(10);
        now = rtc.now();
    } while (save == now.second());

    digitalWrite(LED_BUILTIN, HIGH);

    // Redraw display
    display.clearDisplay();
    display.setFont(&FreeSans9pt7b);
    display.setCursor(0, BLUE_AREA_Y - 1);
    display.println("..");
    display.setFont(&FreeSans12pt7b);
    snprintf(tmp, sizeof(tmp), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    display.setCursor(0, LINE_1_Y);
    display.println(tmp);
    snprintf(tmp, sizeof(tmp), "%02d/%02d/%04d", now.day(), now.month(), now.year());
    display.setCursor(0, LINE_2_Y);
    display.println(tmp);
    display.display();

    // Flash the light
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);

}

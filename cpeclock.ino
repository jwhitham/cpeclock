#include <Adafruit_CircuitPlayground.h>
#include <Adafruit_I2CDevice.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

#define EEPROM_ADDRESS 0x50

#include "rx433.h"
#include "mail.h"
#include "hal.h"
#include "ncrs.h"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define BLUE_AREA_Y (SCREEN_HEIGHT / 4)
#define LINE_1_Y ((SCREEN_HEIGHT / 2) + 2)
#define LINE_2_Y (SCREEN_HEIGHT - 5)

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3c ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


#define NVRAM_SIZE 56
#define KEY 0x98

#define RX433_PIN   (PIN_A1)
#define INT_PIN     (PIN_A2)

RTC_DS1307 rtc;     // RTC address 0x68


void set_int_pin(char value)
{
    digitalWrite(INT_PIN, value ? HIGH : LOW);
}

void setup()
{
    CircuitPlayground.begin();
    
    Wire.begin();
    Wire1.begin();
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(INT_PIN, OUTPUT);
    pinMode(RX433_PIN, INPUT_PULLUP);
    digitalWrite(LED_BUILTIN, HIGH);
    digitalWrite(INT_PIN, LOW);

    const char* boot = "Boot";
    Serial.begin(9600);
    Serial.println(boot);
    Serial.flush();

    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("display.begin() failed");
        Serial.flush();
        for(;;);
    }
    display.clearDisplay();
    display_message(boot);
    CircuitPlayground.speaker.enable(false);

    if (!rtc.begin()) {
        Serial.println("rtc.begin() failed");
        Serial.flush();
        display_message("RTC ERROR 1");
        for(;;);
    }
    if (!rtc.isrunning()) {
        Serial.println("RTC is NOT running");
        rtc.adjust(DateTime(2012, 4, 14, 12, 0, 0));
        if (! rtc.isrunning()) {
            Serial.println("RTC is still NOT running despite an attempt to start it");
            display_message("RTC ERROR 2");
            for(;;);
        }
    }
    if (!ncrs_init()) {
        Serial.println("ncrs_init() failed");
        display_message("NCRS ERROR");
        for(;;);
    }

    if (!mail_init()) {
        Serial.println("mail_init() failed");
        display_message("MAIL ERROR");
        for(;;);
    }

    attachInterrupt(digitalPinToInterrupt(RX433_PIN), rx433_interrupt, RISING);

    digitalWrite(LED_BUILTIN, LOW);
    CircuitPlayground.strip.setBrightness(255);
    boot = "Booted";
    display_message(boot);
    Serial.println(boot);
    Serial.flush();
}

uint8_t nvram_read(uint8_t addr)
{
    return rtc.readnvram(addr);
}

void nvram_write(uint8_t addr, uint8_t data)
{
    rtc.writenvram(addr, data);
}

void set_clock(uint8_t hour, uint8_t minute, uint8_t second)
{
    rtc.adjust(DateTime(2012, 4, 14, hour, minute, second));
}

void disable_interrupts(void)
{
    noInterrupts();
}

void enable_interrupts(void)
{
    interrupts();
}

void display_message(const char* msg)
{
    display.fillRect(0, 0, SCREEN_WIDTH, BLUE_AREA_Y, 0);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setFont(&FreeSans9pt7b);
    display.setCursor(0, BLUE_AREA_Y - 1);
    display.println(msg);
    display.display();
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


void loop()
{
    char tmp[16];
    uint16_t cap;

    // Wait for RTC to advance
    DateTime now = rtc.now();
    uint8_t save = now.second();
    do {
        now = rtc.now();
        uint8_t expect = (save + 1) % 60;
        if ((now.second() != expect) && (now.second() != save)) {
            break;
        }
        mail_receive_messages();
    } while (save == now.second());

    {
        const int CAP_SAMPLES = 20;   // Number of samples to take for a capacitive touch read.
        uint8_t i, j, red, green, blue;
        j = now.second() % 10;
        CircuitPlayground.strip.setBrightness(255);
        red = CircuitPlayground.slideSwitch() ? 40 : 20;
        green = CircuitPlayground.leftButton() ? 40 : 20;
        blue = CircuitPlayground.rightButton() ? 40 : 20;
        for (i = 0; i <= j; i++) {
            CircuitPlayground.setPixelColor(i, red, green, blue);
        }
        cap = 0;
        if (CircuitPlayground.slideSwitch()) {
            cap = CircuitPlayground.readCap(A3);
        }
        red = green = blue = ((cap >> 4) >= 0xff) ? 0xff : (cap >> 4);
        for (; i <= 9; i++) {
            CircuitPlayground.setPixelColor(i, red, green, blue);
        }
        CircuitPlayground.strip.show();
    }

    // Redraw display
    display.fillRect(0, BLUE_AREA_Y, SCREEN_WIDTH, SCREEN_HEIGHT - BLUE_AREA_Y, 0);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setFont(&FreeSans12pt7b);
    snprintf(tmp, sizeof(tmp), "%02d:%02d:%02d %04x", now.hour(), now.minute(), now.second(), cap);
    display.setCursor(0, LINE_1_Y);
    display.println(tmp);
    display.display();

}

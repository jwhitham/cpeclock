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
static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define NUM_LEDS 10

#define NVRAM_SIZE 56
#define KEY 0x98

#define RX433_PIN   (PIN_A1)
#define INT_PIN     (PIN_A2)

static RTC_DS1307 rtc;     // RTC address 0x68


static const DateTime INVALID_TIME = DateTime(2000, 1, 1);
static const TimeSpan SCREEN_ON_TIME = TimeSpan(20);
static const TimeSpan MESSAGE_ON_TIME = TimeSpan(5);
static const TimeSpan ALARM_ON_TIME = TimeSpan(600);
static const TimeSpan ONE_DAY = TimeSpan(1, 0, 0, 0);

static DateTime alarm_time = INVALID_TIME;
static DateTime now_time = INVALID_TIME;
static DateTime screen_off_time = INVALID_TIME;
static DateTime alarm_screen_off_time = INVALID_TIME;
static DateTime message_off_time = INVALID_TIME;

static char message_buffer[32];
static uint16_t clock_text_x;
static bool allow_sound;

static void display_message_now(const char* msg);

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
    display_message_now(boot);
    CircuitPlayground.speaker.enable(false);

    if (!rtc.begin()) {
        Serial.println("rtc.begin() failed");
        Serial.flush();
        display_message_now("RTC ERROR 1");
        for(;;);
    }
    if (!rtc.isrunning()) {
        Serial.println("RTC is NOT running");
        rtc.adjust(DateTime(2012, 4, 14, 12, 0, 0));
        if (! rtc.isrunning()) {
            Serial.println("RTC is still NOT running despite an attempt to start it");
            display_message_now("RTC ERROR 2");
            for(;;);
        }
    }

    now_time = rtc.now();
    screen_off_time = now_time + SCREEN_ON_TIME;
    message_off_time = INVALID_TIME;
    alarm_screen_off_time = INVALID_TIME;
    alarm_time = INVALID_TIME;
    allow_sound = CircuitPlayground.slideSwitch();

    // determine X location for clock
    {
        int16_t x1, y1;
        uint16_t w, h;
        display.setTextSize(1);
        display.setFont(&FreeSans12pt7b);
        display.getTextBounds("99:99:99", 0, LINE_1_Y, &x1, &y1, &w, &h);
        clock_text_x = (SCREEN_WIDTH - w) / 2;
    }

    if (!ncrs_init()) {
        Serial.println("ncrs_init() failed");
        display_message_now("NCRS ERROR");
        for(;;);
    }

    if (!mail_init()) {
        Serial.println("mail_init() failed");
        display_message_now("MAIL ERROR");
        for(;;);
    }

    attachInterrupt(digitalPinToInterrupt(RX433_PIN), rx433_interrupt, RISING);

    digitalWrite(LED_BUILTIN, LOW);
    CircuitPlayground.strip.setBrightness(0);
    CircuitPlayground.strip.show();
    boot = "Booted";
    display_message_now(boot);
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
    snprintf(message_buffer, sizeof(message_buffer), "%s", msg);
    screen_off_time = now_time + SCREEN_ON_TIME;
    message_off_time = now_time + MESSAGE_ON_TIME;
}

static void display_message_now(const char* msg)
{
    display_message(msg);
    display.clearDisplay();
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

void set_clock(uint8_t hour, uint8_t minute, uint8_t second)
{
    DateTime new_time = DateTime(now_time.year(), now_time.month(), now_time.day(), hour, minute, second);

    while (new_time <= now_time) {
        new_time = new_time + ONE_DAY;
    }
    rtc.adjust(new_time);
    display_message("SET CLOCK");
}

void set_alarm(uint8_t hour, uint8_t minute)
{
    alarm_time = DateTime(now_time.year(), now_time.month(), now_time.day(), hour, minute, 0);
    while (alarm_time <= now_time) {
        alarm_time = alarm_time + ONE_DAY;
    }
    alarm_screen_off_time = now_time + MESSAGE_ON_TIME;
    display_message("SET ALARM");
}

void unset_alarm()
{
    alarm_time = INVALID_TIME;
    alarm_screen_off_time = now_time + MESSAGE_ON_TIME;
    display_message("UNSET ALARM");
}

static void spin_loop(uint32_t wait) {
    uint32_t start = millis();
    while ((millis() - start) <= wait) {
        now_time = rtc.now();
        mail_receive_messages();
        if (CircuitPlayground.leftButton() != CircuitPlayground.rightButton()) {
            // Button press
            if ((alarm_time <= now_time)
            && (now_time <= (alarm_time + ALARM_ON_TIME))) {
                // alarm sounding - stop the alarm
                alarm_time = INVALID_TIME;
            }
            alarm_screen_off_time = now_time + MESSAGE_ON_TIME;
            screen_off_time = now_time + SCREEN_ON_TIME;
        }
        if (allow_sound != CircuitPlayground.slideSwitch()) {
            // Switch moved
            allow_sound = CircuitPlayground.slideSwitch();
            if (allow_sound) {
                display_message("SOUND ON");
            } else {
                display_message("SOUND OFF");
            }
        }
    }
}

void loop()
{
    char tmp[16];
    int alarm_since = -1;
    int i, j;

    now_time = rtc.now();
    
    bool alternator = !(now_time.second() & 1);

    // Check for the alarm
    if ((alarm_time != INVALID_TIME)
    && (alarm_time <= now_time)
    && (now_time <= (alarm_time + ALARM_ON_TIME))) {
        // alarm should sound
        alarm_since = (now_time - alarm_time).totalseconds();
        mail_notify_alarm_sounds();
    }

    // Update the display
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setFont(&FreeSans9pt7b);
    display.setCursor(0, BLUE_AREA_Y - 1);
    if ((now_time <= alarm_screen_off_time) && alternator && (alarm_time != INVALID_TIME)) {
        // message about the alarm
        snprintf(tmp, sizeof(tmp), "ALARM %02d:%02d", alarm_time.hour(), alarm_time.minute());
        display.println(tmp);
    } else if (now_time <= message_off_time) {
        display.println(message_buffer);
    }

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setFont(&FreeSans12pt7b);
    display.setCursor(clock_text_x, LINE_1_Y);
    if ((alarm_since >= 0) && alternator) {
        // alarm is sounding
        snprintf(tmp, sizeof(tmp), "AL %02d:%02d", alarm_time.hour(), alarm_time.minute());
        display.println(tmp);
    } else if (now_time <= screen_off_time) {
        snprintf(tmp, sizeof(tmp), "%02d:%02d:%02d", now_time.hour(), now_time.minute(), now_time.second());
        display.println(tmp);
    }
    display.display();

    // Do stuff in response to the alarm
    if (alarm_since >= 0) {
        int pattern = alarm_since / 30;

        // Lighting effects
        if (pattern == 0) {
            // gradual brightness increase
            CircuitPlayground.strip.setBrightness(((alarm_since + 1) * 255) / 30);
            for (i = 0; i < NUM_LEDS; i++) {
                CircuitPlayground.setPixelColor(i, 255, 255, 200);
            }
            CircuitPlayground.strip.show();
        } else {
            // 1,2,3... Flashing 
            CircuitPlayground.strip.setBrightness(255);
            for (i = 0; i < NUM_LEDS; i++) {
                CircuitPlayground.setPixelColor(i, 255, 255, 200);
            }
            CircuitPlayground.strip.show();
        }

        // Sound effects
        if (pattern == 2) {
            CircuitPlayground.speaker.enable(allow_sound);
            CircuitPlayground.playTone(440, (alarm_since % 30) + 10);
        } else if (pattern >= 2) {
            CircuitPlayground.speaker.enable(allow_sound);
            CircuitPlayground.playTone(440, 40);
        }

        // Further lighting effects
        if (pattern > 0) {
            spin_loop(50);
            CircuitPlayground.strip.setBrightness(0);
            CircuitPlayground.strip.show();
        }

        if ((pattern >= 3) && (pattern <= 4)) {
            // 3,4... Flash red
            spin_loop(450);
            CircuitPlayground.strip.setBrightness(255);
            for (i = 0; i < NUM_LEDS; i++) {
                CircuitPlayground.setPixelColor(i, 255, 0, 0);
            }
            CircuitPlayground.strip.show();
            CircuitPlayground.playTone(440, 40);
            spin_loop(50);
            CircuitPlayground.strip.setBrightness(0);
            CircuitPlayground.strip.show();
        } else if (pattern >= 5) {
            // Flashing red more
            for (j = 0; j < 3; j++) {
                spin_loop(200);
                CircuitPlayground.strip.setBrightness(255);
                for (i = 0; i < NUM_LEDS; i++) {
                    CircuitPlayground.setPixelColor(i, 255, 0, 0);
                }
                CircuitPlayground.strip.show();
                CircuitPlayground.playTone(440, 40);
                spin_loop(50);
                CircuitPlayground.strip.setBrightness(0);
                CircuitPlayground.strip.show();
            }
        }
    } else {
        CircuitPlayground.strip.setBrightness(0);
        CircuitPlayground.strip.show();
    }

    CircuitPlayground.speaker.enable(false);

    // Wait for RTC to advance
    uint8_t save = now_time.second();
    do {
        spin_loop(1);
        now_time = rtc.now();
    } while (save == now_time.second());
}


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

#define CAP_SAMPLES      20   // Number of samples to take for a capacitive touch read.
#define CAP_THRESHOLD 800

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3c ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define NUM_LEDS 10

#define NVRAM_SIZE 56
#define KEY 0x98

#define RX433_PIN   (PIN_A1)
#define INT_PIN     (PIN_A2)
#define EXT_BUTTON_PIN (PIN_A3)

#define PERIOD 10 // milliseconds

static RTC_DS1307 rtc;     // RTC address 0x68


static const DateTime INVALID_TIME = DateTime(2000, 1, 1);
static const TimeSpan SCREEN_ON_TIME = TimeSpan(20);
static const TimeSpan MESSAGE_ON_TIME = TimeSpan(5);
static const TimeSpan ONE_DAY = TimeSpan(1, 0, 0, 0);

static DateTime now_time = INVALID_TIME;
static DateTime screen_off_time = INVALID_TIME;
static DateTime message_off_time = INVALID_TIME;

static char message_buffer[32];
static uint16_t clock_text_x = 0;
static bool allow_sound = false;
static unsigned alarm_active = 0;
static uint32_t millisecond_offset = 0;
static uint32_t last_running_time = 0;
static uint32_t sound_trigger = 0;
static uint32_t strobe_trigger = 0;

void setup()
{
    CircuitPlayground.begin();
    
    Wire.begin();
    Wire1.begin();
    CircuitPlayground.speaker.enable(false);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(INT_PIN, OUTPUT);
    pinMode(RX433_PIN, INPUT_PULLUP);
    pinMode(EXT_BUTTON_PIN, INPUT_PULLUP);
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
    display.setTextWrap(false);
    display_message(boot);

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

    now_time = rtc.now();
    millisecond_offset = millis();
    screen_off_time = now_time + SCREEN_ON_TIME;
    message_off_time = INVALID_TIME;
    allow_sound = CircuitPlayground.slideSwitch();
    alarm_active = false;

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
        display_message("NCRS ERROR");
        for(;;);
    }

    if (!mail_init()) {
        Serial.println("mail_init() failed");
        display_message("MAIL ERROR");
        for(;;);
    }

    alarm_init();

    attachInterrupt(digitalPinToInterrupt(RX433_PIN), rx433_interrupt, RISING);

    digitalWrite(LED_BUILTIN, LOW);
    CircuitPlayground.strip.setBrightness(0);
    CircuitPlayground.strip.show();
    Serial.println("Booted");
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

static void update_display(void)
{
    bool alternator = !(now_time.second() & 1);
    char tmp[16];

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // Update the upper line on the display
    display.setFont(&FreeSans9pt7b);
    display.setCursor(0, BLUE_AREA_Y - 1);
    if (now_time <= message_off_time) {
        display.println(message_buffer);
    }

    // Update the lower line on the display
    display.setFont(&FreeSans12pt7b);
    display.setCursor(clock_text_x, LINE_1_Y);
    if (alarm_active && alternator) {
        // alarm is sounding
        uint8_t hour, minute;
        alarm_get(&hour, &minute);
        snprintf(tmp, sizeof(tmp), "AL %02d:%02d", hour, minute);
        display.println(tmp);
    } else if ((now_time <= screen_off_time) || alarm_active) {
        // show the time
        snprintf(tmp, sizeof(tmp), "%02d:%02d:%02d", now_time.hour(), now_time.minute(), now_time.second());
        display.println(tmp);
    }
    display.display();
}

void display_message(const char* msg)
{
    snprintf(message_buffer, sizeof(message_buffer), "%s", msg);
    screen_off_time = now_time + SCREEN_ON_TIME;
    message_off_time = now_time + MESSAGE_ON_TIME;
    update_display();
}

void clock_set(uint8_t hour, uint8_t minute, uint8_t second)
{
    DateTime new_time = DateTime(now_time.year(), now_time.month(), now_time.day(), hour, minute, second);

    while (new_time <= now_time) {
        new_time = new_time + ONE_DAY;
    }
    rtc.adjust(new_time);
    now_time = new_time;
    display_message("SET CLOCK");
}

static bool try_subtract_strobe_trigger(uint32_t how_often)
{
    if (strobe_trigger >= how_often) {
        strobe_trigger -= how_often;
        return true;
    } else {
        return false;
    }
}

static void update_alarm(void)
{
    // No action if the alarm is disabled, except for switching off NeoPixels
    // and the speaker
    if (!alarm_active) {
        if (0 != CircuitPlayground.strip.getBrightness()) {
            CircuitPlayground.strip.setBrightness(0);
            digitalWrite(INT_PIN, HIGH);
            CircuitPlayground.strip.show();
            digitalWrite(INT_PIN, LOW);
        }
        CircuitPlayground.speaker.enable(false);
        strobe_trigger = sound_trigger = 0;
        return;
    }

    uint32_t i;

    // How many milliseconds has the alarm been running for?
    uint32_t running_time = ((alarm_active - 1) * 60000) + millis() - millisecond_offset;

    // How many milliseconds since the last time update_alarm ran?
    uint32_t millisecond_delta = running_time - last_running_time;
    last_running_time = running_time;

    const uint32_t TOTAL_PHASE_TIME = 30000;
    const uint32_t TOTAL_PULSE_TIME = 2000;
    uint32_t phase = running_time / TOTAL_PHASE_TIME;
    uint32_t phase_time = running_time % TOTAL_PHASE_TIME;
    uint32_t pulse_time = phase_time % TOTAL_PULSE_TIME;
    uint32_t brightness = CircuitPlayground.strip.getBrightness();

    strobe_trigger += millisecond_delta;
    sound_trigger += millisecond_delta;

    // Brightness effects
    switch ((phase <= 4) ? phase : (((phase - 1) % 4) + 1)) {
        case 0:
            // gradual fade in
            if (try_subtract_strobe_trigger(250)) {
                brightness = ((phase_time * 255) / TOTAL_PHASE_TIME);
            }
            break;
        case 1:
            // pulsing
            if (try_subtract_strobe_trigger(250)) {
                if (pulse_time < (TOTAL_PULSE_TIME / 2)) {
                    brightness = (255 - ((pulse_time * 255) / (TOTAL_PULSE_TIME / 2)));
                } else {
                    brightness = (((pulse_time - (TOTAL_PULSE_TIME / 2)) * 255) / (TOTAL_PULSE_TIME / 2));
                }
            }
            break;
        case 2:
            // strobing, once every two seconds
            if (try_subtract_strobe_trigger(2000)) {
                brightness = (255);
            } else {
                brightness = (0);
            }
            break;
        case 3:
            // strobing, twice a second
            if (try_subtract_strobe_trigger(500)) {
                brightness = (255);
            } else {
                brightness = (0);
            }
            break;
        default:
            // strobing, four times a second
            if (try_subtract_strobe_trigger(250)) {
                brightness = (255);
            } else {
                brightness = (0);
            }
            break;
    }

    if (brightness != CircuitPlayground.strip.getBrightness()) {
        // Update required - this will involve disabling interrupts
        // while writing to the NeoPixels, so it will disrupt receiving messages
        CircuitPlayground.strip.setBrightness(brightness);

        // Lighting colour effects
        if (phase <= 4) {
            // yellow
            for (i = 0; i < NUM_LEDS; i++) {
                CircuitPlayground.strip.setPixelColor(i, 255, 255, 0);
            }
        } else if (phase <= 8) {
            // white
            for (i = 0; i < NUM_LEDS; i++) {
                CircuitPlayground.strip.setPixelColor(i, 255, 255, 255);
            }
        } else {
            if (pulse_time < (TOTAL_PULSE_TIME / 2)) {
                // white
                for (i = 0; i < NUM_LEDS; i++) {
                    CircuitPlayground.strip.setPixelColor(i, 255, 255, 255);
                }
            } else {
                // red
                for (i = 0; i < NUM_LEDS; i++) {
                    CircuitPlayground.strip.setPixelColor(i, 255, 0, 0);
                }
            }
        }
        digitalWrite(INT_PIN, HIGH);
        CircuitPlayground.strip.show();
        digitalWrite(INT_PIN, LOW);
    }

    // Sound effects
    uint32_t how_often = 0;
    switch (phase) {
        case 0:
        case 1:
        case 2:
            // no sound
            how_often = 0;
            sound_trigger = 0;
            break;
        case 3:
            how_often = 10000;
            break;
        case 4:
            how_often = 5000;
            break;
        case 5:
            how_often = 3000;
            break;
        case 6:
            how_often = 2000;
            break;
        case 7:
            how_often = 1000;
            break;
        case 8:
            how_often = 500;
            break;
        case 9:
            how_often = 200;
            break;
        default:
            how_often = 100;
            break;
    }
    if (phase >= 3) {
        CircuitPlayground.speaker.enable(allow_sound);
    }
    if ((sound_trigger >= how_often) && (how_often > 0)) {
        CircuitPlayground.playTone(440, 20);
        sound_trigger -= how_often;
    }
}

void loop()
{
    uint8_t previous_second = now_time.second();
    now_time = rtc.now();

    if (now_time.second() != previous_second) {
        // at the start of the minute, update the millisecond offset
        if (now_time.second() == 0) {
            millisecond_offset = millis();
        }
        // Check for the alarm
        alarm_active = alarm_update(now_time.hour(), now_time.minute());
        update_alarm();
        update_display();
    } else {
        // Update the alarm frequently when active
        update_alarm();
    }

    // These tasks run every time loop() is called
    mail_receive_messages();

    if (!digitalRead(EXT_BUTTON_PIN)) {
        // extension button
        screen_off_time = now_time + SCREEN_ON_TIME;
    }
    if (CircuitPlayground.rightButton()) {
        // right button
        if (alarm_reset()) {
            alarm_active = alarm_update(now_time.hour(), now_time.minute());
            update_alarm();
        }
    }
    if (CircuitPlayground.leftButton()) {
        // left button - cancel / disable alarm
        if (alarm_unset()) {
            alarm_active = alarm_update(now_time.hour(), now_time.minute());
            update_alarm();
        }
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

    delay(PERIOD);
}


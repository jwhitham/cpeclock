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
static uint16_t clock_text_x = 0;
static bool allow_sound = false;
static alarm_state_t alarm_state = ALARM_DISABLED;
static uint32_t millisecond_offset = 0;
static uint32_t last_running_time = 0;
static uint32_t sound_trigger = 0;
static uint32_t strobe_trigger = 0;

void set_int_pin(char value)
{
    digitalWrite(INT_PIN, value ? HIGH : LOW);
}

void setup()
{
    CircuitPlayground.begin();
    
    Wire.begin();
    Wire1.begin();
    CircuitPlayground.speaker.enable(false);
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
    if ((now_time <= alarm_screen_off_time) && alternator && (alarm_time != INVALID_TIME)) {
        // message about the alarm (e.g. button pressed)
        snprintf(tmp, sizeof(tmp), "ALARM %02d:%02d", alarm_time.hour(), alarm_time.minute());
        display.println(tmp);
    } else if (now_time <= message_off_time) {
        display.println(message_buffer);
    }

    // Update the lower line on the display
    display.setFont(&FreeSans12pt7b);
    display.setCursor(clock_text_x, LINE_1_Y);
    if ((alarm_state == ALARM_ACTIVE) && alternator) {
        // alarm is sounding
        snprintf(tmp, sizeof(tmp), "AL %02d:%02d", alarm_time.hour(), alarm_time.minute());
        display.println(tmp);
    } else if ((now_time <= screen_off_time) || (alarm_state == ALARM_ACTIVE)) {
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

void set_clock(uint8_t hour, uint8_t minute, uint8_t second)
{
    DateTime new_time = DateTime(now_time.year(), now_time.month(), now_time.day(), hour, minute, second);

    while (new_time <= now_time) {
        new_time = new_time + ONE_DAY;
    }
    rtc.adjust(new_time);
    display_message("SET CLOCK");
}

void alarm_set(uint8_t hour, uint8_t minute, alarm_state_t state)
{
    switch (state) {
        case ALARM_ENABLED:
            // Reached when the alarm should be enabled, as a result of a message received,
            // or when booting up, or due to a button press. Adjust the time so that it's in the future.
            alarm_time = DateTime(now_time.year(), now_time.month(), now_time.day(), hour, minute, 0);
            while (alarm_time <= now_time) {
                alarm_time = alarm_time + ONE_DAY;
            }
            display_message("ALARM SET");
            break;
        case ALARM_ACTIVE:
            // Reached when the alarm should begin to sound, as a result of reaching the alarm time.
            display_message("ALARM!");
            break;
        default:
            // Reached when the alarm should be disabled, as a result of the user pressing a button
            // while the alarm is sounding, or as a result of timing out, or due a message received.
            display_message("ALARM OFF");
            state = ALARM_DISABLED;
            alarm_time = INVALID_TIME;
            break;
    }
    alarm_state = state;
    alarm_screen_off_time = now_time + MESSAGE_ON_TIME;
    mail_set_alarm_state(state);
}

static void update_alarm(void)
{
    uint32_t i;

    // How many milliseconds has the alarm been running for?
    uint32_t running_time = (now_time - alarm_time).totalseconds() * 1000;
    running_time += millis() - millisecond_offset;

    // How many milliseconds since the last time update_alarm ran?
    uint32_t millisecond_delta = running_time - last_running_time;
    last_running_time = running_time;

    const uint32_t TOTAL_PHASE_TIME = 30000;
    const uint32_t TOTAL_PULSE_TIME = 2000;
    uint32_t phase = running_time / TOTAL_PHASE_TIME;
    uint32_t phase_time = running_time % TOTAL_PHASE_TIME;
    uint32_t pulse_time = phase_time % TOTAL_PULSE_TIME;

    strobe_trigger += millisecond_delta;
    sound_trigger += millisecond_delta;

    // Brightness effects
    switch ((phase <= 4) ? phase : (((phase - 1) % 4) + 1)) {
        case 0:
            // gradual fade in
            CircuitPlayground.strip.setBrightness((phase_time * 255) / TOTAL_PHASE_TIME);
            break;
        case 1:
            // pulsing
            if (pulse_time < (TOTAL_PULSE_TIME / 2)) {
                CircuitPlayground.strip.setBrightness(255 - ((pulse_time * 255) / (TOTAL_PULSE_TIME / 2)));
            } else {
                CircuitPlayground.strip.setBrightness(((pulse_time - (TOTAL_PULSE_TIME / 2)) * 255) / (TOTAL_PULSE_TIME / 2));
            }
            strobe_trigger = 0;
            break;
        case 2:
            // strobing, once a second
            if (strobe_trigger >= 1000) {
                CircuitPlayground.strip.setBrightness(255);
                strobe_trigger = 0;
            } else {
                CircuitPlayground.strip.setBrightness(0);
            }
            break;
        case 3:
            // strobing, twice a second
            if (strobe_trigger >= 500) {
                CircuitPlayground.strip.setBrightness(255);
                strobe_trigger = 0;
            } else {
                CircuitPlayground.strip.setBrightness(0);
            }
            break;
        default:
            // strobing, four times a second
            if (strobe_trigger >= 250) {
                CircuitPlayground.strip.setBrightness(255);
                strobe_trigger = 0;
            } else {
                CircuitPlayground.strip.setBrightness(0);
            }
            break;
    }

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
    CircuitPlayground.strip.show();

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
        sound_trigger = 0;
    }
}

void loop()
{
    uint8_t previous_second = now_time.second();
    now_time = rtc.now();

    // These tasks run every second
    if (now_time.second() != previous_second) {
        // Check for the alarm
        switch (alarm_state) {
            case ALARM_ENABLED:
                if ((alarm_time != INVALID_TIME)
                && (alarm_time <= now_time)
                && (now_time <= (alarm_time + ALARM_ON_TIME))) {
                    // alarm should sound
                    alarm_set(0, 0, ALARM_ACTIVE);
                }
                break;
            case ALARM_ACTIVE:
                if ((alarm_time == INVALID_TIME)
                || (now_time > (alarm_time + ALARM_ON_TIME))) {
                    // alarm should not sound
                    alarm_set(0, 0, ALARM_DISABLED);
                }
                break;
            default:
                break;
        }
        millisecond_offset = millis();

        if (alarm_state != ALARM_ACTIVE) {
            CircuitPlayground.strip.setBrightness(0);
            CircuitPlayground.strip.show();
            CircuitPlayground.speaker.enable(false);
        }
        update_display();
    }

    // These tasks run slightly less often than every millisecond
    if (alarm_state == ALARM_ACTIVE) {
        update_alarm();
    }

    // These tasks run every time loop() is called
    mail_receive_messages();

    if (CircuitPlayground.leftButton() && CircuitPlayground.rightButton()) {
        // both buttons pressed
        if (alarm_time == INVALID_TIME) {
            // reload the alarm for the next day, using the same time as before
            mail_reload_alarm(1);
        }
    }
    if (CircuitPlayground.leftButton() || CircuitPlayground.rightButton()) {
        // Either button pressed
        if (alarm_state == ALARM_ACTIVE) {
            // alarm is sounding - stop the alarm
            alarm_set(0, 0, ALARM_DISABLED);
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

    delay(1);
}


#include <Adafruit_CircuitPlayground.h>
#include <Adafruit_I2CDevice.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

#define EEPROM_ADDRESS 0x50


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

RTC_DS1307 rtc;     // RTC address 0x68

extern "C" {
    volatile extern uint32_t rx433_data[];
    volatile extern uint32_t rx433_count;
    extern void rx433_interrupt(void);
}

void setup()
{
    CircuitPlayground.begin();
    
    Wire.begin();
    Wire1.begin();
    Serial.begin(9600);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(RX433_PIN, INPUT_PULLUP);
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("Boot " __DATE__);
    Serial.flush();

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
    attachInterrupt(digitalPinToInterrupt(RX433_PIN), rx433_interrupt, RISING);

    for (uint8_t i = 0; i < NVRAM_SIZE; i++) {
        rtc.writenvram(i, i ^ KEY);
    }
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("OK");
    Serial.flush();
    delay(100);
    display.clearDisplay();
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

static uint16_t errors = 0;
static uint16_t tests = 0;
static uint32_t copy_rx433_data = 0;
static uint32_t copy_rx433_count = 0;
static uint32_t copy_countdown = 0;


void collect_rx433_messages(void)
{
    // Any new messages received?
    noInterrupts();
    if (rx433_count) {
        if (copy_rx433_data == rx433_data[0]) {
            copy_rx433_count ++;
        } else {
            copy_rx433_data = rx433_data[0];
            copy_rx433_count = 1;
        }
        rx433_count = 0;
        copy_countdown = 3;
    }
    interrupts();
}

extern "C" {
#include "sha256.h"
}

static void sha_test(char* tmp, size_t size)
{
    SHA256_CTX ctx;
    BYTE hash[32];
    size_t i;
    uint32_t test = 0x41424344;
    // in memory: 0x44 has the lowest address i.e. little endian

    i = 0;
    memset(hash, 0, sizeof(hash));
    memset(tmp, 0, size);
    memcpy(hash, &test, 4);
    /*sha256_init(&ctx);
    sha256_update(&ctx, (const BYTE *) tmp, 4);
    sha256_final(&ctx, hash); */
    while((size >= 3) && (i < sizeof(hash)) && (i <= 5)) {
        snprintf(tmp, 3, "%02x", hash[i]);
        i++;
        size -= 2;
        tmp += 2;
    }
}




void loop()
{
    char tmp[16];

    // Wait for RTC to advance
    DateTime now = rtc.now();
    uint8_t save = now.second();
    uint16_t cycles = 0;
    do {
        now = rtc.now();
        uint8_t expect = (save + 1) % 60;
        uint8_t i = tests % NVRAM_SIZE;
        if (((now.second() != expect) && (now.second() != save))
        || (now.year() != 2012)
        || (now.month() != 4)
        || ((rtc.readnvram(i) ^ KEY) != i)) {
            errors ++;
            break;
        }
        collect_rx433_messages();
        tests ++;
        cycles ++;
    } while (save == now.second());

    digitalWrite(LED_BUILTIN, HIGH);

    if (copy_countdown) {
        copy_countdown --;
    }

    // Redraw display
    display.clearDisplay();
    display.setFont(&FreeSans9pt7b);
    display.setCursor(0, BLUE_AREA_Y - 1);
    if (copy_countdown) {
        snprintf(tmp, sizeof(tmp), "%08x %u", (unsigned) copy_rx433_data, (unsigned) copy_rx433_count);
        display.println(tmp);
    } else {
        sha_test(tmp, sizeof(tmp));
        display.println(tmp);
        copy_rx433_count = 0;
    }
    display.setFont(&FreeSans12pt7b);
    snprintf(tmp, sizeof(tmp), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    display.setCursor(0, LINE_1_Y);
    display.println(tmp);
    snprintf(tmp, sizeof(tmp), "%04x %04x", errors, tests);
    display.setCursor(0, LINE_2_Y);
    display.println(tmp);
    display.display();

    // Flash the light
    digitalWrite(LED_BUILTIN, LOW);

}

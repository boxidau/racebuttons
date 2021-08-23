#define USE_SX1278_RADIO 0
#define USE_E32_RADIO 1

#include <constants.h>
#include <structures.h>

#include <Arduino.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>
#include <cryptoradio.h>
#include <buzzer.h>

#if USE_SX1278_RADIO
#include <SX1278Radio.h>
SX1278Radio radio_module;
#elif USE_E32_RADIO
#include <E32Radio.h>
E32Radio radio_module;
#else
#include <DummyRadio.h>
DummyRadio radio_module;
#endif

// LED Strip Setup
Adafruit_NeoPixel strip = Adafruit_NeoPixel(16, PIN_LED_STRIP, NEO_GRB + NEO_KHZ800);

const uint32_t kDimPurple = strip.Color(20, 0, 20);
const uint32_t kDimGreen = strip.Color(0, 20, 0);
const uint32_t kWhite = strip.Color(255, 255, 255);
const uint32_t kRed = strip.Color(255, 0, 0);
const uint32_t kBlue = strip.Color(0, 0, 255);
const uint32_t kYellow = strip.Color(255, 200, 0);
const uint32_t kDimOrange = strip.Color(20, 9, 0);

RaceButton buttons[BUTTON_COUNT] = {
    RaceButton{PIN_BLACK, 0, 8, kWhite},
    RaceButton{PIN_RED, 2, 10, kRed},
    RaceButton{PIN_BLUE, 5, 13, kBlue},
    RaceButton{PIN_YELLOW, 7, 15, kYellow},
};

const int eeprom_local_val_idx = 64;
const int eeprom_pairing_info_idx = 8;
const int eeprom_brightness_val_idx = 120;

elapsedMillis broadcast_timer;
elapsedMillis dead_comms_timer;
elapsedMillis reset_timer;
elapsedMillis brightness_timer;

elapsedMillis tx_led;
elapsedMillis rx_led;

elapsedMillis respond_timer;
bool should_respond = false;
bool brightness = true;

bool base_station = false;

PairingInfo pairing_info;
CryptoRadio cr = CryptoRadio(&radio_module);

Buzzer buzzer = Buzzer(BUZZER_PIN);

void broadcastStatus()
{
    Packet p = {pairing_info.remote_address, 0, PACKET_TYPE_STATE};
    p.data[0] = packBitmap(buttons, BUTTON_COUNT);
    tx_led = 0;
    if (!cr.sendPacket(&p))
    {
        Serial.println("[ERROR] sending packet");
    }
}

void readPairingInfo()
{
    EEPROM.begin();
    byte pairing_info_raw[sizeof(PairingInfo)];
    for (int i = 0; i < sizeof(PairingInfo); i++)
    {
        noInterrupts();
        auto res = EEPROM.read(eeprom_pairing_info_idx + i);
        interrupts();
        pairing_info_raw[i] = res;
        Serial.print("EEPROM reading from offset: ");
        Serial.print(eeprom_pairing_info_idx + i);
        Serial.print(" val: ");
        Serial.println(res, HEX);
    }
    memcpy(&pairing_info, pairing_info_raw, sizeof(PairingInfo));
}

void writePairingInfo()
{
    byte pairing_info_raw[sizeof(PairingInfo)];
    memcpy(pairing_info_raw, &pairing_info, sizeof(PairingInfo));
    EEPROM.begin();
    for (int i = 0; i < sizeof(PairingInfo); i++)
    {
        Serial.print("EEPROM Writing to offset: ");
        Serial.print(eeprom_pairing_info_idx + i);
        Serial.print(" val: ");
        Serial.println(pairing_info_raw[i], HEX);
        noInterrupts();
        EEPROM.write(eeprom_pairing_info_idx + i, pairing_info_raw[i]);
        interrupts();
    }
}

// Setup function
void setup()
{
    Serial.begin(115200);
    EEPROM.begin();

    pinMode(HEALTH_LED, OUTPUT);
    digitalWrite(HEALTH_LED, HIGH);

    for (uint8_t i = 0; i < BUTTON_COUNT; i++)
    {
        pinMode(buttons[i].pin, INPUT_PULLUP);
        buttons[i].bounce.interval(DEBOUNCE_TIME);
        buttons[i].bounce.attach(buttons[i].pin);
    }

    strip.begin();
    brightness = EEPROM.read(eeprom_brightness_val_idx);
    strip.setBrightness(brightness ? STRIP_BRIGHTNESS : STRIP_BRIGHTNESS_DIM);
    strip.show(); // Initialize all pixels to 'off'

    // allow for yellow button to pause setup()
    while (!digitalRead(PIN_YELLOW))
    {
    }
    Serial.println("initializing...");

    readPairingInfo();
    cr.setEncryptionKey(pairing_info.shared_key);
    cr.begin();

    if (!digitalRead(PIN_BLACK))
    {
        // enter pairing mode
        strip.fill(strip.Color(0, 40, 40));
        strip.show();
        Serial.println("Entering pairing mode...");

        if (cr.pairingMode(&pairing_info))
        {
            strip.fill(strip.Color(0, 40, 0));
            writePairingInfo();
        }
        else
        {
            strip.fill(strip.Color(40, 0, 0));
        }
        strip.show();
        delay(1000);
        strip.clear();
    }

    uint16_t local_address = cr.getAddress();
    Serial.print("Pairing info - remote addr: ");
    Serial.print(pairing_info.remote_address, HEX);
    Serial.print(" local address: ");
    Serial.print(local_address, HEX);
    Serial.print(" shared key: ");
    for (int i = 0; i < 32; i++)
    {
        Serial.print(" 0x");
        Serial.print(pairing_info.shared_key[i], HEX);
    }
    Serial.println("");

    if (pairing_info.remote_address > local_address)
    {
        Serial.println("This is the base station!");
        base_station = true;
    }
    else
    {
        Serial.println("This is a remote node!");
    }

    // load from local storage
    unpackBitmap(EEPROM.read(eeprom_local_val_idx), LOCAL_STATE, buttons, BUTTON_COUNT);
    buzzer.beep(100);
}

void detectRebootRequest()
{
    if (!digitalRead(buttons[2].pin) && !digitalRead(buttons[3].pin))
    {
        if (reset_timer > 2000)
        {
            _reboot_Teensyduino_();
        }
        strip.fill(strip.Color(40, 40, 0));
    }
    else
    {
        reset_timer = 0;
    }
}

void detectBrightnessRequest()
{
    if (!digitalRead(buttons[3].pin))
    {
        if (brightness_timer > 2000)
        {
            brightness = !brightness;
            brightness_timer = 0;
            EEPROM.update(eeprom_brightness_val_idx, brightness);
        }
    }
    else
    {
        brightness_timer = 0;
    }
}

void showCommsStatus()
{
    if (dead_comms_timer > DEAD_COMMS_INTERVAL)
    {
        strip.setPixelColor(3, kDimPurple);
        strip.setPixelColor(4, kDimPurple);
        if (base_station)
        {
            strip.setPixelColor(11, kDimPurple);
            strip.setPixelColor(12, kDimPurple);
        }
    }
    else
    {
        strip.setPixelColor(3, kDimGreen);
        strip.setPixelColor(4, kDimGreen);
        if (base_station)
        {
            strip.setPixelColor(11, kDimGreen);
            strip.setPixelColor(12, kDimGreen);
        }
    }
}

void showRXTXIndicators()
{
    if (!base_station)
        return;
    if (tx_led < 100)
    {
        strip.setPixelColor(6, kDimOrange);
    }
    if (rx_led < 100)
    {
        strip.setPixelColor(14, kDimOrange);
    }
}

void loop()
{
    buzzer.update();
    strip.clear();
    for (uint8_t i = 0; i < BUTTON_COUNT; i++)
    {
        if (buttons[i].bounce.update() && buttons[i].bounce.fell())
        {
            buttons[i].local_status = !buttons[i].local_status;
        }

        strip.setPixelColor(
            buttons[i].local_led_index,
            buttons[i].local_status ? buttons[i].color : 0);

        strip.setPixelColor(
            buttons[i].remote_led_index,
            buttons[i].remote_status ? buttons[i].color : 0);
    }
    showCommsStatus();
    showRXTXIndicators();
    detectRebootRequest();
    detectBrightnessRequest();
    strip.setBrightness(brightness ? STRIP_BRIGHTNESS : STRIP_BRIGHTNESS_DIM);
    strip.show();

    if (broadcast_timer > BROADCAST_INTERVAL)
    {
        digitalWrite(HEALTH_LED, !digitalRead(HEALTH_LED));
        broadcast_timer = 0;
        if (base_station)
        {
            broadcastStatus();
        }
        uint8_t cur_val = packBitmap(buttons, BUTTON_COUNT);
        EEPROM.update(eeprom_local_val_idx, cur_val);
    }

    if (should_respond && respond_timer > 10)
    {
        should_respond = false;
        broadcastStatus();
    }

    static Packet packet;
    if (cr.receivePacket(&packet))
    {
        rx_led = 0;
        switch (packet.packet_type)
        {
        case PACKET_TYPE_STATE:
            unpackBitmap(packet.data[0], REMOTE_STATE, buttons, BUTTON_COUNT);
            if (dead_comms_timer > DEAD_COMMS_INTERVAL) {
                buzzer.beep(500);
            }
            dead_comms_timer = 0;
            if (!base_station)
            {
                should_respond = true;
                respond_timer = 0;
            }
            break;
        default:
            Serial.println("Received packet type without handler");
        }
    }
}

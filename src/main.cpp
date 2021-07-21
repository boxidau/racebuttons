#define BUTTON_COUNT 4
#define BROADCAST_INTERVAL 500
#define DEAD_COMMS_INTERVAL 30000
#define DEBOUNCE_TIME 50

#define RESET_OKAY_BUTTON 0

#define PIN_LED_STRIP 4 // D2
#define STRIP_BRIGHTNESS 255

#define PACKET_LENGTH 4

// SPI Pins
#define HSCLK 14   // D5
#define HMISO 12   // D6
#define HMOSI 13   // D7
#define SX_NSS 15  // D8
#define SX_RST 16  // D0
#define SX_DIO0 10 // SD3

// Button Pins
#define PIN_D1 5
#define PIN_D3 0
#define PIN_D4 2
#define PIN_SD2 9

#include <Arduino.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>
#include <RadioLib.h>

// IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
// pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
// and minimize distance between Arduino and first pixel.  Avoid connecting
// on a live circuit...if you must, connect GND first.

// LED Strip Setup
Adafruit_NeoPixel strip = Adafruit_NeoPixel(16, PIN_LED_STRIP, NEO_GRB + NEO_KHZ800);

// Button Setup
struct Button
{
    int pin;

    int local_led_index;
    int remote_led_index;
    uint32_t color;
    bool local_status;
    bool remote_status;

    bool _pressed;
    unsigned long _debounce_timer;
};

Button buttons[BUTTON_COUNT] = {
    Button{PIN_D1, 0, 8, strip.Color(150, 150, 150)},
    Button{A0, 2, 10, strip.Color(150, 0, 0)},
    Button{PIN_D4, 5, 13, strip.Color(0, 0, 150)},
    Button{PIN_D3, 7, 15, strip.Color(190, 150, 0)},
};

const int eeprom_base_station_idx = 0;
const int eeprom_local_val_idx = 8;

// Poll Timer
unsigned long broadcast_timer = 0;

// Dead comms timer
unsigned long dead_comms_timer = 0;

enum StateType
{
    LOCAL_STATE,
    REMOTE_STATE
};

SX1278 radio = new Module(SX_NSS, SX_DIO0, SX_RST);

bool base_station = false;

// flag to indicate that a packet was received
volatile bool receivedFlag = false;
// disable interrupt when it's not needed
volatile bool enableInterrupt = true;

uint8_t packBitmap()
{
    uint8_t bitmap = 0;
    for (uint8_t i = 0; i < BUTTON_COUNT; i++)
    {
        bitmap |= (buttons[i].local_status << i);
    }
    return bitmap;
}

void unpackBitmap(uint8_t bitmap, StateType state_type)
{
    for (uint8_t i = 0; i < BUTTON_COUNT; i++)
    {
        if (state_type == REMOTE_STATE) {
            buttons[i].remote_status = bitmap & (1 << i);
        } else {
            buttons[i].local_status = bitmap & (1 << i);
        }
    }
}

void radioListen()
{
    int state = radio.startReceive();
    if (state != ERR_NONE)
    {
        Serial.print(F("[SX1278] listen mode failed, code: "));
        Serial.println(state);
    }
}

void broadcastStatus()
{
    byte data[PACKET_LENGTH] = {packBitmap(), 0, 0, 0};

    Serial.print("[SX1278] TX Data:");
    for (int i = 0; i < PACKET_LENGTH; i++)
    {
        Serial.print(" 0x");
        Serial.print(data[i], HEX);
    }
    Serial.println("");

    enableInterrupt = false;
    int state = radio.transmit(data, PACKET_LENGTH);
    enableInterrupt = true;
    radioListen();

    if (state == ERR_TX_TIMEOUT)
    {
        // timeout occurred while transmitting packet
        Serial.println(F("TX timeout!"));
    }
    else if (state != ERR_NONE)
    {
        // some other error occurred
        Serial.print(F("TX failed, code "));
        Serial.println(state);
    }
}

bool receivePacket(byte *packet)
{
    bool receivedPacket = false;
    // check if the flag is set
    if (receivedFlag)
    {
        // disable the interrupt service routine while
        // processing the data
        enableInterrupt = false;

        // reset flag
        receivedFlag = false;

        // you can read received data as an Arduino String
        int state = radio.readData(packet, PACKET_LENGTH);

        if (state == ERR_NONE)
        {
            receivedPacket = true;
            // print data of the packet
            Serial.print(F("[SX1278] RX Data: "));
            for (int i = 0; i < PACKET_LENGTH; i++)
            {
                Serial.print(" 0x");
                Serial.print(packet[i], HEX);
            }
            Serial.println("");

            // print RSSI (Received Signal Strength Indicator)
            Serial.print(F("[SX1278] RSSI: "));
            Serial.print(radio.getRSSI());
            Serial.print(F(" dBm, "));

            // print SNR (Signal-to-Noise Ratio)
            Serial.print(F("SNR: "));
            Serial.print(radio.getSNR());
            Serial.print(F(" dB, "));

            // print frequency error
            Serial.print(F("Frequency error: "));
            Serial.print(radio.getFrequencyError());
            Serial.println(F(" Hz"));
        }
        else if (state == ERR_CRC_MISMATCH)
        {
            // packet was received, but is malformed
            Serial.println(F("[SX1278] CRC error!"));
        }
        else
        {
            // some other error occurred
            Serial.print(F("[SX1278] Failed, code "));
            Serial.println(state);
        }

        if (!base_station)
        {
            broadcastStatus();
        }

        // put module back to listen mode
        radio.startReceive();

        // we're ready to receive more packets,
        // enable interrupt service routine
        enableInterrupt = true;
    }
    return receivedPacket;
}

IRAM_ATTR void setFlag(void)
{
    // check if the interrupt is enabled
    if (!enableInterrupt)
    {
        return;
    }
    // we got a packet, set the flag
    receivedFlag = true;
}

// Setup function
void setup()
{
    strip.begin();
    strip.setBrightness(STRIP_BRIGHTNESS);
    strip.show(); // Initialize all pixels to 'off'
    
    Serial.begin(115200);
    while (!Serial)
    {
        delay(200);
    }
    
    for (uint8_t i = 0; i < BUTTON_COUNT; i++)
    {
        pinMode(buttons[i].pin, INPUT_PULLUP);
    }

    EEPROM.begin(512);
    base_station = EEPROM.read(eeprom_base_station_idx) > 0;
    if (!digitalRead(PIN_D1))
    {
        base_station = true;
        EEPROM.write(eeprom_base_station_idx, 1);
        Serial.println("Persisting mode: base station");
        EEPROM.commit();

    }
    else if (analogRead(A0) < 100)
    {
        base_station = false;
        EEPROM.write(eeprom_base_station_idx, 0);
        Serial.println("Persisting mode: node");
        EEPROM.commit();
    }

    if (base_station) {
        Serial.println("This is the base station!");
    } else {
        Serial.println("This is a remote node!");
    }

    // load from local storage
    unpackBitmap(EEPROM.read(eeprom_local_val_idx), LOCAL_STATE);

    Serial.print(F("[SX1278] Initializing ... "));
    int state = radio.begin(434.0, 125.0, 9, 7, SX127X_SYNC_WORD, 10, 8, 0);
    if (state == ERR_NONE)
    {
        Serial.println(F("success!"));
    }
    else
    {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true)
            ;
    }

    radio.setDio0Action(setFlag);
    radioListen();
    
}

// Loop function
void loop()
{
    for (uint8_t i = 0; i < BUTTON_COUNT; i++)
    {
        int pulled_low = !digitalRead(buttons[i].pin);
        if (buttons[i].pin == A0) {
            pulled_low = analogRead(A0) < 100;
        }
        auto press_time = millis();
        if (press_time > (buttons[i]._debounce_timer + DEBOUNCE_TIME) && buttons[i]._pressed != pulled_low)
        {
            buttons[i]._debounce_timer = press_time;
            buttons[i]._pressed = pulled_low;
            if (pulled_low)
            {
                buttons[i].local_status = !buttons[i].local_status;
            }
        }

        strip.setPixelColor(
            buttons[i].local_led_index,
            buttons[i].local_status ? buttons[i].color : 0);

        strip.setPixelColor(
            buttons[i].remote_led_index,
            buttons[i].remote_status ? buttons[i].color : 0);
    }
    if (base_station) {
        if (millis() > (dead_comms_timer + DEAD_COMMS_INTERVAL)) {
            strip.setPixelColor(3, strip.Color(20, 0, 20));
            strip.setPixelColor(4, strip.Color(20, 0, 20));
            strip.setPixelColor(11, strip.Color(20, 0, 20));
            strip.setPixelColor(12, strip.Color(20, 0, 20));
        } else {
            strip.setPixelColor(3, strip.Color(0, 20, 0));
            strip.setPixelColor(4, strip.Color(0, 20, 0));
            strip.setPixelColor(11, strip.Color(0, 20, 0));
            strip.setPixelColor(12, strip.Color(0, 20, 0));
        }
    }

    strip.show();

    if (millis() > broadcast_timer + BROADCAST_INTERVAL)
    {
        broadcast_timer = millis();
        if (base_station)
        {
            broadcastStatus();
        }
        uint8_t cur_val = packBitmap();
        if (EEPROM.read(eeprom_local_val_idx) != cur_val) {
            EEPROM.write(eeprom_local_val_idx, cur_val);
            EEPROM.commit();
        }
    }


    static byte packet[PACKET_LENGTH];
    if (receivePacket(packet)) {
        unpackBitmap(packet[0], REMOTE_STATE);
        Serial.println("RX packet");
        dead_comms_timer = millis();
    }
}

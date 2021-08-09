#include <constants.h>
#include <structures.h>

#include <Arduino.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>
#include <cryptoradio.h>

// LED Strip Setup
Adafruit_NeoPixel strip = Adafruit_NeoPixel(16, PIN_LED_STRIP, NEO_GRB + NEO_KHZ800);


Button buttons[BUTTON_COUNT] = {
    Button{PIN_D1, 0, 8, strip.Color(150, 150, 150)},
    Button{A0, 2, 10, strip.Color(150, 0, 0)},
    Button{PIN_D4, 5, 13, strip.Color(0, 0, 150)},
    Button{PIN_D3, 7, 15, strip.Color(190, 150, 0)},
};

const int eeprom_local_val_idx = 0;
const int eeprom_pairing_info_idx = 8;

// Poll Timer
unsigned long broadcast_timer = 0;

// Dead comms timer
unsigned long dead_comms_timer = 0;

bool base_station = false;

PairingInfo pairing_info;
CryptoRadio cr;

void broadcastStatus()
{
    Packet p = {pairing_info.remote_address, 0, PACKET_TYPE_STATE};
    p.data[0] = packBitmap(buttons, BUTTON_COUNT);
    cr.sendPacket(&p);
}

void readPairingInfo()
{
    byte pairing_info_raw[sizeof(PairingInfo)];
    for (int i = 0; i < sizeof(PairingInfo); i++) {
        pairing_info_raw[i] = EEPROM.read(eeprom_pairing_info_idx + i);
    }
    memcpy(&pairing_info, pairing_info_raw, sizeof(PairingInfo));
}

void writePairingInfo()
{
    for (int i = 0; i < sizeof(PairingInfo); i++) {
        EEPROM.write(eeprom_pairing_info_idx + i, ((byte*)&pairing_info)[i]);
    }
    EEPROM.commit();
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
    readPairingInfo();

    Serial.print(F("[SX1278] Initializing ... "));
    cr.begin(pairing_info.shared_key);

    if (!digitalRead(PIN_D1))
    {
        // enter pairing mode
        strip.fill(strip.Color(0, 40, 40));
        strip.show();
        Serial.println("Entering pairing mode...");
        
        if (cr.pairingMode(&pairing_info)) {
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
    
    Serial.print("Pairing info - remote addr: ");
    Serial.print(pairing_info.remote_address);
    Serial.print(" shared key: ");
    for (int i = 0; i < 32; i++)
    {
        Serial.print(" 0x");
        Serial.print(pairing_info.shared_key[i], HEX);
    }
    Serial.println("");

    if (pairing_info.base_station) {
        Serial.println("This is the base station!");
    } else {
        Serial.println("This is a remote node!");
    }

    // load from local storage
    unpackBitmap(EEPROM.read(eeprom_local_val_idx), LOCAL_STATE, buttons, BUTTON_COUNT);

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

    strip.show();

    if (millis() > broadcast_timer + BROADCAST_INTERVAL)
    {
        broadcast_timer = millis();
        if (pairing_info.base_station)
        {
            broadcastStatus();
        }
        uint8_t cur_val = packBitmap(buttons, BUTTON_COUNT);
        if (EEPROM.read(eeprom_local_val_idx) != cur_val) {
            EEPROM.write(eeprom_local_val_idx, cur_val);
            EEPROM.commit();
        }
    }


    static Packet packet;
    if (cr.receivePacket(&packet) && packet.packet_type == PACKET_TYPE_STATE) {
        unpackBitmap(packet.data[0], REMOTE_STATE, buttons, BUTTON_COUNT);
        dead_comms_timer = millis();        
        if (!pairing_info.base_station)
        {
            broadcastStatus();
        }
    }
}

#pragma once

#include <AbstractRadio.h>
#include <LoRa_E32.h>

// E32 Pins
#define RADIO_AUX 12
#define RADIO_M0 11
#define RADIO_M1 10
#define RADIO_SERIAL HardwareSerial3

class E32Radio : public AbstractRadio {
    public:
        void begin();
        void listen();
        bool receive(byte* data, uint8_t size);
        bool send(byte* data, uint8_t size);
    private:
        static LoRa_E32 radio;
};
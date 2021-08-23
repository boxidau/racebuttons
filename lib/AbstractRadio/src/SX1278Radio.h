#pragma once
#include <AbstractRadio.h>
#include <RadioLib.h>

// SX1278 Pins
#define SX_NSS 15  // D8
#define SX_RST 16  // D0
#define SX_DIO0 10 // SD3

class SX1278Radio : public AbstractRadio {
    public:
        void begin();
        void listen();
        bool receive(byte* data, uint8_t size);
        bool send(byte* data, uint8_t size);
    private:
        static SX1278 radio;
};
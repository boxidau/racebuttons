#pragma once
#include <Arduino.h>

#define RADIO_BROADCAST_ADDRESS 0xFFFF

class AbstractRadio {
    public:
        virtual void begin();
        virtual void listen();
        virtual bool receive(byte* data, uint8_t size);
        virtual bool send(byte* data, uint8_t size);
};
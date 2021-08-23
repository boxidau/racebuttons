#pragma once
#include <AbstractRadio.h>

class DummyRadio : public AbstractRadio {
    public:
        void begin();
        void listen();
        bool receive(byte* data, uint8_t size);
        bool send(byte* data, uint8_t size);
};
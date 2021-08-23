#include <DummyRadio.h>

void DummyRadio::begin() {}

void DummyRadio::listen() {}

bool DummyRadio::receive(byte *data, uint8_t size)
{
    return false;
}

bool DummyRadio::send(byte *data, uint8_t size)
{
    return true;
}
#pragma once
#include <Arduino.h>
#include <Bounce2.h>
#include <constants.h>

// Button Setup
struct RaceButton
{
    int pin;
    int local_led_index;
    int remote_led_index;
    uint32_t color;
    Bounce bounce;
    bool local_status;
    bool remote_status;
};

enum StateType
{
    LOCAL_STATE,
    REMOTE_STATE
};

enum PacketType : uint8_t
{
    PACKET_TYPE_PAIRING_REQUEST = 0x81,
    PACKET_TYPE_PAIRING_RESPONSE = 0x82,
    PACKET_TYPE_PAIRING_TEST = 0x03,
    PACKET_TYPE_STATE = 0x04
};

#pragma pack(push, 1)
struct Packet
{
    uint16_t destination;
    uint16_t source;
    PacketType packet_type;
    byte data[MAX_PACKET_DATA_LENGTH];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct PairingInfo
{
    uint16_t remote_address;
    uint8_t shared_key[32];
};
#pragma pack(pop)

uint8_t packBitmap(RaceButton *buttons, uint8_t len);
void unpackBitmap(uint8_t bitmap, StateType state_type, RaceButton *buttons, uint8_t len);

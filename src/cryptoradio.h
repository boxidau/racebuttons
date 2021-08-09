#pragma once
#include <Crypto.h>
#include <AES.h>
#include <Curve25519.h>
#include <RNG.h>
#include <string.h>
#include <Arduino.h>
#include <RadioLib.h>
#include <structures.h>
#include <FastCRC.h>

void pairingMode();
void testDH();

class CryptoRadio {
    public:
        static void begin(byte _encryption_key[32]);
        static SX1278 radio;
        static void radioListen();
        static bool receivePacket(Packet*);
        static bool sendPacket(Packet*);
        static bool pairingMode(PairingInfo*);
        static byte encryption_key[32];
};
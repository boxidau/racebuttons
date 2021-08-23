#pragma once
#include <Crypto.h>
#include <AES.h>
#include <Curve25519.h>
#include <RNG.h>
#include <string.h>
#include <Arduino.h>
#include <AbstractRadio.h>
#include <structures.h>
#include <FastCRC.h>
#include <TeensyID.h>

class CryptoRadio {
    public:
        CryptoRadio(AbstractRadio*);
        static void begin();
        static void setEncryptionKey(byte _encryption_key[32]);
        static bool receivePacket(Packet*);
        static bool sendPacket(Packet*);
        static bool pairingMode(PairingInfo*);
        static uint16_t getAddress();
    private:
        static AbstractRadio* radio;
        static byte encryption_key[32];
        static byte local_nonce;
        static byte remote_nonce;
};
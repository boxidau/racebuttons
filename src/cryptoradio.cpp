#include <cryptoradio.h>

// disable interrupt when it's not needed
volatile static bool enableInterrupt = true;
// flag to indicate that a packet was received
volatile static bool receivedFlag;

AES256 aes256;
FastCRC16 CRC16;

byte CryptoRadio::encryption_key[32];
SX1278 CryptoRadio::radio = new Module(SX_NSS, SX_DIO0, SX_RST);

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

void CryptoRadio::begin(byte _encryption_key[32])
{
    memcpy(encryption_key, _encryption_key, 32);
    Serial.print(F("[RADIO] Initializing ... "));
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

void CryptoRadio::radioListen()
{
    int state = radio.startReceive();
    if (state != ERR_NONE)
    {
        Serial.print(F("[RADIO] listen mode failed, code: "));
        Serial.println(state);
    }
}

bool isEncryptedPacketType(PacketType packet_type)
{
    return !((uint8_t)packet_type & 0x80);
}

void printPacket(const Packet *packet, bool rx_tx)
{
    Serial.print("[RADIO] ");
    Serial.print(rx_tx ? "RX" : "TX");
    Serial.print(" packet dest: " + String(packet->destination));
    Serial.print(", src: " + String(packet->source));
    Serial.print(F(", type: "));
    switch(packet->packet_type) {
        case PACKET_TYPE_PAIRING_REQUEST:
            Serial.print("PACKET_TYPE_PAIRING_REQUEST");
            break;
        case PACKET_TYPE_PAIRING_RESPONSE:
            Serial.print("PACKET_TYPE_PAIRING_RESPONSE");
            break;
        case PACKET_TYPE_PAIRING_TEST:
            Serial.print("PACKET_TYPE_PAIRING_TEST");
            break;
        case PACKET_TYPE_STATE:
            Serial.print("PACKET_TYPE_STATE");
            break;
    }
    Serial.print(", data: ");
    for (int i = 0; i < MAX_PACKET_DATA_LENGTH; i++)
    {
        Serial.print(" 0x");
        Serial.print(packet->data[i], HEX);
    }
    Serial.println("");
}

bool CryptoRadio::sendPacket(Packet *packet) {
    packet->source = (uint16_t)ESP.getChipId();
    printPacket(packet, false);

    bool is_encrypted = isEncryptedPacketType(packet->packet_type);
    if (is_encrypted) {
        // add nonce to data
        packet->data[MAX_PACKET_DATA_LENGTH - 4] = (uint8_t)random();
        packet->data[MAX_PACKET_DATA_LENGTH - 3] = (uint8_t)random();
        uint16_t checksum = CRC16.ccitt(packet->data, MAX_PACKET_DATA_LENGTH - 2);
        packet->data[MAX_PACKET_DATA_LENGTH - 2] = checksum & 0xff;
        packet->data[MAX_PACKET_DATA_LENGTH - 1] = (checksum >> 8);

        printPacket(packet, false);
        aes256.setKey(encryption_key, aes256.keySize());
        byte encrypted_data[MAX_PACKET_DATA_LENGTH];
        for (int i = 0; i < MAX_PACKET_DATA_LENGTH; i+=aes256.blockSize()) {
            byte buffer[16];
            aes256.encryptBlock(buffer, &packet->data[i]);
            memcpy(&encrypted_data[i], buffer, aes256.blockSize());
        }
    
        memcpy(packet->data, encrypted_data, MAX_PACKET_DATA_LENGTH);
    }
    
    byte* p_encoded = (byte*)packet;

    enableInterrupt = false;
    int state = radio.transmit(p_encoded, sizeof(Packet));
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
    return state == ERR_NONE;
}

bool CryptoRadio::receivePacket(Packet *packet)
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

        byte raw_data[sizeof(Packet)];
        // you can read received data as an Arduino String
        int state = radio.readData(raw_data, sizeof(Packet));

        if (state == ERR_NONE)
        {
            receivedPacket = true;
            memcpy(packet, raw_data, sizeof(Packet));

            bool is_encrypted = isEncryptedPacketType(packet->packet_type);
            if (is_encrypted) {
                byte decrypted_data[32];
                aes256.setKey(encryption_key, aes256.keySize());
                for (int i = 0; i < MAX_PACKET_DATA_LENGTH; i+=aes256.blockSize()) {
                    byte buffer[16];
                    aes256.decryptBlock(buffer, &packet->data[i]);
                    memcpy(&decrypted_data[i], buffer, aes256.blockSize());
                }

                memcpy(packet->data, decrypted_data, MAX_PACKET_DATA_LENGTH);
                
                uint16_t checksum = CRC16.ccitt(packet->data, MAX_PACKET_DATA_LENGTH - 2);
                uint16_t packet_checksum = packet->data[MAX_PACKET_DATA_LENGTH - 2];
                packet_checksum |= (packet->data[MAX_PACKET_DATA_LENGTH - 1] << 8);
                
                if (checksum != packet_checksum) {
                    Serial.println("[RADIO] checksum failure, ignoring packet");
                    receivedPacket = false;
                }
            }

            printPacket(packet, true);
    
            // print RSSI (Received Signal Strength Indicator)
            Serial.print(F("[RADIO] RSSI: "));
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
            Serial.println(F("[RADIO] CRC error!"));
        }
        else
        {
            // some other error occurred
            Serial.print(F("[RADIO] Failed, code "));
            Serial.println(state);
        }

        // we're ready to receive more packets,
        // enable interrupt service routine
        enableInterrupt = true;

        // put module back to listen mode
        radio.startReceive();

    }
    return receivedPacket;
}

bool CryptoRadio::pairingMode(PairingInfo *pairing_info)
{
    uint8_t local_k[32];
    uint8_t local_f[32]; // local secret
    uint8_t remote_k[32];

    Serial.println("Starting DH key exchange");
    Serial.println("Generate random k/f local keys ... ");
    Serial.flush();
    Curve25519::dh1(local_k, local_f);

    Packet p = {BROADCAST_ADDRESS, 0, PACKET_TYPE_PAIRING_REQUEST};
    memcpy(p.data, local_k, 32);

    unsigned long interval_timer = 0;
    unsigned long backoff = 0;

    bool pair_response_sent = false;
    bool pair_response_received = false;
    Packet recv_packet;
    while(true) {
        bool recv = receivePacket(&recv_packet);
        if (recv) {
            Serial.println("RECV PACKET");
        }

        if (recv && recv_packet.packet_type == PACKET_TYPE_PAIRING_REQUEST)
        {
            Serial.println("Received pairing request");
            memcpy(remote_k, recv_packet.data, 32);
            Packet pair_resp_p = {recv_packet.source, 0, PACKET_TYPE_PAIRING_RESPONSE};
            sendPacket(&pair_resp_p);
            pair_response_sent = true;
        }

        if (recv && recv_packet.packet_type == PACKET_TYPE_PAIRING_RESPONSE)
        {
            Serial.println("Received pairing response");
            pair_response_received = true;
        }
        
        if (pair_response_sent && pair_response_received) {
            break;
        }

        if (millis() > interval_timer + backoff + BROADCAST_INTERVAL) {
            interval_timer = millis();
            sendPacket(&p);
            if (backoff < 5000) {
                backoff+= 100;
            }
        }
    }

    if (!pair_response_sent || !pair_response_received) {
        Serial.print("Failed to pair: ");
        if (!pair_response_sent) {
            Serial.print(" no pairing request received");
        }
        if (!pair_response_received) {
            Serial.print(" no pairing response received");
        }
        Serial.println("");
        return false;
    }

    Serial.println("Generating shared secret ... ");
    Serial.flush();
    if (Curve25519::dh2(remote_k, local_f)) {
        // remote_k is now the shared secret
        memcpy(pairing_info->shared_key, remote_k, 32);
        memcpy(encryption_key, remote_k, 32);
        pairing_info->remote_address = recv_packet.source;
        pairing_info->base_station = (recv_packet.source < (uint16_t)ESP.getChipId());

        return true;
    }
    return false;

}
#include <cryptoradio.h>

AES256 aes256;
FastCRC16 CRC16;

byte CryptoRadio::encryption_key[32];
AbstractRadio* CryptoRadio::radio;

uint16_t CryptoRadio::getAddress() {

    static uint32_t addr;
    if (addr > 0) {
        return (uint16_t)addr;
    }

    uint32_t num = 0;
	__disable_irq();
    FTFL_FSTAT = FTFL_FSTAT_RDCOLERR | FTFL_FSTAT_ACCERR | FTFL_FSTAT_FPVIOL;
    FTFL_FCCOB0 = 0x41;
    FTFL_FCCOB1 = 15;
    FTFL_FSTAT = FTFL_FSTAT_CCIF;
    while (!(FTFL_FSTAT & FTFL_FSTAT_CCIF)) ; // wait
    num = *(uint32_t *)&FTFL_FCCOB7;
	__enable_irq();
    addr = (uint16_t)num;
    return addr;
}

CryptoRadio::CryptoRadio(AbstractRadio *_radio)
{
    radio = _radio;
}

void CryptoRadio::begin()
{
    Serial.println(F("[RADIO] Initializing ... "));
    Serial.print("[RADIO] node address: ");
    Serial.println(getAddress());
    radio->begin();
    radio->listen();
}

void CryptoRadio::setEncryptionKey(byte _encryption_key[32])
{
    memcpy(encryption_key, _encryption_key, 32);
}

bool isEncryptedPacketType(PacketType packet_type)
{
    return !((uint8_t)packet_type & 0x80);
}

void printPacket(const Packet *packet, bool rx_tx)
{
    Serial.print("[RADIO] ");
    Serial.print(rx_tx ? "RX" : "TX");
    Serial.print(" packet dest: ");
    Serial.print(packet->destination, HEX);
    Serial.print(", src: ");
    Serial.print(packet->source, HEX);
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
    packet->source = getAddress();
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

    bool result = radio->send(p_encoded, sizeof(Packet));
    radio->listen();
    return result;
}

bool CryptoRadio::receivePacket(Packet *packet)
{
    static byte raw_data[sizeof(Packet)];
    bool receivedPacket = false;
    // check if the flag is set
    if (radio->receive(raw_data, sizeof(Packet)))
    {
        receivedPacket = true;
        memcpy(packet, raw_data, sizeof(Packet));
        if (packet->destination != getAddress() && packet->destination != RADIO_BROADCAST_ADDRESS)
        {
            Serial.print("invalid destination, dropping packet for ");
            Serial.println(packet->destination, HEX);
            return false;
        }

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

    Packet p = {RADIO_BROADCAST_ADDRESS, 0, PACKET_TYPE_PAIRING_REQUEST};
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
        return true;
    }
    return false;

}
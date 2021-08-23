#include <SX1278Radio.h>

// disable interrupt when it's not needed
volatile static bool enableInterrupt = true;
// flag to indicate that a packet was received
volatile static bool receivedFlag;

SX1278 SX1278Radio::radio = new Module(SX_NSS, SX_DIO0, SX_RST);

void setFlag(void)
{
    // check if the interrupt is enabled
    if (!enableInterrupt)
    {
        return;
    }
    // we got a packet, set the flag
    receivedFlag = true;
}

void SX1278Radio::begin() {
    int state = radio.begin(434.0, 125.0, 9, 7, SX127X_SYNC_WORD, 17, 8, 0);
    
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
}

void SX1278Radio::listen() {
    enableInterrupt = true;
    int state = radio.startReceive();
    if (state != ERR_NONE)
    {
        Serial.print(F("[RADIO] listen mode failed, code: "));
        Serial.println(state);
    }
}

bool SX1278Radio::receive(byte* data, uint8_t size) {
    if (!receivedFlag)
    {
        return false;
    }

    bool receiveOkay = false;
    
    // disable the interrupt service routine while
    // processing the data
    enableInterrupt = false;
    // reset flag
    receivedFlag = false;

    // you can read received data as an Arduino String
    int state = radio.readData(data, size);
    if (state == ERR_NONE) 
    {
        receiveOkay = true;
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

    // put module back to listen mode
    listen();
    return receiveOkay;
}

bool SX1278Radio::send(byte* data, uint8_t size) {
    enableInterrupt = false;
    int state = radio.transmit(data, size);
    enableInterrupt = true;

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
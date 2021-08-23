#include <E32Radio.h>



LoRa_E32 E32Radio::radio = LoRa_E32(&Serial3, RADIO_AUX, RADIO_M0, RADIO_M1, UART_BPS_RATE_9600);

void waitForAuxLow()
{
    while(digitalRead(RADIO_AUX) == HIGH)
    ;
}

void waitForAuxHigh()
{
    while(digitalRead(RADIO_AUX) == LOW)
    ;
}

void clearSerial()
{
    while(Serial3.available() > 0)
    {
        Serial3.read();
    }
}

void E32Radio::begin() {
    radio.begin();
    Serial.println("[RADIO] E32 initialized OK");

    ResponseStructContainer c = radio.getConfiguration();
    Serial.println(c.status.getResponseDescription());
    Serial.println(c.status.code);

    // if (!c.status.code == SUCCESS)
    // {
    //     Configuration configuration = *(Configuration*) c.data;
    //     // configuration.ADDL = 0x0;
    //     // configuration.ADDH = 0x0;
    //     // configuration.CHAN = 0x23;

    //     // configuration.OPTION.fec = FEC_0_OFF;
    //     // configuration.OPTION.fixedTransmission = FT_TRANSPARENT_TRANSMISSION;
    //     // configuration.OPTION.ioDriveMode = IO_D_MODE_PUSH_PULLS_PULL_UPS;
    //     configuration.OPTION.transmissionPower = POWER_17;
    //     // configuration.OPTION.wirelessWakeupTime = WAKE_UP_1250;

    //     // configuration.SPED.airDataRate = AIR_DATA_RATE_011_48;
    //     // configuration.SPED.uartBaudRate = UART_BPS_115200;
    //     // configuration.SPED.uartParity = MODE_00_8N1;
    // }

    waitForAuxHigh();
    

}

void E32Radio::listen() {
    waitForAuxHigh();
}

bool E32Radio::receive(byte *data, uint8_t size)
{
    if (Serial3.available() == 0)
    {
        return false;
    }

    elapsedMillis elapsed;
    while (Serial3.available() < size)
    {
        if (elapsed > 300) {
            Serial.print("Receive timeout");
            clearSerial();
            return false;
        }
    }
    
    auto res = Serial3.readBytes(data, size);
    clearSerial();
    waitForAuxHigh();   

    Serial.print("Receive took (ms): ");
    Serial.println(elapsed);
    return res == size;
}

bool E32Radio::send(byte *data, uint8_t size)
{
    elapsedMillis elapsed;
    waitForAuxHigh();
    clearSerial();
    auto written = Serial3.write(data, size);
    waitForAuxLow();
    waitForAuxHigh();
    Serial.print("Send took (ms): ");
    Serial.println(elapsed);
    return written == size;

}
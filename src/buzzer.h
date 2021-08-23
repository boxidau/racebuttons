#pragma once
#include <Arduino.h>

class Buzzer {
    private:
        elapsedMillis beeping;
        uint8_t beepDuration;
        uint8_t pin;

    public:
        Buzzer(uint8_t _pin)
        {
            pin = _pin;
        }

        void beep(uint16_t duration)
        {
            beepDuration = duration;
            beeping = 0;
            analogWrite(pin, 80);
        }

        void update()
        {
            if (beeping > beepDuration)
            {
                analogWrite(pin, 0);
            }
        }
};
#include <structures.h>


uint8_t packBitmap(Button *buttons, uint8_t len)
{
    uint8_t bitmap = 0;
    for (uint8_t i = 0; i < len; i++)
    {
        bitmap |= (buttons[i].local_status << i);
    }
    return bitmap;
}

void unpackBitmap(uint8_t bitmap, StateType state_type, Button *buttons, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++)
    {
        if (state_type == REMOTE_STATE) {
            buttons[i].remote_status = bitmap & (1 << i);
        } else {
            buttons[i].local_status = bitmap & (1 << i);
        }
    }
}

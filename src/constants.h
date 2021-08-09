#define BUTTON_COUNT 4
#define BROADCAST_INTERVAL 1000
#define DEAD_COMMS_INTERVAL 10000
#define DEBOUNCE_TIME 50

#define RESET_OKAY_BUTTON 0

#define PIN_LED_STRIP 4 // D2
#define STRIP_BRIGHTNESS 30

#define PACKET_LENGTH 4
#define MAX_PACKET_DATA_LENGTH 32

#define BROADCAST_ADDRESS 0xFFFF

// SPI Pins
#define HSCLK 14   // D5
#define HMISO 12   // D6
#define HMOSI 13   // D7
#define SX_NSS 15  // D8
#define SX_RST 16  // D0
#define SX_DIO0 10 // SD3

// Button Pins
#define PIN_D1 5
#define PIN_D3 0
#define PIN_D4 2
#define PIN_SD2 9
// Define Arduino Library
#ifndef __Arduino__
	#include <Arduino.h>
#endif

// Define Digital SPI Pins
#define Digital_MISO_PORT  PORTB
#define Digital_MISO_DDR   DDRB
#define Digital_MISO_PIN   0

#define Digital_MOSI_PORT  PORTB
#define Digital_MOSI_DDR   DDRB
#define Digital_MOSI_PIN   1

#define Digital_SCK_PORT   PORTB
#define Digital_SCK_DDR    DDRB
#define Digital_SCK_PIN    2

#define Digital_Reset_PORT PORTB
#define Digital_Reset_DDR  DDRB
#define Digital_Reset_PIN  3

// Define Power Pins
#define Burn_Enable_PORT   PORTB
#define Burn_Enable_DDR    DDRB
#define Burn_Enable_PIN    0

#define Power_Done_PORT    PORTB
#define Power_Done_DDR     DDRB
#define Power_Done_PIN     1

// Define LED Pins
#define LED_Red_PORT       PORTC
#define LED_Red_DDR        DDRC
#define LED_Red_PIN        3

#define LED_Green_PORT     PORTC
#define LED_Green_DDR      DDRC
#define LED_Green_PIN      2

#define LED_Blue_PORT      PORTC
#define LED_Blue_DDR       DDRC
#define LED_Blue_PIN       1












const char 				Firmware_Name[] 				= "FW.HEX";


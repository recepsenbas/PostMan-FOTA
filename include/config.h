// Define Arduino Library
#ifndef __Arduino__
	#include <Arduino.h>
#endif

//------------------------------------------------------------------------------
//      CONFIG
//------------------------------------------------------------------------------
const char 				Version[] 						= "00.01.01";
const char 				name[] 							= "STF.hex";
//------------------------------------------------------------------------------
uint8_t 				_LED_Error_ 					= A3;
uint8_t 				_LED_Ready_ 					= A2;
uint8_t 				_LED_Working_ 					= A1;
const int 				noLED 							= -1;
//------------------------------------------------------------------------------
const unsigned int 		ENTER_PROGRAMMING_ATTEMPTS 		= 1;
const byte 				BB_DELAY_MICROSECONDS 			= 6; // control speed of programming
//------------------------------------------------------------------------------

// bit banged SPI pins
const byte 				MSPIM_SCK 						= 2;
const byte 				MSPIM_SS  						= 3;
const byte 				BB_MISO   						= 0;
const byte 				BB_MOSI   						= 1;
#define BB_MISO_PORT PIND
#define BB_MOSI_PORT PORTD
#define BB_SCK_PORT PORTD
const byte 				BB_SCK_BIT 						= 2;
const byte 				BB_MISO_BIT 					= 0;
const byte 				BB_MOSI_BIT 					= 1;
const byte 				RESET 							= MSPIM_SS;

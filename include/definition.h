// Define Arduino Library
#ifndef __Arduino__
	#include <Arduino.h>
#endif

// if signature found in above table, this is its index
int foundSig = -1;
byte lastAddressMSB = 0;

// Number of Items in an Array
#define NUMITEMS(arg) ((uint16_t) (sizeof (arg) / sizeof (arg [0])))

// Message Type Definitions
#define MSG_NO_SD_CARD							0	// cannot open SD card
#define MSG_CANNOT_OPEN_FILE					1	// canoot open file 'wantedFile' (above)
#define MSG_LINE_TOO_LONG						2	// line on disk too long to read
#define MSG_LINE_TOO_SHORT						3	// line too short to be valid
#define MSG_LINE_DOES_NOT_START_WITH_COLON		4	// line does not start with a colon
#define MSG_INVALID_HEX_DIGITS					5	// invalid hex where there should be hex
#define MSG_BAD_SUMCHECK						6	// line fails sumcheck
#define MSG_LINE_NOT_EXPECTED_LENGTH			7	// record not length expected
#define MSG_UNKNOWN_RECORD_TYPE					8	// record type not known
#define MSG_NO_END_OF_FILE_RECORD				9	// no 'end of file' at end of file
#define MSG_FILE_TOO_LARGE_FOR_FLASH			10	// file will not fit into flash
#define MSG_CANNOT_ENTER_PROGRAMMING_MODE		11	// cannot program target chip
#define MSG_NO_BOOTLOADER_FUSE					12	// chip does not have bootloader
#define MSG_CANNOT_FIND_SIGNATURE				13	// cannot find chip signature
#define MSG_UNRECOGNIZED_SIGNATURE				14	// signature not known
#define MSG_BAD_START_ADDRESS					15	// file start address invalid
#define MSG_VERIFICATION_ERROR					16	// verification error after programming
#define MSG_FLASHED_OK							17	// flashed OK
#define MSG_FLASHED_TEST						18	// flashed OK
	
// SPI Commands	
#define Command_Progam_Enable					0xAC
#define Command_Chip_Erase						0x80
#define Command_Write_Lock_Byte					0xE0
#define Command_Write_Low_Fuse_Byte				0xF7
#define Command_Write_High_Fuse_Byte			0xD6
#define Command_Write_Extended_Fuse_Byte		0xFF
#define Command_Poll_Ready						0xF0
#define Command_Program_ACK						0x53
#define Command_Read_Signature_Byte				0x30
#define Command_Read_Calibration_Byte			0x38
#define Command_Read_Low_Fuse_Byte				0x50
#define Command_Read_Low_Fuse_Byte_Arg2			0x00
#define Command_Read_Extended_Fuse_Byte			0x50
#define Command_Read_Extended_Fuse_Byte_Arg2	0x08
#define Command_Read_High_Fuse_Byte				0x58
#define Command_Read_High_Fuse_Byte_Arg2		0x08
#define Command_Read_Lock_Byte					0x58
#define Command_Read_Lock_Byte_Arg2				0x00
#define Command_Read_Program_Memory				0x20
#define Command_Write_Program_Memory			0x4C
#define Command_Load_Extended_Address_Byte		0x4D
#define Command_Load_Program_Memory				0x40

// Actions Definitions
#define Action_Check_File						0
#define Action_Verify_Flash						1
#define Action_Write_To_Flash					2

// Fuse Definitions
#define Low_Fuse								0
#define High_Fuse								1
#define Ext_Fuse								2
#define Lock_Byte								3
#define Calibration_Byte						4

// HEX Record Types
#define HEX_Data_Record							0
#define HEX_End_Of_File							1
#define HEX_Extended_Segment_Address_Record		2
#define HEX_Start_Segment_Address_Record		3
#define HEX_Extended_Linear_Addres_Record		4
#define HEX_Start_Linear_Address_Record 		5	

// Define LED
#define LED_No									0
#define LED_Error								A3
#define LED_Ready								A2
#define LED_Working								A1

// Signature Definitions
typedef struct {
	
	// Signature
	uint8_t Signature [3];

	// Description
	const char * Description;

	// Flash Size
	uint32_t Flash_Size;

	// Bootloader Size
	uint32_t Bootloader_Size;

	// Page Size
	uint32_t Page_Size;

	// Fuse With Bootloader Size
	uint8_t Fuse_With_Bootloader_Size;

	// Timed Writes
	bool Timed_Writes;

} Signature_Type;

// Definitions
#define	NO_FUSE									0xFF

// Fuse Definitions
const Signature_Type Signatures[] PROGMEM = {

		// Attiny84 family
		{{0x1E, 0x91, 0x0B}, "ATtiny24", 2048, 0, 32, NO_FUSE, false},
		{{0x1E, 0x92, 0x07}, "ATtiny44", 4096, 0, 64, NO_FUSE, false},
		{{0x1E, 0x93, 0x0C}, "ATtiny84", 8192, 0, 64, NO_FUSE, false},

		// Attiny85 family
		{{0x1E, 0x91, 0x08}, "ATtiny25", 2048, 0, 32, NO_FUSE, false},
		{{0x1E, 0x92, 0x06}, "ATtiny45", 4096, 0, 64, NO_FUSE, false},
		{{0x1E, 0x93, 0x0B}, "ATtiny85", 8192, 0, 64, NO_FUSE, false},

		// Atmega328 family
		{{0x1E, 0x92, 0x0A}, "ATmega48PA", 4096, 0, 64, NO_FUSE, false},
		{{0x1E, 0x93, 0x0F}, "ATmega88PA", 8192, 256, 128, Ext_Fuse, false},
		{{0x1E, 0x94, 0x0B}, "ATmega168PA", 16384, 256, 128, Ext_Fuse, false},
		{{0x1E, 0x95, 0x0F}, "ATmega328P", 32768, 512, 128, High_Fuse, false},

		// Atmega644 family
		{{0x1E, 0x94, 0x0A}, "ATmega164P", 16384, 256, 128, High_Fuse, false},
		{{0x1E, 0x95, 0x08}, "ATmega324P", 32768, 512, 128, High_Fuse, false},
		{{0x1E, 0x96, 0x0A}, "ATmega644P", 65536, 1024, 256, High_Fuse, false},

		// Atmega2560 family
		{{0x1E, 0x96, 0x08}, "ATmega640", 65536, 1024, 256, High_Fuse, false},
		{{0x1E, 0x97, 0x03}, "ATmega1280", 131072, 1024, 256, High_Fuse, false},
		{{0x1E, 0x97, 0x04}, "ATmega1281", 131072, 1024, 256, High_Fuse, false},
		{{0x1E, 0x98, 0x01}, "ATmega2560", 262144, 1024, 256, High_Fuse, false},
		{{0x1E, 0x98, 0x02}, "ATmega2561", 262144, 1024, 256, High_Fuse, false},

		// AT90USB family
		{{0x1E, 0x93, 0x82}, "At90USB82", 8192, 512, 128, High_Fuse, false},
		{{0x1E, 0x94, 0x82}, "At90USB162", 16384, 512, 128, High_Fuse, false},

		// Atmega32U2 family
		{{0x1E, 0x93, 0x89}, "ATmega8U2", 8192, 512, 128, High_Fuse, false},
		{{0x1E, 0x94, 0x89}, "ATmega16U2", 16384, 512, 128, High_Fuse, false},
		{{0x1E, 0x95, 0x8A}, "ATmega32U2", 32768, 512, 128, High_Fuse, false},

		// Atmega32U4 family -  (datasheet is wrong about flash page size being 128 words)
		{{0x1E, 0x94, 0x88}, "ATmega16U4", 16384, 512, 128, High_Fuse, false},
		{{0x1E, 0x95, 0x87}, "ATmega32U4", 32768, 512, 128, High_Fuse, false},

		// ATmega1284P family
		{{0x1E, 0x97, 0x05}, "ATmega1284P", 131072, 1024, 256, High_Fuse, false},

		// ATtiny4313 family
		{{0x1E, 0x91, 0x0A}, "ATtiny2313A", 2048, 0, 32, NO_FUSE, false},
		{{0x1E, 0x92, 0x0D}, "ATtiny4313", 4096, 0, 64, NO_FUSE, false},

		// ATtiny13 family
		{{0x1E, 0x90, 0x07}, "ATtiny13A", 1024, 0, 32, NO_FUSE, false},

		// Atmega8A family
		{{0x1E, 0x93, 0x07}, "ATmega8A", 8192, 256, 64, High_Fuse, true},

		// ATmega64rfr2 family
		{{0x1E, 0xA6, 0x02}, "ATmega64rfr2", 262144, 1024, 256, High_Fuse, false},
		{{0x1E, 0xA7, 0x02}, "ATmega128rfr2", 262144, 1024, 256, High_Fuse, false},
		{{0x1E, 0xA8, 0x02}, "ATmega256rfr2", 262144, 1024, 256, High_Fuse, false},

}; // end of signatures
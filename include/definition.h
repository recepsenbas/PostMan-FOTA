// Define Arduino Library
#ifndef __Arduino__
	#include <Arduino.h>
#endif

typedef enum {

	// cannot open SD card
	MSG_NO_SD_CARD,
	
	// canoot open file 'wantedFile' (above)
	MSG_CANNOT_OPEN_FILE,
	
	// line on disk too long to read
	MSG_LINE_TOO_LONG,

	// line too short to be valid
	MSG_LINE_TOO_SHORT,

	// line does not start with a colon
	MSG_LINE_DOES_NOT_START_WITH_COLON,

	// invalid hex where there should be hex
	MSG_INVALID_HEX_DIGITS,

	// line fails sumcheck
	MSG_BAD_SUMCHECK,

	// record not length expected
	MSG_LINE_NOT_EXPECTED_LENGTH,

	// record type not known
	MSG_UNKNOWN_RECORD_TYPE,

	// no 'end of file' at end of file
	MSG_NO_END_OF_FILE_RECORD,

	// file will not fit into flash
	MSG_FILE_TOO_LARGE_FOR_FLASH,

	// cannot program target chip
	MSG_CANNOT_ENTER_PROGRAMMING_MODE,

	// chip does not have bootloader
	MSG_NO_BOOTLOADER_FUSE,

	// cannot find chip signature
	MSG_CANNOT_FIND_SIGNATURE,

	// signature not known
	MSG_UNRECOGNIZED_SIGNATURE,

	// file start address invalid
	MSG_BAD_START_ADDRESS,

	// verification error after programming
	MSG_VERIFICATION_ERROR,

	// flashed OK
	MSG_FLASHED_OK,

} msgType;

// programming commands to send via SPI to the chip
enum {
	
	progamEnable 				= 0xAC,
	chipErase 					= 0x80,
	writeLockByte 				= 0xE0,
	writeLowFuseByte 			= 0xF7,
	writeHighFuseByte 			= 0xD6,
	writeExtendedFuseByte 		= 0xFF,
	pollReady 					= 0xF0,
	programAcknowledge 			= 0x53,
	readSignatureByte 			= 0x30,
	readCalibrationByte 		= 0x38,
	readLowFuseByte 			= 0x50,
	readLowFuseByteArg2 		= 0x00,
	readExtendedFuseByte 		= 0x50,
	readExtendedFuseByteArg2 	= 0x08,
	readHighFuseByte 			= 0x58,
	readHighFuseByteArg2 		= 0x08,
	readLockByte 				= 0x58,
	readLockByteArg2 			= 0x00,
	readProgramMemory 			= 0x20,
	writeProgramMemory 			= 0x4C,
	loadExtendedAddressByte 	= 0x4D,
	loadProgramMemory 			= 0x40,
	
}; 

// actions to take
enum {
	checkFile,
	verifyFlash,
	writeToFlash,
};

// meaning of bytes in above array
enum {
	lowFuse,
	highFuse,
	extFuse,
	lockByte,
	calibrationByte
};

// types of record in .hex file
enum {
	hexDataRecord,  					// 00
	hexEndOfFile,   					// 01
	hexExtendedSegmentAddressRecord, 	// 02
	hexStartSegmentAddressRecord,  		// 03
	hexExtendedLinearAddressRecord, 	// 04
	hexStartLinearAddressRecord 		// 05
};

typedef struct {
	byte sig [3];
	const char * desc;
	unsigned long flashSize;
	unsigned int baseBootSize;
	unsigned long pageSize;
	byte fuseWithBootloaderSize;
	byte timedWrites;
} signatureType;

const signatureType signatures [] PROGMEM = {

	 {
		 { 0x1E, 0x98, 0x01 },		// Signature (Atmega2560 - 0x1E, 0x98, 0x01
		 "ATmega2560",				// Description
		 262144,					// Flash Size
		 1024,						// Bootloader Size
		 256,						// Flash Page Size
		 highFuse,					// Fuse to Change
		 false						//
	 },
		 
};

// number of items in an array
#define NUMITEMS(arg) ((unsigned int) (sizeof (arg) / sizeof (arg [0])))

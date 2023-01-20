/* *******************************************************************************
 *  Copyright (C) 2014-2021 Mehmet Gunce Akkoyun Can not be copied and/or
 *	distributed without the express permission of Mehmet Gunce Akkoyun.
 *
 *	Project				: B100AA Firmware Over The Air Programming
 *	Code Developer		: Mehmet Gunce Akkoyun (akkoyun@me.com)
 *	Revision			: 01.00.00
 *
 *********************************************************************************/

#include <SdFat.h>
#include <EEPROM.h>

// file system object
SdFat sd;

// if signature found in above table, this is its index
int foundSig = -1;
byte lastAddressMSB = 0;

//------------------------------------------------------------------------------
//      CONFIG
//------------------------------------------------------------------------------
const char 				Version[] 						= "00.01.01";
const char 				name[] 							= "STF.HEX";
//------------------------------------------------------------------------------
const int 				errorLED 						= 17;
const int 				readyLED 						= 16;
const int 				workingLED 						= 15;
const int 				noLED 							= -1;
//------------------------------------------------------------------------------
const unsigned int 		ENTER_PROGRAMMING_ATTEMPTS 		= 5;
const byte 				BB_DELAY_MICROSECONDS 			= 6;
//------------------------------------------------------------------------------

// bit banged SPI pins
const byte 				MSPIM_SCK 						= 2;
const byte 				BB_MISO   						= 0;
const byte 				BB_MOSI   						= 1;
#define BB_MISO_PORT PIND
#define BB_MOSI_PORT PORTD
#define BB_SCK_PORT PORTD
const byte 				BB_SCK_BIT 						= 2;
const byte 				BB_MISO_BIT 					= 0;
const byte 				BB_MOSI_BIT 					= 1;
//const byte 				RESET 							= 3;






//------------------------------------------------------------------------------
// status "messages"
typedef enum {
	
	MSG_NO_SD_CARD,          // cannot open SD card
	MSG_CANNOT_OPEN_FILE,    // canoot open file 'wantedFile' (above)
	MSG_LINE_TOO_LONG,       // line on disk too long to read
	MSG_LINE_TOO_SHORT,      // line too short to be valid
	MSG_LINE_DOES_NOT_START_WITH_COLON,  // line does not start with a colon
	MSG_INVALID_HEX_DIGITS,  // invalid hex where there should be hex
	MSG_BAD_SUMCHECK,        // line fails sumcheck
	MSG_LINE_NOT_EXPECTED_LENGTH,  // record not length expected
	MSG_UNKNOWN_RECORD_TYPE,  // record type not known
	MSG_NO_END_OF_FILE_RECORD,  // no 'end of file' at end of file
	MSG_FILE_TOO_LARGE_FOR_FLASH,  // file will not fit into flash

	MSG_CANNOT_ENTER_PROGRAMMING_MODE,  // cannot program target chip
	MSG_NO_BOOTLOADER_FUSE,             // chip does not have bootloader
	MSG_CANNOT_FIND_SIGNATURE,      // cannot find chip signature
	MSG_UNRECOGNIZED_SIGNATURE,     // signature not known
	MSG_BAD_START_ADDRESS,          // file start address invalid
	MSG_VERIFICATION_ERROR,         // verification error after programming
	MSG_FLASHED_OK,                 // flashed OK
	
} msgType;
//------------------------------------------------------------------------------
enum {
	
	progamEnable = 0xAC,
	
	// writes are preceded by progamEnable
	chipErase = 0x80,
	writeLockByte = 0xE0,
	writeLowFuseByte = 0xF7,
	writeHighFuseByte = 0xD6,
	writeExtendedFuseByte = 0xFF,
	
	pollReady = 0xF0,
	
	programAcknowledge = 0x53,
	
	readSignatureByte = 0x30,
	readCalibrationByte = 0x38,
	
	readLowFuseByte = 0x50,       readLowFuseByteArg2 = 0x00,
	readExtendedFuseByte = 0x50,  readExtendedFuseByteArg2 = 0x08,
	readHighFuseByte = 0x58,      readHighFuseByteArg2 = 0x08,
	readLockByte = 0x58,          readLockByteArg2 = 0x00,
	
	readProgramMemory = 0x20,
	writeProgramMemory = 0x4C,
	loadExtendedAddressByte = 0x4D,
	loadProgramMemory = 0x40,
	
};  // programming commands to send via SPI to the chip
//------------------------------------------------------------------------------
enum {
	checkFile,
	verifyFlash,
	writeToFlash,
}; // actions to take
//------------------------------------------------------------------------------
enum {
	lowFuse,
	highFuse,
	extFuse,
	lockByte,
	calibrationByte
}; // meaning of bytes in above array
//------------------------------------------------------------------------------
enum {
	hexDataRecord,  					// 00
	hexEndOfFile,   					// 01
	hexExtendedSegmentAddressRecord, 	// 02
	hexStartSegmentAddressRecord,  		// 03
	hexExtendedLinearAddressRecord, 	// 04
	hexStartLinearAddressRecord 		// 05
}; // types of record in .hex file
//------------------------------------------------------------------------------
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
		 
};  // end of signatures
//------------------------------------------------------------------------------
signatureType currentSignature; // copy of current signature entry for matching processor
//------------------------------------------------------------------------------

unsigned long pagesize, pagemask, oldPage, extendedAddress, lowestAddress, highestAddress, bytesWritten;
unsigned int progressBarCount, errors, lineCount;
bool gotEndOfFile;

byte program (const byte b1, const byte b2 = 0, const byte b3 = 0, const byte b4 = 0) {
  
	noInterrupts ();
	BB_SPITransfer (b1);
	BB_SPITransfer (b2);
	BB_SPITransfer (b3);
	byte b = BB_SPITransfer (b4);
	interrupts ();
	return b;
	
} // end of program
byte readFlash (unsigned long addr) {
	
	byte high = (addr & 1) ? 0x08 : 0;  // set if high byte wanted
	addr >>= 1;  // turn into word address

	// set the extended (most significant) address byte if necessary
	byte MSB = (addr >> 16) & 0xFF;
	
	if (MSB != lastAddressMSB) {
		
		program (loadExtendedAddressByte, 0, MSB);
		lastAddressMSB = MSB;
		
	}  // end if different MSB

	return program (readProgramMemory | high, highByte (addr), lowByte (addr));
	
} // end of readFlash
uint8_t BB_SPITransfer (uint8_t c) {
	
	uint8_t bit;
   
	for (bit = 0; bit < 8; bit++) {
	
		// write MOSI on falling edge of previous clock
		if (c & 0x80) PORTD |= bit(BB_MOSI_BIT);
		else PORTD &= ~bit(BB_MOSI_BIT);
	
		c <<= 1;
 
		// read MISO
		c |= (PIND & bit(BB_MISO_BIT)) != 0;
 
		// clock high
		PORTD |= bit (BB_SCK_BIT);
 
		// delay between rise and fall of clock
		delayMicroseconds(6);
 
		// clock low
		PORTD &= ~bit (BB_SCK_BIT);

		// delay between rise and fall of clock
		delayMicroseconds(6);
		
	}
   
	return c;
	
}  // end of BB_SPITransfer

const unsigned long NO_PAGE = 0xFFFFFFFF;

byte fuses [5]; // copy of fuses/lock bytes found for this processor

// number of items in an array
#define NUMITEMS(arg) ((unsigned int) (sizeof (arg) / sizeof (arg [0])))

// which program instruction writes which fuse
const byte fuseCommands [4] = { writeLowFuseByte, writeHighFuseByte, writeExtendedFuseByte, writeLockByte };


//------------------------------------------------------------------------------
//      SETUP
//------------------------------------------------------------------------------
void setup (void) {

	// Set All LED Signals Output and Pull-Down
	DDRC	|= 0b00001110;
	PORTC	&= 0b11110001;

	// Port B
	DDRB	|= 0b00000011;
	PORTB 	&= 0b11111100;

	// Boot Delay
	delay(500);
  
	// breadboards.  use SPI_FULL_SPEED for better performance.
	while (!sd.begin (10, SPI_HALF_SPEED)) {

		ShowMessage(MSG_NO_SD_CARD);

		// OFF Burn Buffer
		PORTB 	&= 0b11111110;

		// Done Signal [HIGH]
		PORTB	|= 0b00000010;
		
		// Sleep Delay
		delay(200);

	}
  
}  // end of setup

//------------------------------------------------------------------------------
//      LOOP
//------------------------------------------------------------------------------
void loop (void) {

	// ON Burn Buffer
	PORTB 	|= 0b00000001;
	delay(10);

	if (!startProgramming()) {
		
		ShowMessage (MSG_CANNOT_ENTER_PROGRAMMING_MODE);

		// Sleep Delay
		delay(200);
		
		return;
		
	}  // end of could not enter programming mode

	// Get Signature
	getSignature ();
	
	// Get Fuse Byte
	getFuseBytes ();
  
	// No Signature Found
	if (foundSig == -1) {
		
		// Signature Error Message
		ShowMessage (MSG_CANNOT_FIND_SIGNATURE);
		
		// OFF Burn Buffer
		PORTB 	&= 0b11111110;

		// Done Signal [HIGH]
		PORTB	|= 0b00000010;
		
		// Sleep Delay
		delay(200);

		return;

	}
  
	// Set Working LED ON
	digitalWrite(workingLED, HIGH);
	
	// Burn Firmware
	bool ok = writeFlashContents();
	
	// Turn OFF LED
	digitalWrite(workingLED, LOW);
	digitalWrite(readyLED, LOW);
		
	// Delay
	delay (200);
  
	// OK Message
	if (ok) ShowMessage (MSG_FLASHED_OK);

	// Stop Programming
	stopProgramming ();
	delay(100);

	// OFF Burn Buffer
	PORTB 	&= 0b11111110;

	// Done Signal [HIGH]
	PORTB	|= 0b00000010;
	
	// Sleep Delay
	delay(200);

}

//------------------------------------------------------------------------------
//      FUNCTIONS
//------------------------------------------------------------------------------
void getSignature(void) {
	
	foundSig = -1;
	lastAddressMSB = 0;
	
	byte sig[3];
	
	for (byte i = 0; i < 3; i++) sig[i] = program(readSignatureByte, 0, i);
  
	for (unsigned int j = 0; j < NUMITEMS(signatures); j++) {
		
		// Get MCU Signature
		memcpy_P(&currentSignature, &signatures [j], sizeof currentSignature);
	
		// Control Signature
		if (memcmp(sig, currentSignature.sig, sizeof sig) == 0) {
			
			foundSig = j;
			
			// make sure extended address is zero to match lastAddressMSB variable
			program(loadExtendedAddressByte, 0, 0);
			
			return;
			
		}  // end of signature found
		
	}  // end of for each signature

	ShowMessage (MSG_UNRECOGNIZED_SIGNATURE);
	
}
void getFuseBytes(void) {
	
	fuses [lowFuse]   = program(readLowFuseByte, readLowFuseByteArg2);
	fuses [highFuse]  = program(readHighFuseByte, readHighFuseByteArg2);
	fuses [extFuse]   = program(readExtendedFuseByte, readExtendedFuseByteArg2);
	fuses [lockByte]  = program(readLockByte, readLockByteArg2);
	fuses [calibrationByte]  = program(readCalibrationByte);
	
}
bool readHexFile(const char * fName, const byte action) {
  
	const int maxLine = 80;
	char buffer[maxLine];
	ifstream sdin (fName);
	int lineNumber = 0;
	gotEndOfFile = false;
	extendedAddress = 0;
	errors = 0;
	lowestAddress = 0xFFFFFFFF;
	highestAddress = 0;
	bytesWritten = 0;
	progressBarCount = 0;

	pagesize = currentSignature.pageSize;
	pagemask = ~(pagesize - 1);
	oldPage = NO_PAGE;

	
	
	// check for open error
	if (!sdin.is_open()) {
		ShowMessage(MSG_CANNOT_OPEN_FILE);
		
		// OFF Burn Buffer
		PORTB 	&= 0b11111110;

		// Done Signal [HIGH]
		PORTB	|= 0b00000010;
		
		// Sleep Delay
		delay(200);
		
		return true;
	}

	switch (action) {
		
		case checkFile:
			break;
	  
		case verifyFlash:
			break;
	
		case writeToFlash:
			program (progamEnable, chipErase);   // erase it
			delay (20);  // for Atmega8
			pollUntilReady ();
			clearPage();  // clear temporary page
			break;
			
	} // end of switch
 
	while (sdin.getline (buffer, maxLine)) {
		
		lineNumber++;
		int count = sdin.gcount();
		if (sdin.fail()) {
			ShowMessage (MSG_LINE_TOO_LONG);
			return true;
		}  // end of fail (line too long?)
	  
		// ignore empty lines
		if (count > 1) {
			if (processLine (buffer, action)) {
				return true;  // error
			}
		}
	}    // end of while each line
	
	if (!gotEndOfFile) {
		ShowMessage (MSG_NO_END_OF_FILE_RECORD);
		
		// OFF Burn Buffer
		PORTB 	&= 0b11111110;

		// Done Signal [HIGH]
		PORTB	|= 0b00000010;
		
		// Sleep Delay
		delay(200);
		
		return true;
	}

	switch (action) {
		
		case writeToFlash:
			
			// commit final page
			if (oldPage != NO_PAGE) commitPage (oldPage);
			break;
	  
		case verifyFlash:
			if (errors > 0) {
				ShowMessage (MSG_VERIFICATION_ERROR);
				
				// OFF Burn Buffer
				PORTB 	&= 0b11111110;

				// Done Signal [HIGH]
				PORTB	|= 0b00000010;
				
				// Sleep Delay
				delay(200);
				
				return true;
			}  // end if
			break;
		
		case checkFile:
			break;
			
	}  // end of switch
  
	return false;
	

}
void blink(const int whichLED1, const int whichLED2, const byte times = 1, const unsigned long repeat = 1, const unsigned long interval = 100) {
  
	for (unsigned long i = 0; i < repeat; i++) {
		
		for (byte j = 0; j < times; j++) {
			
			digitalWrite (whichLED1, HIGH);
			
			if (whichLED2 != noLED) digitalWrite (whichLED2, HIGH); delay (interval); digitalWrite (whichLED1, LOW);

			if (whichLED2 != noLED) digitalWrite (whichLED2, LOW); delay (interval);
			
		}  // end of for loop
	
		if (i < (repeat - 1)) delay (500);
		
	}  // end of repeating the sequence

}
void ShowMessage(const byte which) {
  
	// Set All LED Signals LOW
	PORTC &= 0b11110001;

	// now flash an appropriate sequence
	switch (which) {
	  
		// problems with SD card or finding the file
		case MSG_NO_SD_CARD:						blink(errorLED, noLED, 1, 2); 		break;
		case MSG_CANNOT_OPEN_FILE:					blink(errorLED, noLED, 2, 2); 		break;
	  
		// problems reading the .hex file
		case MSG_LINE_TOO_LONG:						blink(errorLED, workingLED, 1, 2); 	break;
		case MSG_LINE_TOO_SHORT:					blink(errorLED, workingLED, 2, 2); 	break;
		case MSG_LINE_DOES_NOT_START_WITH_COLON:	blink(errorLED, workingLED, 3, 2); 	break;
		case MSG_INVALID_HEX_DIGITS:				blink(errorLED, workingLED, 4, 2); 	break;
		case MSG_BAD_SUMCHECK:						blink(errorLED, workingLED, 5, 2); 	break;
		case MSG_LINE_NOT_EXPECTED_LENGTH:			blink(errorLED, workingLED, 6, 2); 	break;
		case MSG_UNKNOWN_RECORD_TYPE:				blink(errorLED, workingLED, 7, 2); 	break;
		case MSG_NO_END_OF_FILE_RECORD:				blink(errorLED, workingLED, 8, 2); 	break;
	  
		// problems with the file contents
		case MSG_FILE_TOO_LARGE_FOR_FLASH:			blink(errorLED, workingLED, 9, 2); 	break;
	  
		// problems programming the chip
		case MSG_CANNOT_ENTER_PROGRAMMING_MODE:		blink(errorLED, noLED, 3, 2); 		break;
		case MSG_NO_BOOTLOADER_FUSE:				blink(errorLED, noLED, 4, 2); 		break;
		case MSG_CANNOT_FIND_SIGNATURE:				blink(errorLED, noLED, 5, 2); 		break;
		case MSG_UNRECOGNIZED_SIGNATURE:			blink(errorLED, noLED, 6, 2); 		break;
		case MSG_BAD_START_ADDRESS:					blink(errorLED, noLED, 7, 2); 		break;
		case MSG_VERIFICATION_ERROR:				blink(errorLED, noLED, 8, 2); 		break;
		case MSG_FLASHED_OK:						blink(readyLED, noLED, 3, 3); 		break;
	  
		default:									blink(errorLED, 10, 10);  			break;
			
	}  // end of switch on which message
	
}
bool writeFlashContents(void) {
	
  errors = 0;
  
  if (chooseInputFile()) return false;

  // ensure back in programming mode
  if (!startProgramming ()) return false;

  // now commit to flash
  if (readHexFile(name, writeToFlash)) return false;

  // turn ready LED on during verification
  digitalWrite (readyLED, HIGH);

  // verify
  if (readHexFile(name, verifyFlash)) return false;

  // now fix up fuses so we can boot
  if (errors == 0) updateFuses (true);
	
  return errors == 0;
	
}
bool chooseInputFile(void) {
 
	if (readHexFile(name, checkFile)) {
		return true;  // error, don't attempt to write
	}
  
	// check file would fit into device memory
	if (highestAddress > currentSignature.flashSize) {
		ShowMessage (MSG_FILE_TOO_LARGE_FOR_FLASH);
		return true;
	}
  
	// check start address makes sense
	if (updateFuses (false)) {
		return true;
	}
	
	return false;
	
}
bool updateFuses (const bool writeIt) {
  
	unsigned long addr;
	unsigned int  len;
  
	byte fusenumber = currentSignature.fuseWithBootloaderSize;
  
	// if no fuse, can't change it
	if (fusenumber == 0xFF) return false;
	
	addr = currentSignature.flashSize;
	len = currentSignature.baseBootSize;
	
	if (lowestAddress == 0) {
		// don't use bootloader
		fuses[fusenumber] |= 1;
	} else {
		
		byte newval = 0xFF;
	
		if (lowestAddress == (addr - len))
			newval = 3;
		else if (lowestAddress == (addr - len * 2))
			newval = 2;
		else if (lowestAddress == (addr - len * 4))
			newval = 1;
		else if (lowestAddress == (addr - len * 8))
			newval = 0;
		else {
			ShowMessage (MSG_BAD_START_ADDRESS);
			return true;
		}
	  
		if (newval != 0xFF) {
			newval <<= 1;
			fuses[fusenumber] &= ~0x07;   // also program (clear) "boot into bootloader" bit
			fuses[fusenumber] |= newval;
		}  // if valid
		
	}  // if not address 0
  
	if (writeIt) {
		writeFuse (fuses[fusenumber], fuseCommands[fusenumber]);
	}
	
	return false;
	
}
void writeFuse (const byte newValue, const byte instruction) {
	
	if (newValue == 0) return;  // ignore
  
	program (progamEnable, instruction, 0, newValue);
	pollUntilReady ();
	
}
void stopProgramming (void) {
  
	// turn off pull-ups
	PORTD = 0b00000000;

	// set everything back to inputs
	DDRD = 0b11110000;
	
}
bool startProgramming (void) {

	// Declare Variables
	uint8_t _Confirm;
	uint8_t _TimeOut = 0;

	// Set Reset Output
	pinMode (3, OUTPUT);
//	DDRD |= 0b00001000;

//delay(20000);

	// Set SCK Low
	PORTD &= 0b11111011;

	// Set SCK Output
	// Set MOSI Output
	DDRD = 0b00001110;

  





	// we are in sync if we get back programAcknowledge on the third byte
	do {
		
		// regrouping pause
		delay(100);

		// Set SCK Low
		PORTD &= 0b11111011;

		// then pulse reset, see page 309 of datasheet
		PORTD |= 0b00001000;
		delayMicroseconds(10);
		PORTD &= 0b11110111;

		delay (25);  // wait at least 20 mS

		BB_SPITransfer(0xAC);
		BB_SPITransfer(0x53);

		_Confirm = BB_SPITransfer (0);

		BB_SPITransfer (0);
	
		if (_Confirm != 0x53) if (_TimeOut++ >= ENTER_PROGRAMMING_ATTEMPTS) return(false);
	
	} while (_Confirm != 0x53);
	
	return true;  // entered programming mode OK
	
}
bool processLine (const char * pLine, const byte action) {
	
	if (*pLine++ != ':') {
		ShowMessage (MSG_LINE_DOES_NOT_START_WITH_COLON);
		return true;  // error
	}
  
	const int maxHexData = 40;
	byte hexBuffer [maxHexData];
	int bytesInLine = 0;
  
	if (action == checkFile) if (lineCount++ % 40 == 0) showProgress ();
	
	// convert entire line from ASCII into binary
	while (isxdigit (*pLine)) {
		
		// can't fit?
		if (bytesInLine >= maxHexData) {
			ShowMessage (MSG_LINE_TOO_LONG);
			return true;
		} // end if too long
	  
		if (hexConv (pLine, hexBuffer [bytesInLine++])) return true;
		
	}  // end of while
	
	if (bytesInLine < 5) {
		ShowMessage (MSG_LINE_TOO_SHORT);
		return true;
	}

  // sumcheck it
  
	byte sumCheck = 0;
	for (int i = 0; i < (bytesInLine - 1); i++) sumCheck += hexBuffer [i];
	
	// 2's complement
	sumCheck = ~sumCheck + 1;
  
	// check sumcheck
	if (sumCheck != hexBuffer [bytesInLine - 1]) {
		ShowMessage (MSG_BAD_SUMCHECK);
		return true;
	}
  
	// length of data (eg. how much to write to memory)
	byte len = hexBuffer [0];
  
	// the data length should be the number of bytes, less
	//   length / address (2) / transaction type / sumcheck
	if (len != (bytesInLine - 5)) {
		ShowMessage (MSG_LINE_NOT_EXPECTED_LENGTH);
		return true;
	}
	
	// two bytes of address
	unsigned long addrH = hexBuffer [1];
	unsigned long addrL = hexBuffer [2];
	unsigned long addr = addrL | (addrH << 8);
  
	byte recType = hexBuffer [3];

	switch (recType) {
	
			// stuff to be written to memory
		case hexDataRecord:
			
			lowestAddress  = min (lowestAddress, addr + extendedAddress);
			highestAddress = max (lowestAddress, addr + extendedAddress + len - 1);
			bytesWritten += len;
	
			switch (action) {
				case checkFile:  // nothing much to do, we do the checks anyway
					break;
		  
				case verifyFlash:
					verifyData (addr + extendedAddress, &hexBuffer [4], len);
					break;
		
				case writeToFlash:
					writeData (addr + extendedAddress, &hexBuffer [4], len);
					break;
					
			} // end of switch on action
			break;
  
			// end of data
		case hexEndOfFile:
			gotEndOfFile = true;
			break;
  
			// we are setting the high-order byte of the address
		case hexExtendedSegmentAddressRecord:
			extendedAddress = ((unsigned long) hexBuffer [4]) << 12;
			break;

			// ignore these, who cares?
		case hexStartSegmentAddressRecord:
		case hexExtendedLinearAddressRecord:
		case hexStartLinearAddressRecord:
			break;
		
		default:
			ShowMessage (MSG_UNKNOWN_RECORD_TYPE);
			return true;
			
	}  // end of switch on recType
	
	return false;
	
}
void verifyData (const unsigned long addr, const byte * pData, const int length) {
  
	// check each byte
	for (int i = 0; i < length; i++) {
		
		unsigned long thisPage = (addr + i) & pagemask;
		
		// page changed? show progress
		if (thisPage != oldPage && oldPage != NO_PAGE) showProgress ();
		
		// now this is the current page
		oldPage = thisPage;
	  
		byte found = readFlash (addr + i);
		byte expected = pData [i];
		
		if (found != expected) errors++;
		
	}  // end of for
	
}
void writeData (const unsigned long addr, const byte * pData, const int length) {
	
	// write each byte
	for (int i = 0; i < length; i++) {
		
		unsigned long thisPage = (addr + i) & pagemask;
		
		// page changed? commit old one
		if (thisPage != oldPage && oldPage != NO_PAGE) commitPage (oldPage);
		
		// now this is the current page
		oldPage = thisPage;
		
		// put byte into work buffer
		writeFlash (addr + i, pData [i]);
		
	}  // end of for
	
}
void commitPage (unsigned long addr) {
  
	addr >>= 1;  // turn into word address
  
	// set the extended (most significant) address byte if necessary
	byte MSB = (addr >> 16) & 0xFF;
	
	if (MSB != lastAddressMSB) {
		
		program (loadExtendedAddressByte, 0, MSB);
		lastAddressMSB = MSB;
		
	}  // end if different MSB
	
	showProgress ();
  
	program (writeProgramMemory, highByte (addr), lowByte (addr));
	pollUntilReady ();
  
	clearPage();  // clear ready for next page full
	
}
void clearPage (void) {
	
	unsigned int len = currentSignature.pageSize;
	
	for (unsigned int i = 0; i < len; i++) writeFlash (i, 0xFF);
	
}
void showProgress (void) {
	
	digitalWrite (workingLED, ! digitalRead (workingLED));
	
}
void pollUntilReady (void) {
	
	if (currentSignature.timedWrites) delay (10);  // at least 2 x WD_FLASH which is 4.5 mS
	else {
		
		while ((program (pollReady) & 1) == 1) {}  // wait till ready
		
	}  // end of if
	
}
bool hexConv (const char * (& pStr), byte & b) {

	if (!isxdigit (pStr [0]) || !isxdigit (pStr [1])) {
		
		ShowMessage (MSG_INVALID_HEX_DIGITS);
		return true;
		
	} // end not hex
  
	b = *pStr++ - '0';
	
	if (b > 9) b -= 7;
  
	// high-order nybble
	b <<= 4;
  
	byte b1 = *pStr++ - '0';
	
	if (b1 > 9) b1 -= 7;
	
	b |= b1;
  
	return false;  // OK
	
}
void writeFlash (unsigned long addr, const byte data) {
	
	byte high = (addr & 1) ? 0x08 : 0;  // set if high byte wanted
	addr >>= 1;  // turn into word address
	program (loadProgramMemory | high, 0, lowByte (addr), data);
	
}

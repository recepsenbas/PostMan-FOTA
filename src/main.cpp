
// fixed file name to read from SD card (root directory)
const char wantedFile[] = "/fw.hex";


// the three "status" LEDs
const int errorLED = 17;
const int readyLED = 16;
const int workingLED = 15;

const int noLED = -1; // must be int! - can be negative

// status "messages"
typedef enum {
	MSG_NO_SD_CARD,						// cannot open SD card
	MSG_CANNOT_OPEN_FILE,				// canoot open file 'wantedFile' (above)
	MSG_LINE_TOO_LONG,					// line on disk too long to read
	MSG_LINE_TOO_SHORT,					// line too short to be valid
	MSG_LINE_DOES_NOT_START_WITH_COLON, // line does not start with a colon
	MSG_INVALID_HEX_DIGITS,				// invalid hex where there should be hex
	MSG_BAD_SUMCHECK,					// line fails sumcheck
	MSG_LINE_NOT_EXPECTED_LENGTH,		// record not length expected
	MSG_UNKNOWN_RECORD_TYPE,			// record type not known
	MSG_NO_END_OF_FILE_RECORD,			// no 'end of file' at end of file
	MSG_FILE_TOO_LARGE_FOR_FLASH,		// file will not fit into flash

	MSG_CANNOT_ENTER_PROGRAMMING_MODE, // cannot program target chip
	MSG_NO_BOOTLOADER_FUSE,			   // chip does not have bootloader
	MSG_CANNOT_FIND_SIGNATURE,		   // cannot find chip signature
	MSG_UNRECOGNIZED_SIGNATURE,		   // signature not known
	MSG_BAD_START_ADDRESS,			   // file start address invalid
	MSG_VERIFICATION_ERROR,			   // verification error after programming
	MSG_FLASHED_OK,					   // flashed OK
} msgType;

// for SDFat library see: https://github.com/greiman/SdFat

#include <SdFat.h>
#include <EEPROM.h>

const char Version[] = "1.25h";

const unsigned int ENTER_PROGRAMMING_ATTEMPTS = 10;

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




// control speed of programming
const byte BB_DELAY_MICROSECONDS = 6;

const unsigned long NO_PAGE = 0xFFFFFFFF;
const int MAX_FILENAME = 13;


// actions to take
enum
{
	checkFile,
	verifyFlash,
	writeToFlash,
};

// file system object
SdFat sd;

// copy of fuses/lock bytes found for this processor
byte fuses[5];

// meaning of bytes in above array
enum
{
	lowFuse,
	highFuse,
	extFuse,
	lockByte,
	calibrationByte
};

// structure to hold signature and other relevant data about each chip
typedef struct
{
	byte sig[3];
	const char *desc;
	unsigned long flashSize;
	unsigned int baseBootSize;
	unsigned long pageSize;		 // bytes
	byte fuseWithBootloaderSize; // ie. one of: lowFuse, highFuse, extFuse
	byte timedWrites;			 // if pollUntilReady won't work by polling the chip
} signatureType;

const unsigned long kb = 1024;
const byte NO_FUSE = 0xFF;

// see Atmega datasheets
const signatureType signatures[] PROGMEM =
	{
		//     signature        description   flash size   bootloader  flash  fuse
		//                                                     size    page    to
		//                                                             size   change

		// Attiny84 family
		{{0x1E, 0x91, 0x0B}, "ATtiny24", 2 * kb, 0, 32, NO_FUSE, false},
		{{0x1E, 0x92, 0x07}, "ATtiny44", 4 * kb, 0, 64, NO_FUSE, false},
		{{0x1E, 0x93, 0x0C}, "ATtiny84", 8 * kb, 0, 64, NO_FUSE, false},

		// Attiny85 family
		{{0x1E, 0x91, 0x08}, "ATtiny25", 2 * kb, 0, 32, NO_FUSE, false},
		{{0x1E, 0x92, 0x06}, "ATtiny45", 4 * kb, 0, 64, NO_FUSE, false},
		{{0x1E, 0x93, 0x0B}, "ATtiny85", 8 * kb, 0, 64, NO_FUSE, false},

		// Atmega328 family
		{{0x1E, 0x92, 0x0A}, "ATmega48PA", 4 * kb, 0, 64, NO_FUSE, false},
		{{0x1E, 0x93, 0x0F}, "ATmega88PA", 8 * kb, 256, 128, extFuse, false},
		{{0x1E, 0x94, 0x0B}, "ATmega168PA", 16 * kb, 256, 128, extFuse, false},
		{{0x1E, 0x95, 0x0F}, "ATmega328P", 32 * kb, 512, 128, highFuse, false},

		// Atmega644 family
		{{0x1E, 0x94, 0x0A}, "ATmega164P", 16 * kb, 256, 128, highFuse, false},
		{{0x1E, 0x95, 0x08}, "ATmega324P", 32 * kb, 512, 128, highFuse, false},
		{{0x1E, 0x96, 0x0A}, "ATmega644P", 64 * kb, 1 * kb, 256, highFuse, false},

		// Atmega2560 family
		{{0x1E, 0x96, 0x08}, "ATmega640", 64 * kb, 1 * kb, 256, highFuse, false},
		{{0x1E, 0x97, 0x03}, "ATmega1280", 128 * kb, 1 * kb, 256, highFuse, false},
		{{0x1E, 0x97, 0x04}, "ATmega1281", 128 * kb, 1 * kb, 256, highFuse, false},
		{{0x1E, 0x98, 0x01}, "ATmega2560", 256 * kb, 1 * kb, 256, highFuse, false},

		{{0x1E, 0x98, 0x02}, "ATmega2561", 256 * kb, 1 * kb, 256, highFuse, false},

		// AT90USB family
		{{0x1E, 0x93, 0x82}, "At90USB82", 8 * kb, 512, 128, highFuse, false},
		{{0x1E, 0x94, 0x82}, "At90USB162", 16 * kb, 512, 128, highFuse, false},

		// Atmega32U2 family
		{{0x1E, 0x93, 0x89}, "ATmega8U2", 8 * kb, 512, 128, highFuse, false},
		{{0x1E, 0x94, 0x89}, "ATmega16U2", 16 * kb, 512, 128, highFuse, false},
		{{0x1E, 0x95, 0x8A}, "ATmega32U2", 32 * kb, 512, 128, highFuse, false},

		// Atmega32U4 family -  (datasheet is wrong about flash page size being 128 words)
		{{0x1E, 0x94, 0x88}, "ATmega16U4", 16 * kb, 512, 128, highFuse, false},
		{{0x1E, 0x95, 0x87}, "ATmega32U4", 32 * kb, 512, 128, highFuse, false},

		// ATmega1284P family
		{{0x1E, 0x97, 0x05}, "ATmega1284P", 128 * kb, 1 * kb, 256, highFuse, false},

		// ATtiny4313 family
		{{0x1E, 0x91, 0x0A}, "ATtiny2313A", 2 * kb, 0, 32, NO_FUSE, false},
		{{0x1E, 0x92, 0x0D}, "ATtiny4313", 4 * kb, 0, 64, NO_FUSE, false},

		// ATtiny13 family
		{{0x1E, 0x90, 0x07}, "ATtiny13A", 1 * kb, 0, 32, NO_FUSE, false},

		// Atmega8A family
		{{0x1E, 0x93, 0x07}, "ATmega8A", 8 * kb, 256, 64, highFuse, true},

		// ATmega64rfr2 family
		{{0x1E, 0xA6, 0x02}, "ATmega64rfr2", 256 * kb, 1 * kb, 256, highFuse, false},
		{{0x1E, 0xA7, 0x02}, "ATmega128rfr2", 256 * kb, 1 * kb, 256, highFuse, false},
		{{0x1E, 0xA8, 0x02}, "ATmega256rfr2", 256 * kb, 1 * kb, 256, highFuse, false},

}; // end of signatures

char name[MAX_FILENAME] = {0}; // current file name

// number of items in an array
#define NUMITEMS(arg) ((unsigned int)(sizeof(arg) / sizeof(arg[0])))

// programming commands to send via SPI to the chip
enum
{
	progamEnable = 0xAC,

	// writes are preceded by progamEnable
	chipErase = 0x80,
	writeLockByte = 0xE0,
	writeLowFuseByte = 0xA0,
	writeHighFuseByte = 0xA8,
	writeExtendedFuseByte = 0xA4,

	pollReady = 0xF0,

	programAcknowledge = 0x53,

	readSignatureByte = 0x30,
	readCalibrationByte = 0x38,

	readLowFuseByte = 0x50,
	readLowFuseByteArg2 = 0x00,
	readExtendedFuseByte = 0x50,
	readExtendedFuseByteArg2 = 0x08,
	readHighFuseByte = 0x58,
	readHighFuseByteArg2 = 0x08,
	readLockByte = 0x58,
	readLockByteArg2 = 0x00,

	readProgramMemory = 0x20,
	writeProgramMemory = 0x4C,
	loadExtendedAddressByte = 0x4D,
	loadProgramMemory = 0x40,

}; // end of enum

// which program instruction writes which fuse
const byte fuseCommands[4] = {writeLowFuseByte, writeHighFuseByte, writeExtendedFuseByte, writeLockByte};

// types of record in .hex file
enum
{
	hexDataRecord,					 // 00
	hexEndOfFile,					 // 01
	hexExtendedSegmentAddressRecord, // 02
	hexStartSegmentAddressRecord,	 // 03
	hexExtendedLinearAddressRecord,	 // 04
	hexStartLinearAddressRecord		 // 05
};


volatile boolean fired = false;

// blink one or two LEDs for "times" times, with a delay of "interval". Wait a second and do it again "repeat" times.
void blink(const int whichLED1,
		   const int whichLED2,
		   const byte times = 1,
		   const unsigned long repeat = 1,
		   const unsigned long interval = 200)
{
	for (unsigned long i = 0; i < repeat; i++)
	{
		for (byte j = 0; j < times; j++)
		{
			digitalWrite(whichLED1, HIGH);
			if (whichLED2 != noLED)
				digitalWrite(whichLED2, HIGH);
			delay(interval);
			digitalWrite(whichLED1, LOW);
			if (whichLED2 != noLED)
				digitalWrite(whichLED2, LOW);
			delay(interval);
		} // end of for loop
		if (i < (repeat - 1))
			delay(1000);
	} // end of repeating the sequence
} // end of blink

void ShowMessage(const byte which)
{
	// first turn off all LEDs
	digitalWrite(errorLED, LOW);
	digitalWrite(workingLED, LOW);
	digitalWrite(readyLED, LOW);

	// now flash an appropriate sequence
	switch (which)
	{
	// problems with SD card or finding the file
	case MSG_NO_SD_CARD:
		blink(errorLED, noLED, 1, 5);
		break;
	case MSG_CANNOT_OPEN_FILE:
		blink(errorLED, noLED, 2, 5);
		break;

	// problems reading the .hex file
	case MSG_LINE_TOO_LONG:
		blink(errorLED, workingLED, 1, 5);
		break;
	case MSG_LINE_TOO_SHORT:
		blink(errorLED, workingLED, 2, 5);
		break;
	case MSG_LINE_DOES_NOT_START_WITH_COLON:
		blink(errorLED, workingLED, 3, 5);
		break;
	case MSG_INVALID_HEX_DIGITS:
		blink(errorLED, workingLED, 4, 5);
		break;
	case MSG_BAD_SUMCHECK:
		blink(errorLED, workingLED, 5, 5);
		break;
	case MSG_LINE_NOT_EXPECTED_LENGTH:
		blink(errorLED, workingLED, 6, 5);
		break;
	case MSG_UNKNOWN_RECORD_TYPE:
		blink(errorLED, workingLED, 7, 5);
		break;
	case MSG_NO_END_OF_FILE_RECORD:
		blink(errorLED, workingLED, 8, 5);
		break;

	// problems with the file contents
	case MSG_FILE_TOO_LARGE_FOR_FLASH:
		blink(errorLED, workingLED, 9, 5);
		break;

	// problems programming the chip
	case MSG_CANNOT_ENTER_PROGRAMMING_MODE:
		blink(errorLED, noLED, 3, 5);
		break;
	case MSG_NO_BOOTLOADER_FUSE:
		blink(errorLED, noLED, 4, 5);
		break;
	case MSG_CANNOT_FIND_SIGNATURE:
		blink(errorLED, noLED, 5, 5);
		break;
	case MSG_UNRECOGNIZED_SIGNATURE:
		blink(errorLED, noLED, 6, 5);
		break;
	case MSG_BAD_START_ADDRESS:
		blink(errorLED, noLED, 7, 5);
		break;
	case MSG_VERIFICATION_ERROR:
		blink(errorLED, noLED, 8, 5);
		break;
	case MSG_FLASHED_OK:
		blink(readyLED, noLED, 3, 10);
		break;

	default:
		blink(errorLED, 10, 10);
		break; // unknown error
	}		   // end of switch on which message
} // end of ShowMessage

// Bit Banged SPI transfer
byte BB_SPITransfer(byte c)
{
	byte bit;

	for (bit = 0; bit < 8; bit++)
	{
		// write MOSI on falling edge of previous clock
		if (c & 0x80)
			BB_MOSI_PORT |= bit(BB_MOSI_BIT);
		else
			BB_MOSI_PORT &= ~bit(BB_MOSI_BIT);
		c <<= 1;

		// read MISO
		c |= (BB_MISO_PORT & bit(BB_MISO_BIT)) != 0;

		// clock high
		BB_SCK_PORT |= bit(BB_SCK_BIT);

		// delay between rise and fall of clock
		delayMicroseconds(BB_DELAY_MICROSECONDS);

		// clock low
		BB_SCK_PORT &= ~bit(BB_SCK_BIT);

		// delay between rise and fall of clock
		delayMicroseconds(BB_DELAY_MICROSECONDS);
	}

	return c;
} // end of BB_SPITransfer

// if signature found in above table, this is its index
int foundSig = -1;
byte lastAddressMSB = 0;
// copy of current signature entry for matching processor
signatureType currentSignature;

// execute one programming instruction ... b1 is command, b2, b3, b4 are arguments
//  processor may return a result on the 4th transfer, this is returned.
byte program(const byte b1, const byte b2 = 0, const byte b3 = 0, const byte b4 = 0) {

	BB_SPITransfer(b1);
	BB_SPITransfer(b2);
	BB_SPITransfer(b3);
	byte b = BB_SPITransfer(b4);
	return b;

} // end of program

// read a byte from flash memory
byte readFlash(unsigned long addr)
{
	byte high = (addr & 1) ? 0x08 : 0; // set if high byte wanted
	addr >>= 1;						   // turn into word address

	// set the extended (most significant) address byte if necessary
	byte MSB = (addr >> 16) & 0xFF;
	if (MSB != lastAddressMSB)
	{
		program(loadExtendedAddressByte, 0, MSB);
		lastAddressMSB = MSB;
	} // end if different MSB

	return program(readProgramMemory | high, highByte(addr), lowByte(addr));
} // end of readFlash

// write a byte to the flash memory buffer (ready for committing)
void writeFlash(unsigned long addr, const byte data)
{
	byte high = (addr & 1) ? 0x08 : 0; // set if high byte wanted
	addr >>= 1;						   // turn into word address
	program(loadProgramMemory | high, 0, lowByte(addr), data);
} // end of writeFlash

// convert two hex characters into a byte
//    returns true if error, false if OK
bool hexConv(const char *(&pStr), byte &b)
{

	if (!isxdigit(pStr[0]) || !isxdigit(pStr[1]))
	{
		ShowMessage(MSG_INVALID_HEX_DIGITS);
		return true;
	} // end not hex

	b = *pStr++ - '0';
	if (b > 9)
		b -= 7;

	// high-order nybble
	b <<= 4;

	byte b1 = *pStr++ - '0';
	if (b1 > 9)
		b1 -= 7;

	b |= b1;

	return false; // OK
} // end of hexConv

// poll the target device until it is ready to be programmed
void pollUntilReady()
{
	if (currentSignature.timedWrites)
		delay(10); // at least 2 x WD_FLASH which is 4.5 mS
	else
	{
		while ((program(pollReady) & 1) == 1)
		{
		} // wait till ready
	}	  // end of if
} // end of pollUntilReady

unsigned long pagesize;
unsigned long pagemask;
unsigned long oldPage;
unsigned int progressBarCount;

// shows progress, toggles working LED
void showProgress()
{
	digitalWrite(workingLED, !digitalRead(workingLED));
} // end of showProgress

// clear entire temporary page to 0xFF in case we don't write to all of it
void clearPage()
{
	unsigned int len = currentSignature.pageSize;
	for (unsigned int i = 0; i < len; i++)
		writeFlash(i, 0xFF);
} // end of clearPage

// commit page to flash memory
void commitPage(unsigned long addr)
{
	addr >>= 1; // turn into word address

	// set the extended (most significant) address byte if necessary
	byte MSB = (addr >> 16) & 0xFF;
	if (MSB != lastAddressMSB)
	{
		program(loadExtendedAddressByte, 0, MSB);
		lastAddressMSB = MSB;
	} // end if different MSB

	showProgress();

	program(writeProgramMemory, highByte(addr), lowByte(addr));
	pollUntilReady();

	clearPage(); // clear ready for next page full
} // end of commitPage

// write data to temporary buffer, ready for committing
void writeData(const unsigned long addr, const byte *pData, const int length)
{
	// write each byte
	for (int i = 0; i < length; i++)
	{
		unsigned long thisPage = (addr + i) & pagemask;
		// page changed? commit old one
		if (thisPage != oldPage && oldPage != NO_PAGE)
			commitPage(oldPage);
		// now this is the current page
		oldPage = thisPage;
		// put byte into work buffer
		writeFlash(addr + i, pData[i]);
	} // end of for

} // end of writeData

// count errors
unsigned int errors;

void verifyData(const unsigned long addr, const byte *pData, const int length)
{
	// check each byte
	for (int i = 0; i < length; i++)
	{
		unsigned long thisPage = (addr + i) & pagemask;
		// page changed? show progress
		if (thisPage != oldPage && oldPage != NO_PAGE)
			showProgress();
		// now this is the current page
		oldPage = thisPage;

		byte found = readFlash(addr + i);
		byte expected = pData[i];
		if (found != expected)
			errors++;
	} // end of for

} // end of verifyData

bool gotEndOfFile;
unsigned long extendedAddress;

unsigned long lowestAddress;
unsigned long highestAddress;
unsigned long bytesWritten;
unsigned int lineCount;

/*
  Line format:

  :nnaaaatt(data)ss

  Where:
  :      = a colon

  (All of below in hex format)

  nn     = length of data part
  aaaa   = address (eg. where to write data)
  tt     = transaction type
		   00 = data
		   01 = end of file
		   02 = extended segment address (changes high-order byte of the address)
		   03 = start segment address *
		   04 = linear address *
		   05 = start linear address *
  (data) = variable length data
  ss     = sumcheck

			  We don't use these

*/

// returns true if error, false if OK
bool processLine(const char *pLine, const byte action)
{
	if (*pLine++ != ':')
	{
		ShowMessage(MSG_LINE_DOES_NOT_START_WITH_COLON);
		return true; // error
	}

	const int maxHexData = 40;
	byte hexBuffer[maxHexData];
	int bytesInLine = 0;

	if (action == checkFile)
		if (lineCount++ % 40 == 0)
			showProgress();

	// convert entire line from ASCII into binary
	while (isxdigit(*pLine))
	{
		// can't fit?
		if (bytesInLine >= maxHexData)
		{
			ShowMessage(MSG_LINE_TOO_LONG);
			return true;
		} // end if too long

		if (hexConv(pLine, hexBuffer[bytesInLine++]))
			return true;
	} // end of while

	if (bytesInLine < 5)
	{
		ShowMessage(MSG_LINE_TOO_SHORT);
		return true;
	}

	// sumcheck it

	byte sumCheck = 0;
	for (int i = 0; i < (bytesInLine - 1); i++)
		sumCheck += hexBuffer[i];

	// 2's complement
	sumCheck = ~sumCheck + 1;

	// check sumcheck
	if (sumCheck != hexBuffer[bytesInLine - 1])
	{
		ShowMessage(MSG_BAD_SUMCHECK);
		return true;
	}

	// length of data (eg. how much to write to memory)
	byte len = hexBuffer[0];

	// the data length should be the number of bytes, less
	//   length / address (2) / transaction type / sumcheck
	if (len != (bytesInLine - 5))
	{
		ShowMessage(MSG_LINE_NOT_EXPECTED_LENGTH);
		return true;
	}

	// two bytes of address
	unsigned long addrH = hexBuffer[1];
	unsigned long addrL = hexBuffer[2];

	unsigned long addr = addrL | (addrH << 8);

	byte recType = hexBuffer[3];

	switch (recType)
	{
	// stuff to be written to memory
	case hexDataRecord:
		lowestAddress = min(lowestAddress, addr + extendedAddress);
		highestAddress = max(lowestAddress, addr + extendedAddress + len - 1);
		bytesWritten += len;

		switch (action)
		{
		case checkFile: // nothing much to do, we do the checks anyway
			break;

		case verifyFlash:
			verifyData(addr + extendedAddress, &hexBuffer[4], len);
			break;

		case writeToFlash:
			writeData(addr + extendedAddress, &hexBuffer[4], len);
			break;
		} // end of switch on action
		break;

	// end of data
	case hexEndOfFile:
		gotEndOfFile = true;
		break;

	// we are setting the high-order byte of the address
	case hexExtendedSegmentAddressRecord:
		extendedAddress = ((unsigned long)hexBuffer[4]) << 12;
		break;

	// ignore these, who cares?
	case hexStartSegmentAddressRecord:
	case hexExtendedLinearAddressRecord:
	case hexStartLinearAddressRecord:
		break;

	default:
		ShowMessage(MSG_UNKNOWN_RECORD_TYPE);
		return true;
	} // end of switch on recType

	return false;
} // end of processLine

//------------------------------------------------------------------------------
// returns true if error, false if OK
bool readHexFile(const char *fName, const byte action)
{

	const int maxLine = 80;
	char buffer[maxLine];
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


	PORTB |= 0b00000001;
	ifstream sdin("fw.hex");

	// check for open error
	if (!sdin.is_open())
	{
		ShowMessage(MSG_CANNOT_OPEN_FILE);
		return true;
	}

	switch (action)
	{
	case checkFile:
		break;

	case verifyFlash:
		break;

	case writeToFlash:
		program(progamEnable, chipErase); // erase it
		delay(20);						  // for Atmega8
		pollUntilReady();
		clearPage(); // clear temporary page
		break;
	} // end of switch

	while (sdin.getline(buffer, maxLine))
	{
		lineNumber++;
		int count = sdin.gcount();
		if (sdin.fail())
		{
			ShowMessage(MSG_LINE_TOO_LONG);
			return true;
		} // end of fail (line too long?)

		// ignore empty lines
		if (count > 1)
		{
			if (processLine(buffer, action))
			{
				return true; // error
			}
		}
	} // end of while each line

	if (!gotEndOfFile)
	{
		ShowMessage(MSG_NO_END_OF_FILE_RECORD);
		return true;
	}

	switch (action)
	{
	case writeToFlash:
		// commit final page
		if (oldPage != NO_PAGE)
			commitPage(oldPage);
		break;

	case verifyFlash:
		if (errors > 0)
		{
			ShowMessage(MSG_VERIFICATION_ERROR);
			return true;
		} // end if
		break;

	case checkFile:
		break;
	} // end of switch

	return false;
} // end of readHexFile

// returns true if managed to enter programming mode
bool startProgramming()
{

	// ON Burn Buffer
	PORTB |= 0b00000001;

	byte confirm;
	pinMode(RESET, OUTPUT);
	digitalWrite(MSPIM_SCK, LOW);
	pinMode(MSPIM_SCK, OUTPUT);
	pinMode(BB_MOSI, OUTPUT);
	unsigned int timeout = 0;

	// we are in sync if we get back programAcknowledge on the third byte
	do {
		// regrouping pause
		delay(100);

		// ensure SCK low
		digitalWrite(MSPIM_SCK, LOW);

		// then pulse reset, see page 309 of datasheet
		digitalWrite(RESET, HIGH);
		delayMicroseconds(10); // pulse for at least 2 clock cycles
		digitalWrite(RESET, LOW);

		delay(25); // wait at least 20 mS
		BB_SPITransfer(progamEnable);
		BB_SPITransfer(programAcknowledge);
		confirm = BB_SPITransfer(0);
		BB_SPITransfer(0);

		if (confirm != programAcknowledge) {

			if (timeout++ >= ENTER_PROGRAMMING_ATTEMPTS) return false;

		} // end of not entered programming mode

	} while (confirm != programAcknowledge);

	return true; // entered programming mode OK
} // end of startProgramming

void stopProgramming()
{
	// turn off pull-ups
	digitalWrite(RESET, LOW);
	digitalWrite(MSPIM_SCK, LOW);
	digitalWrite(BB_MOSI, LOW);
	digitalWrite(BB_MISO, LOW);

	// set everything back to inputs
	pinMode(RESET, INPUT);
	pinMode(MSPIM_SCK, INPUT);
	pinMode(BB_MOSI, INPUT);
	pinMode(BB_MISO, INPUT);

} // end of stopProgramming

void getSignature()
{
	foundSig = -1;
	lastAddressMSB = 0;

	byte sig[3];
	for (byte i = 0; i < 3; i++)
	{
		sig[i] = program(readSignatureByte, 0, i);
	} // end for each signature byte

	for (unsigned int j = 0; j < NUMITEMS(signatures); j++)
	{
		memcpy_P(&currentSignature, &signatures[j], sizeof currentSignature);

		if (memcmp(sig, currentSignature.sig, sizeof sig) == 0)
		{
			foundSig = j;
			// make sure extended address is zero to match lastAddressMSB variable
			program(loadExtendedAddressByte, 0, 0);
			return;
		} // end of signature found
	}	  // end of for each signature

	ShowMessage(MSG_UNRECOGNIZED_SIGNATURE);
} // end of getSignature

void getFuseBytes()
{
	fuses[lowFuse] = program(readLowFuseByte, readLowFuseByteArg2);
	fuses[highFuse] = program(readHighFuseByte, readHighFuseByteArg2);
	fuses[extFuse] = program(readExtendedFuseByte, readExtendedFuseByteArg2);
	fuses[lockByte] = program(readLockByte, readLockByteArg2);
	fuses[calibrationByte] = program(readCalibrationByte);
} // end of getFuseBytes

// write specified value to specified fuse/lock byte
void writeFuse(const byte newValue, const byte instruction)
{
	if (newValue == 0)
		return; // ignore

	program(progamEnable, instruction, 0, newValue);
	pollUntilReady();
} // end of writeFuse

// returns true if error, false if OK
bool updateFuses(const bool writeIt)
{
	unsigned long addr;
	unsigned int len;

	byte fusenumber = currentSignature.fuseWithBootloaderSize;

	// if no fuse, can't change it
	if (fusenumber == NO_FUSE)
	{
		//    ShowMessage (MSG_NO_BOOTLOADER_FUSE);   // maybe this doesn't matter?
		return false; // ok return
	}

	addr = currentSignature.flashSize;
	len = currentSignature.baseBootSize;

	if (lowestAddress == 0)
	{
		// don't use bootloader
		fuses[fusenumber] |= 1;
	}
	else
	{
		byte newval = 0xFF;

		if (lowestAddress == (addr - len))
			newval = 3;
		else if (lowestAddress == (addr - len * 2))
			newval = 2;
		else if (lowestAddress == (addr - len * 4))
			newval = 1;
		else if (lowestAddress == (addr - len * 8))
			newval = 0;
		else
		{
			ShowMessage(MSG_BAD_START_ADDRESS);
			return true;
		}

		if (newval != 0xFF)
		{
			newval <<= 1;
			fuses[fusenumber] &= ~0x07; // also program (clear) "boot into bootloader" bit
			fuses[fusenumber] |= newval;
		} // if valid

	} // if not address 0

	if (writeIt)
	{
		writeFuse(fuses[fusenumber], fuseCommands[fusenumber]);
	}

	return false;
} // end of updateFuses

//------------------------------------------------------------------------------
//      SETUP
//------------------------------------------------------------------------------
void setup() {

	// Port B
	DDRB |= 0b00000011;
	PORTB &= 0b11111100;

	// ON Burn Buffer
	PORTB |= 0b00000001;

	// Boot Delay
	delay(500);

	pinMode(errorLED, OUTPUT);//Kırmızı
	pinMode(readyLED, OUTPUT);//Yeşil
	pinMode(workingLED, OUTPUT);//Mavi

	// initialize the SD card at SPI_HALF_SPEED to avoid bus errors with
	// breadboards.  use SPI_FULL_SPEED for better performance.
	if (!sd.begin(10)) {
		ShowMessage(MSG_NO_SD_CARD);
		PORTB |= 0b00000010;
		delay(200);
		PORTB &= 0b11111101;
	}
	
	

} // end of setup

// returns true if error, false if OK
bool chooseInputFile()
{

	if (readHexFile(name, checkFile))
	{
		return true; // error, don't attempt to write
	}

	// check file would fit into device memory
	if (highestAddress > currentSignature.flashSize)
	{
		ShowMessage(MSG_FILE_TOO_LARGE_FOR_FLASH);
		return true;
	}

	// check start address makes sense
	if (updateFuses(false))
	{
		return true;
	}

	return false;
} // end of chooseInputFile

// returns true if OK, false on error
bool writeFlashContents()
{

	errors = 0;

	if (chooseInputFile())
		return false;

#if CROSSROADS_PROGRAMMING_BOARD
	show7SegmentMessage("Pr");
#endif //  CROSSROADS_PROGRAMMING_BOARD

	// ensure back in programming mode
	if (!startProgramming())
		return false;

	// now commit to flash
	if (readHexFile(name, writeToFlash))
		return false;

#if CROSSROADS_PROGRAMMING_BOARD
	show7SegmentMessage("uF");
#endif //  CROSSROADS_PROGRAMMING_BOARD

	// turn ready LED on during verification
	digitalWrite(readyLED, HIGH);

	// verify
	if (readHexFile(name, verifyFlash))
		return false;

	// now fix up fuses so we can boot
	if (errors == 0)
		updateFuses(true);

	if(errors == 0){
		sd.remove("fw.hex");
		return true;
	}
} // end of writeFlashContents

//------------------------------------------------------------------------------
//      LOOP
//------------------------------------------------------------------------------
void loop() {

	if (!startProgramming()) {

		ShowMessage(MSG_CANNOT_ENTER_PROGRAMMING_MODE);
		return;

	} // end of could not enter programming mode


	getSignature();
	getFuseBytes();

	// don't have signature? don't proceed
	if (foundSig == -1)
	{
		ShowMessage(MSG_CANNOT_FIND_SIGNATURE);
		return;
	} // end of no signature

	digitalWrite(workingLED, HIGH);
	bool ok = writeFlashContents();
	digitalWrite(workingLED, LOW);
	digitalWrite(readyLED, LOW);
	stopProgramming();
	delay(500);

	if (ok)	{
		ShowMessage(MSG_FLASHED_OK);
	}

	// Done Signal [HIGH]
	PORTB |= 0b00000010;

	// Sleep Delay
	delay(200);

} // end of loop
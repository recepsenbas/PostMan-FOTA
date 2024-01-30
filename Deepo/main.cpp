/* *******************************************************************************
 *  Copyright (C) 2014-2021 Mehmet Gunce Akkoyun Can not be copied and/or
 *	distributed without the express permission of Mehmet Gunce Akkoyun.
 *
 *	Project				: B100AA Firmware Over The Air Programming
 *	Code Developer		: Mehmet Gunce Akkoyun (akkoyun@me.com)
 *	Revision			: 01.00.00
 *
 *********************************************************************************/

// Include Dependencies
#include "Includes.h"
#include "Config.h"
#include "Definition.h"

// Define Current Signature
Signature_Type Current_Signature;

// Define Found Fuse
uint8_t Fuses [5]; // copy of fuses/lock bytes found for this processor

// Digital SPI Data Transfer
uint8_t Digital_SPI_Transfer (byte c) {
	byte bit;

	for (bit = 0; bit < 8; bit++)
	{
		// write MOSI on falling edge of previous clock
		if (c & 0x80)
			Digital_MOSI_PORT |= bit(Digital_MOSI_PIN);
		else
			Digital_MOSI_PORT &= ~bit(Digital_MOSI_PIN);
		c <<= 1;

		// read MISO
		c |= (Digital_MISO_PORT & bit(Digital_MISO_PIN)) != 0;

		// clock high
		Digital_SCK_PORT |= bit(Digital_SCK_PIN);

		// delay between rise and fall of clock
		delayMicroseconds(6);

		// clock low
		Digital_SCK_PORT &= ~bit(Digital_SCK_PIN);

		// delay between rise and fall of clock
		delayMicroseconds(6);
	}

	return c;
}



// Digital SPI Start Programming
uint8_t Program (const uint8_t _Data_1, const uint8_t _Data_2 = 0, const uint8_t _Data_3 = 0, const uint8_t _Data_4 = 0) {

	// Stop Interrupts
	noInterrupts ();

	// Transfer Data
	Digital_SPI_Transfer (_Data_1);
	
	// Transfer Data
	Digital_SPI_Transfer (_Data_2);

	// Transfer Data
	Digital_SPI_Transfer (_Data_3);

	// Transfer Data
	uint8_t _Data = Digital_SPI_Transfer (_Data_4);

	// Start Interrupts
	interrupts ();

	// Return Data
	return _Data;
	
}

unsigned long pagesize, pagemask, oldPage, extendedAddress, lowestAddress, highestAddress, bytesWritten;
unsigned int progressBarCount, errors, lineCount;
bool gotEndOfFile;

// which program instruction writes which fuse
const byte fuseCommands [4] = { Command_Write_Low_Fuse_Byte, Command_Write_High_Fuse_Byte, Command_Write_Extended_Fuse_Byte, Command_Write_Lock_Byte };

//------------------------------------------------------------------------------
//      FUNCTIONS
//------------------------------------------------------------------------------
void blink(const int whichLED1, const int whichLED2, const byte times = 1, const unsigned long repeat = 1, const unsigned long interval = 200) {

	for (unsigned long i = 0; i < repeat; i++) {

		for (byte j = 0; j < times; j++) {

			digitalWrite(whichLED1, HIGH);
			if (whichLED2 != -1)
				digitalWrite(whichLED2, HIGH);
			delay(interval);
			digitalWrite(whichLED1, LOW);
			if (whichLED2 != -1)
				digitalWrite(whichLED2, LOW);
			delay(interval);
		} // end of for loop
		if (i < (repeat - 1))
			delay(1000);
	} // end of repeating the sequence

} // end of blink

// LED Show Progress
void LED_Show_Progress (void) {

	// Set Green LED HIGH
	LED_Green_PORT |= (1 << LED_Green_PIN);

	// Blink Delay
	delay (100);

	// Set Green LED LOW
	LED_Green_PORT &= ~(1 << LED_Green_PIN);

}

// Show Message
void Show_Message(const byte which) {
  
	// Set All LED Signals LOW
	PORTC &= 0b11110001;

	// now flash an appropriate sequence
	switch (which) {
	  
		// problems with SD card or finding the file
		case MSG_NO_SD_CARD:						blink(LED_Error, LED_No, 1, 2); 		break;
		case MSG_CANNOT_OPEN_FILE:					blink(LED_Error, LED_No, 2, 2); 		break;
	  
		// problems reading the .hex file
		case MSG_LINE_TOO_LONG:						blink(LED_Error, LED_Working, 1, 2); 	break;
		case MSG_LINE_TOO_SHORT:					blink(LED_Error, LED_Working, 2, 2); 	break;
		case MSG_LINE_DOES_NOT_START_WITH_COLON:	blink(LED_Error, LED_Working, 3, 2); 	break;
		case MSG_INVALID_HEX_DIGITS:				blink(LED_Error, LED_Working, 4, 2); 	break;
		case MSG_BAD_SUMCHECK:						blink(LED_Error, LED_Working, 5, 2); 	break;
		case MSG_LINE_NOT_EXPECTED_LENGTH:			blink(LED_Error, LED_Working, 6, 2); 	break;
		case MSG_UNKNOWN_RECORD_TYPE:				blink(LED_Error, LED_Working, 7, 2); 	break;
		case MSG_NO_END_OF_FILE_RECORD:				blink(LED_Error, LED_Working, 8, 2); 	break;
	  
		// problems with the file contents
		case MSG_FILE_TOO_LARGE_FOR_FLASH:			blink(LED_Error, LED_Working, 9, 2); 	break;
	  
		// problems programming the chip
		case MSG_CANNOT_ENTER_PROGRAMMING_MODE:		blink(LED_Error, LED_No, 3, 2); 		break;
		case MSG_NO_BOOTLOADER_FUSE:				blink(LED_Error, LED_No, 4, 2); 		break;
		case MSG_CANNOT_FIND_SIGNATURE:				blink(LED_Error, LED_No, 5, 2); 		break;
		case MSG_UNRECOGNIZED_SIGNATURE:			blink(LED_Error, LED_No, 6, 2); 		break;
		case MSG_BAD_START_ADDRESS:					blink(LED_Error, LED_No, 7, 2); 		break;
		case MSG_VERIFICATION_ERROR:				blink(LED_Error, LED_No, 8, 2); 		break;
		case MSG_FLASHED_OK:						blink(LED_Ready, LED_No, 3, 3); 		break;

		case MSG_FLASHED_TEST:						blink(LED_Ready, LED_Working, 3, 1); 	break;

		default:									blink(LED_Error, 10, 10);  			break;
			
	}  // end of switch on which message
	
}

// Get Fuse Bytes
void Get_Fuse_Bytes(void) {
	
	// Get Fuse Bytes
	Fuses[Low_Fuse] = Program(Command_Read_Low_Fuse_Byte, Command_Read_Low_Fuse_Byte_Arg2);
	Fuses[High_Fuse] = Program(Command_Read_High_Fuse_Byte, Command_Read_High_Fuse_Byte_Arg2);
	Fuses[Ext_Fuse] = Program(Command_Read_Extended_Fuse_Byte, Command_Read_Extended_Fuse_Byte_Arg2);
	Fuses[Lock_Byte] = Program(Command_Read_Lock_Byte, Command_Read_Lock_Byte_Arg2);
	Fuses[Calibration_Byte] = Program(Command_Read_Calibration_Byte);
	
}

// Get Signature
void Get_Signature(void) {
	
	// Clear Variables
	foundSig = -1;
	lastAddressMSB = 0;

	// Declare Signature
	uint8_t _Signature[3];

	// Get Signature	
	for (uint8_t i = 0; i < 3; i++) _Signature[i] = Program(Command_Read_Signature_Byte, 0, i);

	// Search for Signature
	for (uint16_t j = 0; j < NUMITEMS(Signatures); j++) {
		
		// Get MCU Signature
		memcpy_P(&Current_Signature, &Signatures [j], sizeof Current_Signature);
	
		// Control Signature
		if (memcmp(_Signature, Current_Signature.Signature, sizeof _Signature) == 0) {

			// Set Signature Found		
			foundSig = j;
			
			// make sure extended address is zero to match lastAddressMSB variable
			Program(Command_Load_Extended_Address_Byte, 0, 0);
			
			// End Function
			return;
			
		}
		
	}

	// Signature Error Message
	Show_Message (MSG_UNRECOGNIZED_SIGNATURE);
	
}







// Digital SPI Start Programming
bool Digital_SPI_Start_Programming (void) {

	// Set Digital RESET as OUTPUT
	Digital_Reset_DDR |= (1 << Digital_Reset_PIN);

	// Set Digital SCK LOW
	Digital_SCK_PORT &= ~(1 << Digital_SCK_PIN);

	// Set Digital SCK as OUTPUT
	Digital_SCK_DDR |= (1 << Digital_SCK_PIN);

	// Set Digital MOSI as OUTPUT
	Digital_MOSI_DDR |= (1 << Digital_MOSI_PIN);

	// Declare Try Counter
	uint8_t _Try_Counter = 0;

	// Declare Control Variable
	uint8_t _Control = 0;

	// we are in sync if we get back programAcknowledge on the third byte
	do {

		// Regrouping Pause
		delay (100);

		// Set Digital SCK LOW
		Digital_SCK_PORT &= ~(1 << Digital_SCK_PIN);

		// Set Digital RESET HIGH
		Digital_Reset_PORT |= (1 << Digital_Reset_PIN);

		// Pause
		delayMicroseconds (10);

		// Set Digital RESET LOW
		Digital_Reset_PORT &= ~(1 << Digital_Reset_PIN);

		// Pause
		delay (25);

		// Transfer Data
		Digital_SPI_Transfer (0xAC);

		// Transfer Data
		Digital_SPI_Transfer (0x53);

		// Read Data
		_Control = Digital_SPI_Transfer (0);

		// Transfer Data
		Digital_SPI_Transfer (0);

		// Control for Response	
		if (_Control != 0x53) {

			// Increment Try Counter
			_Try_Counter++;

			// Control for Try Counter
			if (_Try_Counter >= 50) return false;

		}

	} while (_Control != 0x53);

	// End Function
	return (true);

}

// Poll Until Ready
void Poll_Until_Ready (void) {
	
	if (Current_Signature.Timed_Writes) delay (10);  // at least 2 x WD_FLASH which is 4.5 mS
	else {
		
		while ((Program (Command_Poll_Ready) & 1) == 1) {}  // wait till ready
		
	}  // end of if
	
}

// Write Fuse
void Write_Fuse (const uint8_t _New_Value, const uint8_t _Instruction) {

	// Control for New Value	
	if (_New_Value == 0) return;

	// Write Fuse
	Program (Command_Progam_Enable, _Instruction, 0, _New_Value);

	// Poll Until Ready
	Poll_Until_Ready ();

}

// Update Fuses
bool Update_Fuses (const bool writeIt) {
  
	unsigned long addr;
	unsigned int  len;
  
	byte fusenumber = Current_Signature.Fuse_With_Bootloader_Size;
  
	// if no fuse, can't change it
	if (fusenumber == 0xFF) return false;
	
	addr = Current_Signature.Flash_Size;
	len = Current_Signature.Bootloader_Size;
	
	if (lowestAddress == 0) {
		// don't use bootloader
		Fuses[fusenumber] |= 1;
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
			Show_Message (MSG_BAD_START_ADDRESS);
			return true;
		}
	  
		if (newval != 0xFF) {
			newval <<= 1;
			Fuses[fusenumber] &= ~0x07;   // also program (clear) "boot into bootloader" bit
			Fuses[fusenumber] |= newval;
		}  // if valid
		
	}  // if not address 0
  
	// Write Fuses
	if (writeIt) Write_Fuse (Fuses[fusenumber], fuseCommands[fusenumber]);
	
	// End Function
	return false;
	
}

// Hex Conversion
bool HEX_Conv (const char * (& pStr), uint8_t & b) {

	// Check for Hex Digits
	if (!isxdigit (pStr [0]) || !isxdigit (pStr [1])) {

		// Show Message
		Show_Message (MSG_INVALID_HEX_DIGITS);

		// End Function
		return true;
		
	}

	b = *pStr++ - '0';
	if (b > 9) b -= 7;
	b <<= 4;
	uint8_t b1 = *pStr++ - '0';
	if (b1 > 9) b1 -= 7;
	b |= b1;

	// End Function
	return false;
	
}

// Write Flash
void Write_Flash (unsigned long addr, const byte data) {
	
	byte high = (addr & 1) ? 0x08 : 0;  // set if high byte wanted
	addr >>= 1;  // turn into word address
	Program (Command_Load_Program_Memory | high, 0, lowByte (addr), data);
	
}

// Clear Page
void Clear_Page (void) {
	
	unsigned int len = Current_Signature.Page_Size;
	
	for (unsigned int i = 0; i < len; i++) Write_Flash (i, 0xFF);
	
}

// Commit Page
void Commit_Page (unsigned long addr) {
  
	addr >>= 1;  // turn into word address
  
	// set the extended (most significant) address byte if necessary
	byte MSB = (addr >> 16) & 0xFF;
	
	if (MSB != lastAddressMSB) {

		// Get MSB
		Program (Command_Load_Extended_Address_Byte, 0, MSB);

		// Set Last Address MSB
		lastAddressMSB = MSB;
		
	}

	// show progress
	LED_Show_Progress ();

	// commit page
	Program (Command_Write_Program_Memory, highByte (addr), lowByte (addr));

	// poll until ready
	Poll_Until_Ready ();

	// clear page
	Clear_Page();

}

// Write Data
void Write_Data (const unsigned long addr, const byte * pData, const int length) {
	
	// write each byte
	for (int i = 0; i < length; i++) {
		
		unsigned long thisPage = (addr + i) & pagemask;
		
		// page changed? commit old one
		if (thisPage != oldPage && oldPage != 0xFFFFFFFF) Commit_Page (oldPage);
		
		// now this is the current page
		oldPage = thisPage;
		
		// put byte into work buffer
		Write_Flash (addr + i, pData [i]);
		
	} 
	
}

// Read Flash
uint8_t Read_Flash (unsigned long addr) {
	
	uint8_t high = (addr & 1) ? 0x08 : 0;  // set if high uint8_t wanted
	addr >>= 1;  // turn into word address

	// set the extended (most significant) address uint8_t if necessary
	uint8_t MSB = (addr >> 16) & 0xFF;
	
	if (MSB != lastAddressMSB) {
		
		Program (Command_Load_Extended_Address_Byte, 0, MSB);
		lastAddressMSB = MSB;
		
	}  // end if different MSB

	return Program (Command_Read_Program_Memory | high, highByte (addr), lowByte (addr));
	
}

// Verify Data
void Verify_Data (const unsigned long addr, const byte * pData, const int length) {
  
	// check each byte
	for (int i = 0; i < length; i++) {
		
		unsigned long thisPage = (addr + i) & pagemask;
		
		// page changed? show progress
		if (thisPage != oldPage && oldPage != 0xFFFFFFFF) LED_Show_Progress ();
		
		// now this is the current page
		oldPage = thisPage;
	  
		byte found = Read_Flash (addr + i);
		byte expected = pData [i];
		
		if (found != expected) errors++;
		
	}  // end of for
	
}

// Process Line
bool Process_Line (const char * pLine, const byte action) {

	// Check for : Character
	if (*pLine++ != ':') {

		// Show Message
		Show_Message (MSG_LINE_DOES_NOT_START_WITH_COLON);
		
		// End Function
		return true;

	}

	// Declare Buffer
	uint8_t _HEX_Buffer[40];

	// Declare Variables
	uint8_t _Bytes_In_Line = 0;

	// Show LED Progress
	if (action == Action_Check_File) if (lineCount++ % 40 == 0) LED_Show_Progress ();

	// Convert to Hex
	while (isxdigit (*pLine)) {
		
		// can't fit?
		if (_Bytes_In_Line >= 40) {
			
			// Show Message
			Show_Message (MSG_LINE_TOO_LONG);
			
			// End Function
			return true;

		}

		// Convert Hex
		if (HEX_Conv (pLine, _HEX_Buffer [_Bytes_In_Line++])) return true;
		
	}

	// Check for Short Line
	if (_Bytes_In_Line < 5) {
		
		// Show Message
		Show_Message (MSG_LINE_TOO_SHORT);
		
		// End Function
		return true;

	}

	// Declare Sum Check
	uint8_t _Sum_Check = 0;

	// Check Sum Check
	for (uint8_t i = 0; i < (_Bytes_In_Line - 1); i++) _Sum_Check += _HEX_Buffer [i];

	// 2's complement
	_Sum_Check = ~_Sum_Check + 1;
  
	// check sumcheck
	if (_Sum_Check != _HEX_Buffer [_Bytes_In_Line - 1]) {

		// Show Message
		Show_Message (MSG_BAD_SUMCHECK);

		// End Function
		return true;

	}
  
	// length of data (eg. how much to write to memory)
	uint8_t _Len = _HEX_Buffer [0];
  
	// the data length should be the number of bytes, less
	//   length / address (2) / transaction type / sumcheck
	if (_Len != (_Bytes_In_Line - 5)) {

		// Show Message
		Show_Message (MSG_LINE_NOT_EXPECTED_LENGTH);

		// End Function
		return true;

	}
	
	// two bytes of address
	uint8_t _Address_High = _HEX_Buffer [1];
	uint8_t _Address_Low = _HEX_Buffer [2];
	uint16_t _Address = _Address_Low | (_Address_High << 8);

	// Record Type
	uint8_t _Record_Type = _HEX_Buffer [3];

	// Switch Record Type
	switch (_Record_Type) {
	
		// Data Record
		case HEX_Data_Record: {

			lowestAddress  = min (lowestAddress, _Address + extendedAddress);
			highestAddress = max (lowestAddress, _Address + extendedAddress + _Len - 1);
			bytesWritten += _Len;
	
			switch (action) {
				case Action_Check_File:  // nothing much to do, we do the checks anyway
					break;
		  
				case Action_Verify_Flash:
					Verify_Data (_Address + extendedAddress, &_HEX_Buffer [4], _Len);
					break;
		
				case Action_Write_To_Flash:
					Write_Data (_Address + extendedAddress, &_HEX_Buffer [4], _Len);
					break;
					
			} // end of switch on action
			break;

		}
  
		// end of data
		case HEX_End_Of_File: {

			// Set End Of File
			gotEndOfFile = true;
			
			// End Function
			break;

		}
  
		// we are setting the high-order byte of the address
		case HEX_Extended_Segment_Address_Record: {

			extendedAddress = ((unsigned long) _HEX_Buffer [4]) << 12;
			break;

		}

		// Default		
		default: {
			
			// Show Message
			Show_Message (MSG_UNKNOWN_RECORD_TYPE);
			
			// End Function
			return true;

		}

	}

	// End Function
	return false;

}

// Read Hex File
bool Read_Hex_File(const char * _File_Name, const uint8_t _Action) {

	// Declare Buffer
	char _Buffer[80];

	// Clear Buffer
	memset (_Buffer, '\0', 80);

	// Declare File
	ifstream sdin (_File_Name);

	// Clear Variables
	gotEndOfFile = false;
	extendedAddress = 0;
	errors = 0;
	lowestAddress = 0xFFFFFFFF;
	highestAddress = 0;
	bytesWritten = 0;
	progressBarCount = 0;
	pagesize = Current_Signature.Page_Size;
	pagemask = ~(pagesize - 1);
	oldPage = 0xFFFFFFFF;
	
	// check for open error
	if (!sdin.is_open()) {

		// Show Message
		Show_Message(MSG_CANNOT_OPEN_FILE);

		// Set Burn Enable Pin LOW
		Burn_Enable_PORT &= ~(1 << Burn_Enable_PIN);

		// Set Power Done Pin HIGH
		Power_Done_PORT |= (1 << Power_Done_PIN);
		
		// Sleep Delay
		delay(500);

		// End Function
		return true;

	}

	// Action
	switch (_Action) {

		// Check File
		case Action_Check_File: {

			// Break
			break;

		}

		// Verify Flash	  
		case Action_Verify_Flash: {

			// Break
			break;

		}

		// Write To Flash	
		case Action_Write_To_Flash: {

			// Program Enable
			Program (Command_Progam_Enable, Command_Chip_Erase);
			
			// Command Delay
			delay (20);

			// Poll Until Ready
			Poll_Until_Ready ();

			// Clear Page
			Clear_Page();
		
			// Break
			break;

		}
			
	}

	// Read Hex File
	while (sdin.getline (_Buffer, 80)) {

		// check for line too long
		int count = sdin.gcount();

		// check for line too long
		if (sdin.fail()) {

			// Show Message
			Show_Message (MSG_LINE_TOO_LONG);
			
			// End Function
			return true;

		}
	  
		// ignore empty lines
		if (count > 1) {

			// Process Line
			if (Process_Line (_Buffer, _Action)) return true;

		}

	}

	// Close File
	if (!gotEndOfFile) {

		// Show Message
		Show_Message (MSG_NO_END_OF_FILE_RECORD);

		// Set Burn Enable Pin LOW
		Burn_Enable_PORT &= ~(1 << Burn_Enable_PIN);

		// Set Power Done Pin HIGH
		Power_Done_PORT |= (1 << Power_Done_PIN);
		
		// Sleep Delay
		delay(200);

		// End Function		
		return true;

	}

	// Action
	switch (_Action) {

		// Check File
		case Action_Write_To_Flash: {

			// Commit Page
			if (oldPage != 0xFFFFFFFF) Commit_Page (oldPage);

			// Break
			break;

		}

		// Verify Flash
		case Action_Verify_Flash: {

			// Error
			if (errors > 0) {

				// Show Message
				Show_Message (MSG_VERIFICATION_ERROR);

				// Set Burn Enable Pin LOW
				Burn_Enable_PORT &= ~(1 << Burn_Enable_PIN);

				// Set Power Done Pin HIGH
				Power_Done_PORT |= (1 << Power_Done_PIN);

				// Sleep Delay
				delay(200);

				// End Function				
				return true;

			}

			// Break
			break;

		}

		// Check File
		case Action_Check_File: {

			// Break
			break;

		}

	}

	// End Function
	return false;

}

// Choose Input File
bool Choose_Input_File(void) {
 
	// Check File
	if (Read_Hex_File(Firmware_Name, Action_Check_File)) return true;
  
	// check file would fit into device memory
	if (highestAddress > Current_Signature.Flash_Size) {
		Show_Message (MSG_FILE_TOO_LARGE_FOR_FLASH);
		return true;
	}
  
	// check start address makes sense
	if (Update_Fuses (false)) return true;
	
	// End Function
	return false;
	
}

// Write Flash Contents
bool Write_Flash_Contents(void) {
	
  errors = 0;
  
  if (Choose_Input_File()) return false;

  // ensure back in programming mode
  if (!Digital_SPI_Start_Programming ()) return false;

  // now commit to flash
  if (Read_Hex_File(Firmware_Name, Action_Write_To_Flash)) return false;

	// Set Working LED ON
	LED_Green_PORT |= (1 << LED_Green_PIN);

  // verify
  if (Read_Hex_File(Firmware_Name, Action_Verify_Flash)) return false;

  // now fix up fuses so we can boot
  if (errors == 0) Update_Fuses (true);
	
  return errors == 0;
	
}

// Stop Programming
void Digital_SPI_Stop_Programming (void) {

	// Turn Digital SPI Pins Pull-Up OFF
	Digital_MISO_PORT &= ~(1 << Digital_MISO_PIN);
	Digital_MOSI_PORT &= ~(1 << Digital_MOSI_PIN);
	Digital_SCK_PORT &= ~(1 << Digital_SCK_PIN);
	Digital_Reset_PORT &= ~(1 << Digital_Reset_PIN);

	// Set Digital SPI Pins as INPUT
	Digital_MISO_DDR &= ~(1 << Digital_MISO_PIN);
	Digital_MOSI_DDR &= ~(1 << Digital_MOSI_PIN);
	Digital_SCK_DDR &= ~(1 << Digital_SCK_PIN);
	Digital_Reset_DDR &= ~(1 << Digital_Reset_PIN);

}

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

	// Set Burn Enable Pin HIGH
	Burn_Enable_PORT |= (1 << Burn_Enable_PIN);
	delay(200);

	// breadboards.  use SPI_FULL_SPEED for better performance.
	while (!sd.begin (10, SPI_HALF_SPEED)) {

		// Show Message
		Show_Message(MSG_NO_SD_CARD);

		// Set Burn Enable Pin LOW
		Burn_Enable_PORT &= ~(1 << Burn_Enable_PIN);

		// Set Power Done Pin HIGH
		Power_Done_PORT |= (1 << Power_Done_PIN);

		// Sleep Delay
		delay(200);

	}

}  // end of setup

//------------------------------------------------------------------------------
//      LOOP
//------------------------------------------------------------------------------
void loop (void) {

	// Set Burn Enable Pin HIGH
	Burn_Enable_PORT |= (1 << Burn_Enable_PIN);

	// Show Message
	Show_Message(MSG_FLASHED_TEST);

	// Start Programming
	if (!Digital_SPI_Start_Programming()) {

		// Show Message
		Show_Message (MSG_CANNOT_ENTER_PROGRAMMING_MODE);

		// Set Burn Enable Pin LOW
		Burn_Enable_PORT &= ~(1 << Burn_Enable_PIN);

		// Set Power Done Pin HIGH
		Power_Done_PORT |= (1 << Power_Done_PIN);

		// Sleep Delay
		delay(200);

		// End		
		return;
		
	}

	// Show Message
	Show_Message(MSG_FLASHED_OK);






	// Get Signature
	Get_Signature();
	
	// Get Fuse Byte
	Get_Fuse_Bytes();
  
	// No Signature Found
	if (foundSig == -1) {
		
		// Signature Error Message
		Show_Message (MSG_CANNOT_FIND_SIGNATURE);

		// Set Burn Enable Pin LOW
		Burn_Enable_PORT &= ~(1 << Burn_Enable_PIN);

		// Set Power Done Pin HIGH
		Power_Done_PORT |= (1 << Power_Done_PIN);

		// Sleep Delay
		delay(200);

		// End
		return;

	}

	// Set Working LED ON
	LED_Blue_PORT |= (1 << LED_Blue_PIN);
	
	// Burn Firmware
	bool _OK = Write_Flash_Contents();

	// Set Working LED OFF
	LED_Blue_PORT &= ~(1 << LED_Blue_PIN);

	// Delay
	delay (200);
  
	// OK Message
	if (_OK) Show_Message (MSG_FLASHED_OK);

	// Stop Programming
	Digital_SPI_Stop_Programming ();
	delay(100);





	// Set Burn Enable Pin LOW
	Burn_Enable_PORT &= ~(1 << Burn_Enable_PIN);

	// Set Power Done Pin HIGH
	Power_Done_PORT |= (1 << Power_Done_PIN);

	// Sleep Delay
	delay(200);

}

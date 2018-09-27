#ifndef STK_500_PROGRAMMER_H
#define STK_500_PROGRAMMER_H

/*=============================================>>>>>
= Dependencies =
===============================================>>>>>*/

#include "SD/mySD.h"


/*=============================================>>>>>
= Definitions =
===============================================>>>>>*/
//ATmega hardware definitions
#define PAGE_SIZE_WORDS 64
#define BYTES_PER_WORD 2
#define BYTES_PER_FLASH_BLOCK (PAGE_SIZE_WORDS * BYTES_PER_WORD)
//Hex file properties
#define MAX_CHARS_PER_HEX_RECORD 47
//Protocol behaviour settings
#define OPTIBOOT_BAUD_RATE 38400
#define STK_500_FLASH_PROCESS_TIMEOUT 80000 //80 seconds



/*=============================================>>>>>
=  Class for a storing a hex file record and decoding it into consituent parts =
===============================================>>>>>*/
class HexFileRecord{

public:

   bool decode(); //Decodes ascii hex file record into constituent parts

   const char* ascii_line;

   byte byteCount = 0; //Number of data bytes
   uint16_t address = 0;   //beginning memmory address offset of the data block (2-byte word-oriented)
   byte recordType = 0; //0x46 = flash
   const char* data = 0; //pointer to where the data bytes start
   byte checkSum = 0;

};


/*=============================================>>>>>
= Data structure for storing a 128 byte block of program data to be written to target MCU =
===============================================>>>>>*/

struct flash_page_block_t{
   uint16_t block_size_bytes = 0;
   uint16_t addressStart = 0; //This is a word-oriented address!
   byte dataBytes[BYTES_PER_FLASH_BLOCK] = {0};
};


/*=============================================>>>>>
=
Interface object used by STK500 code to retrieve blocks of flash data
from the hex file on the SD card in a format that is useful for re-encoding as
STK500 protocol messages
 =
===============================================>>>>>*/

class HexFileClass{

public:
   void begin(JAZA_FILES_t _targetFile);

   //Function that will load data from a hexfile on SD card into a flash_page_block_t data structure
   unsigned int load_hex_records_flash_data_block(flash_page_block_t &targBlock );

   bool moreBytesToConsume(){
      return (hexfile_chars_consumed < hexfile_total_bytes);
   }
   // unsigned int last_hexRecord_accessed = 0;


private:
   //Function to read/decode a line from hex file on SD card
   bool consume_hex_record(HexFileRecord &targRecord);

   unsigned int hexfile_chars_consumed = 0;
   unsigned int hexfile_total_bytes = 0;
   JAZA_FILES_t targFile = FILE_JAZAPACK_HEX_FILE;

};



/*=============================================>>>>>
=
We can move to a state-machine based flashing program eventually, which would
allow the Electron to keep doing other tasks while concurrently reprogramming
the ATmega.
TODO : make multitasking during firmware upload a thing
 =
===============================================>>>>>*/
// enum programmer_state_t{
//    PROGSTATE_ERROR,
//    PROGSTATE_IDLE,
//    PROGSTATE_RESETTING,
//    PROGSTATE_SYNC_CHECK,
//    PROGSTATE_WRITING_FIRMWARE,
//    PROGSTATE_READING_FIRMWARE,
//    PROGSTATE_SUCCESS
// };


/*=============================================>>>>>
=
Interface class used by outside code to reprogram attached ATmegas
=
===============================================>>>>>*/

class STK_Programmer{

public:

   bool programTarget(JAZA_FILES_t targetHexFile = FILE_JAZAPACK_HEX_FILE);


private:
   void resetTarget();


   bool sync_check_in_progress = false;
   bool waiting_for_response = false;
   unsigned int response_timer_start = 0;

   // programmer_state_t progState = PROGSTATE_IDLE;

};


extern STK_Programmer stk500;



#endif

/*=============================================>>>>>
= Dependencies =
===============================================>>>>>*/
#include "Arduino.h"
#include "STK_500_Programmer.h"
#include "stk500.h"



#define SPI_SPEED_MHZ 10
#define MHZ 1000000

/*=============================================>>>>>
= Defines =
===============================================>>>>>*/
#define TX 1   //Define TX pin of host arduino
//This pin is used
/*=============================================>>>>>
= Global variables =
===============================================>>>>>*/
HexFileClass hexFile; //Declare a hexFile object for working with entire hexfile
//SDFat library object
SdFat sd;            //The instance of the SDFat utility
//Buffer for storing retrieved bytes from SD card
char sdBuf[MAX_CHARS_PER_HEX_RECORD];       //Longest hex file record to be read is 45... so go 50 just to be safe


/*=============================================>>>>>
= SD Card Helper Functions =
===============================================>>>>>*/
/*=============================================>>>>>
= Function to call when an SD error is encountered =
===============================================>>>>>*/
void SD_error_handler(unsigned int lineNum){
   //Debug print
   #ifdef TEST_MODE_VERBOSE_SAFE_MODE_DEBUG
   myLog.warn("SD error handler!");
   #endif
   //Set global SD utility state variables
   //Create warn message with error data payload
   Serial.print("SD ERROR from line ");
   Serial.println(lineNum, DEC);
   Serial.print("SD error code ");
   Serial.println(sd.cardErrorCode(), HEX);
   Serial.print("SD error data ");
   Serial.println(sd.cardErrorData(), HEX);
}


/*= End of SD Card Helper Functions =*/
/*=============================================<<<<<*/



/*=============================================>>>>>
= STK500 MESSAGE HELPER FUNCTIONS =
===============================================>>>>>*/

/*=============================================>>>>>
= Function for waiting for an expected response from the target MCU =
Params:
- expected byte that denotes success
- total number of bytes expected
- milliseconds to wait for response before timeout condition declared
- (optional) human-readable symbol for the command that we sent that we are waiting for a response for (for debugging)
===============================================>>>>>*/

bool STK_wait_receive(byte successByte, byte expected_bytes, unsigned int receiveTimeout, const char* msg_name = NULL){
   //Set timeout timer
   unsigned int timeoutStart = millis();
   byte inByte = 0;
   byte bytesRead = 0;
   //Keep checking for response until timeout or good response
   while(1){
      //Check for incoming byte
      if(Serial1.available()){
         inByte = Serial1.read();
         bytesRead++;
         //Check if we have all bytes expected
         if(bytesRead == expected_bytes){
            //Did we get the expected response?
            if(inByte == successByte){
               return true;
            }
            //Unexpected response
            char myBuf[256];
            snprintf(myBuf, 256, "%s unexpected response!", msg_name);
            Serial.println(myBuf);

            return false;
         }
      }//End if //Serial.available()
      //Has the request timed out?
      if((millis() - timeoutStart) > receiveTimeout){
         char myBuf[256];
         snprintf(myBuf, 256, "%s receive timeout!", msg_name);
         Serial.println(myBuf);

         return false;
      }
   };
}


/*=============================================>>>>>
= Function for establishing that there is an target MCU running Optiboot that we
can communicate with =
===============================================>>>>>*/

bool getSync(){
   Serial1.write(STK_GET_SYNC);
   Serial1.write(CRC_EOP);
   return STK_wait_receive(STK_OK, 2, 500, "STK_GET_SYNC");
}


void STK_send_address_msg(uint16_t target_addr){
   Serial1.write(STK_LOAD_ADDRESS);
   Serial1.write(target_addr&0xFF);       //Send addr_low
   Serial1.write((target_addr>>8)&0xFF);  //Send addr_high
   Serial1.write(CRC_EOP);
}

/*=============================================>>>>>
= Function that sends out a page of flash data to the target MCU using
STK_PROG_PAGE 0x64
=
===============================================>>>>>*/

void STK_send_prog_page_msg( flash_page_block_t &targBlock){
   /*=============================================>>>>>
   = Message header =
   ===============================================>>>>>*/
   Serial1.write(STK_PROG_PAGE); //Send program page command
   Serial1.write((byte)((BYTES_PER_FLASH_BLOCK >> 8) & 0xFF));   //Send bytes_high
   Serial1.write((byte)(BYTES_PER_FLASH_BLOCK & 0xFF));  //Send bytes low
   Serial1.write((byte)STK_MEMTYPE_FLASH);   //Send memtype
   /*=============================================>>>>>
   = Data payload =
   ===============================================>>>>>*/
   for(uint16_t count = 0; count < BYTES_PER_FLASH_BLOCK; count++){
      if(count < targBlock.block_size_bytes){
         Serial1.write(targBlock.dataBytes[count]);
      }
      else{
         //Pad the rest of the flash block with 0xFF's
         Serial1.write(0xFF);
      }
   }
   //Pad rest of page with 0xFF if only partial page is being written

   /*=============================================>>>>>
   = Terminating byte =
   ===============================================>>>>>*/
   Serial1.write(CRC_EOP);
}


/*=============================================>>>>>
= Function that requests a page of flash memory from target MCU and fills a flash_page_block_t
data structure with the returned result =
===============================================>>>>>*/

bool STK_get_flash_block(flash_page_block_t &targetFlashBlock){

   // myLog.info("Reading flash block at word address %#04X ", targetFlashBlock.addressStart);
   // Serial.flush();

   /*=============================================>>>>>
   = Message header =
   ===============================================>>>>>*/
   Serial1.write(STK_READ_PAGE); //Send read page command
   Serial1.write( (byte) ((BYTES_PER_FLASH_BLOCK >> 8) & 0xFF) );//Write bytes_high
   Serial1.write( (byte) (BYTES_PER_FLASH_BLOCK & 0xFF));   //Write bytes_low
   Serial1.write((byte) STK_MEMTYPE_FLASH);
   Serial1.write(CRC_EOP);
   //Wait for firt byte to come back
   if(!STK_wait_receive(STK_INSYNC, 1, 500, "STK_READ_PAGE - start")){
      return false;
   }
   //Read flash block data bytes
   uint16_t bytesRead = 0;
   unsigned int timeStart = millis();
   while(bytesRead < BYTES_PER_FLASH_BLOCK){ //Need to keep reading from target until end of flash block!
      if(Serial1.available()){
         targetFlashBlock.dataBytes[bytesRead] = Serial1.read();
         // Serial.printlnf("%#02.2X", targetFlashBlock.dataBytes[bytesRead]);
         bytesRead++;
         timeStart = millis();   //Reset timeout timer
      }
      else if ((millis() - timeStart) > 2000) {
         //Timeout: 1000 ms since last byte was received
         return false;
      }
   }
   // Serial.println();

   // myLog.info("flash block data received");

   targetFlashBlock.block_size_bytes = bytesRead;
   return STK_wait_receive(STK_OK, 1, 500, "STK_READ_PAGE - end");

}
/*= End of STK500 MESSAGE HELPER FUNCTIONS =*/
/*=============================================<<<<<*/



/*=============================================>>>>>
= STK500 Programmer Class Functions =
===============================================>>>>>*/


/*=============================================>>>>>
= Function called by external code to initialize STK_500_Programmer resources =
===============================================>>>>>*/
bool STK_Programmer::begin(){
   //Initialize the SD card object
   if (!sd.begin(chipSelectPin)) {
      //Initialization error
      SD_error_handler(__LINE__);
      //Done
      return false;
   }
   return false;
}

/*=============================================>>>>>
= Function called by external code to program the attached target target MCU =
===============================================>>>>>*/

bool STK_Programmer::programTarget(const char* targFile){
   //Now we will open the hex file on the SD card and find out what address
   //to start programming at
   hexFile.begin(targFile);  //Reset bytes consumed count to 0
   //First reset the target MCU
   resetTarget();
   //Now ask target MCU if it is there 3 times before continuing
   byte numSyncs = 0;
   while(getSync()){
      if((numSyncs++)>2){
         break;
      }
   };
   if(numSyncs < 3){
      char myBuf[256];
      snprintf(myBuf, 256, "sync failure");
      Serial.println(myBuf);

      return false;
   }
   //Loop through hex file records, assembling multiple records into single flash page blocks
   flash_page_block_t sdFlashBlock;

   unsigned int flashTimer = millis();

   while(hexFile.moreBytesToConsume() ){
      if((millis() - flashTimer) > STK_500_FLASH_PROCESS_TIMEOUT){
         char myBuf[256];
         snprintf(myBuf, 256, "Flash process timeout!");
         Serial.println(myBuf);

         return false;
      }
      //Assemble a bunch of records into a single flash block
      sdFlashBlock.block_size_bytes = hexFile.load_hex_records_flash_data_block(sdFlashBlock);
      //Compose a STK message that sets Optiboot target address to equivalent in hex record
      STK_send_address_msg(sdFlashBlock.addressStart);
      //Wait for appropriate response
      if(!STK_wait_receive(STK_OK, 2, 100, "STK_LOAD_ADDRESS")){
         //Didn't get appropriate response
         char myBuf[256];
         snprintf(myBuf, 256, "Failed to set address 0X%0X", sdFlashBlock.addressStart);
         Serial.println(myBuf);

         return false;
      }
      //Got appropriate response!

      /*=============================================>>>>>
      = Send the page data to the target MCU =
      ===============================================>>>>>*/
      STK_send_prog_page_msg(sdFlashBlock);
      //Wait for appropriate response
      if(!STK_wait_receive(STK_OK, 2, 1000, "STK_PROG_PAGE")){
         char myBuf[256];
         snprintf(myBuf, 256, "Failed to program page at address 0x%0x", sdFlashBlock.addressStart);
         Serial.println(myBuf);

         return false;
      }
   }  //End while hexFile.getRecord()

   // myLog.info("Firmware write complete");
   // Serial.flush();



   /*=============================================>>>>>
   = Now read back the bytes from the target to verify it was programmed correctly =
   ===============================================>>>>>*/
   hexFile.begin(targFile); //Resets bytes consumed to 0
   flash_page_block_t targetFlashBlock;
   flashTimer = millis();  //Reset timeout timer

   // myLog.info("Reading back progmem to verify success");
   Serial.flush();

   while(hexFile.moreBytesToConsume()){
      if((millis() - flashTimer) > STK_500_FLASH_PROCESS_TIMEOUT){
         char myBuf[256];
         snprintf(myBuf, 256, "Flash process timeout!");
         Serial.println(myBuf);

         return false;
      }
      //Assemble a bunch of records into a single flash block
      sdFlashBlock.block_size_bytes = hexFile.load_hex_records_flash_data_block(sdFlashBlock);
      // myLog.info("Done loading sd flash block");
      Serial.flush();

      /*=============================================>>>>>
      = Request the page data from the target MCU =
      ===============================================>>>>>*/
      //Compose a STK message that sets Optiboot target address to equivalent in hex record
      STK_send_address_msg(sdFlashBlock.addressStart);
      //Wait for appropriate response
      if(!STK_wait_receive(STK_OK, 2, 100, "STK_LOAD_ADDRESS")){
         //Didn't get appropriate response
         char myBuf[256];
         snprintf(myBuf, 256, "Failed to set address 0X%0X", sdFlashBlock.addressStart);
         Serial.println(myBuf);

         return false;
      }
      //Got appropriate response!

      // myLog.info("Address successfully set");
      //Set target flash block address and size equal to the one in the sd hex file
      targetFlashBlock.addressStart = sdFlashBlock.addressStart;
      targetFlashBlock.block_size_bytes = sdFlashBlock.block_size_bytes;
      if(!STK_get_flash_block(targetFlashBlock)){
         //Failed to pull flash block from target MCU
         return false;
      }
      /*=============================================>>>>>
      = Compare received flash block with one from hex file =
      ===============================================>>>>>*/
      // Serial.println("BYTE # | TARGET | SD HEX");
      for(uint16_t count = 0; count < sdFlashBlock.block_size_bytes; count++){
         // Serial.printlnf("%#6u |  %#02.2X  |   %#02.2X", count, targetFlashBlock.dataBytes[count], sdFlashBlock.dataBytes[count]);
         // Serial.flush();
         if(targetFlashBlock.dataBytes[count] != sdFlashBlock.dataBytes[count]){
            //Found elements of flash blocks that do not match
            char myBuf[256];
            snprintf(myBuf, 256, "Programmed image does not match hex image at base address %#0X, offset %u", sdFlashBlock.addressStart, count);
            Serial.println(myBuf);

            return false;
         }
      }//End for
   }

   // myLog.info("firmware image match success!");


   /*=============================================>>>>>
   = End programming session gracefully =
   ===============================================>>>>>*/
   Serial1.write(STK_LEAVE_PROGMODE);
   Serial1.write(CRC_EOP);
   if(!STK_wait_receive(STK_OK, 2, 100, "STK_LEAVE_PROGMODE")){
      return false;
   }

   /*=============================================>>>>>
   = SUCCESS =
   ===============================================>>>>>*/
   // myLog.info("Flash success!");
   return true;

}

/*=============================================>>>>>
= Function to reset the attached target MCU =
use MANUAL_DEDICATED_RESET_PIN define (above) to toggle between using a discrete
reset signal pin (for testing) or using the UART TX held low wathdog timeout reset
trigger (production JP2 PCB strategy)
===============================================>>>>>*/

void STK_Programmer::resetTarget(){

   //myLog.info("Resetting target MCU");

   if(use_watchdog_reset_method){
      Serial1.end();
      pinMode(TX, OUTPUT);
      digitalWrite(TX, LOW);
      delay(1000);
      // digitalWrite(TX, HIGH);
      Serial1.begin(optiboot_baud_rate);
      delay(100);
   }
   else{
      digitalWrite(target_reset_pin, LOW);
      delay(1);
      digitalWrite(target_reset_pin, HIGH);
   }

}



/*= End of STK500 Programmer Class Functions =*/
/*=============================================<<<<<*/


/*=============================================>>>>>
= HexFileRecord class functions =
===============================================>>>>>*/

/*=============================================>>>>>
= Function to decode an ascii hex file string into its constituent elements =
===============================================>>>>>*/

bool HexFileRecord::decode(){
   //myLog.info("Decoding hex record: ");
   // printEscapedStr(ascii_line);
   //Serial.println();
   //Check for leading colon
   if((char)ascii_line[0] == ':'){
      //Decode byte count
      if(1 == sscanf(ascii_line + 1, "%2x", &byteCount)) {
         //myLog.trace("byteCount = %#02X", byteCount);
         //Decode address
         if(1 == sscanf(ascii_line + 3, "%4x", &address)) {
            //myLog.trace("address = %#02X", address);
            //Decode record type
            if(1 == sscanf(ascii_line + 7, "%2x", &recordType)) {
               //myLog.trace("recordType = %#02X", recordType);
               //Calculate location of data
               data = ascii_line + 9;
               //myLog.trace("data = %.*s", byteCount*2, data);
               //Decode checksum
               if(1 == sscanf(data + (byteCount*2) , "%2x", &checkSum)) {
                  //TODO: see if checksum makes sense
                  //myLog.trace("checkSum = %#02X", checkSum);
                  return true;
               }
            }
         }
      }
   }
   Serial.print("Invalid hex file record: ");
   Serial.println(sdBuf);

   return false;
}

/*= End of HexFileRecord class functions =*/
/*=============================================<<<<<*/



/*=============================================>>>>>
= HexFileClass class functions =
===============================================>>>>>*/

/*=============================================>>>>>
= Function to initialize hexfile object =
===============================================>>>>>*/
bool HexFileClass::begin(const char* targFilePath){
   //Reset bytes consumed
   hexfile_chars_consumed = 0;
   //Check if sd file is allready open
   if(sdHexFile.isOpen()){
      //Close the file
      if(!sdHexFile.close()){
         //Error writing cache back to SD card!
         SD_error_handler(__LINE__);
         //Return
         return false;
      }
      //Successfully closed the file!
   }
   //Open the target file
   if( !sdHexFile.open( targFilePath , (O_READ) ) ){
      //Error opening the target file!
      SD_error_handler(__LINE__);
      //Return
      return false;
   }
   //Save files size so we don't have to query it from SDFat (not sure if this results in SD card read operations to determine size, so err on side of quickity)
   hexfile_total_bytes = sdHexFile.fileSize();
}

/*=============================================>>>>>
= Function intended to be called repeatedly, each time scanning/consuming/decoding a single
ascii record (including terminating characters) in a hex file located on an attached SD card =
===============================================>>>>>*/
bool HexFileClass::consume_hex_record(HexFileRecord &targRecord){
   //Retrieve the bytes from sd card
   byte bytesToRead = MAX_CHARS_PER_HEX_RECORD;

   if((hexfile_total_bytes - hexfile_chars_consumed) < bytesToRead){
      bytesToRead = (hexfile_total_bytes - hexfile_chars_consumed);
      //myLog.info("Modifying bytesToRead down to %u", bytesToRead);
   }
   //Set sd file access pointer to end of consumed bytes
   if(!sdHexFile.seekSet(hexfile_chars_consumed)){
      SD_error_handler(__LINE__);
      return false;
   }
   int readResult = sdHexFile.read(sdBuf, bytesToRead);
   targRecord.ascii_line = sdBuf;
   if(readResult < 0) {
      SD_error_handler(__LINE__);
      return false;
   }
   if(readResult < 11){ //No valid hex record can be shorter than 11 characters
      Serial.print("Incomplete hex record @ byte ");
      Serial.println(hexfile_chars_consumed, DEC);
      return false;
   }
   if(!targRecord.decode()){
      //Seppuku
      return false;
   }
   //Record length is always 11 + num data bytes
   hexfile_chars_consumed += (11 + (targRecord.byteCount * 2) + 2);   //2 == /r/n hex record terminating characters (Windows style encoding) TBD: does a linux arduino built hex file work?
   return true;
}

/*=============================================>>>>>
= Retrieve/decode/re-encode multiple hex file records into a page block =

Function to retrieve/decode a contiguous set of hex file records and use them to
populate a flash_page_block_t data structure that can in turn be used by STK500
protocol to flash 128 bytes of program memory to target MCU

returns the number of bytes loaded  into the flash data block
===============================================>>>>>*/
unsigned int HexFileClass::load_hex_records_flash_data_block(flash_page_block_t &targBlock){

   //Load a hexFile record using some TODO: SD card load hexFile record function
   HexFileRecord targRecord;
   uint16_t bytesEncoded = 0;
   //myLog.info("Filling flash block");
   //Loop through records
   while(consume_hex_record(targRecord)){
      uint16_t bytesProcessed = 0;
      //If no bytes have been encoded yet, then this is the first hex record being processed.
      //That means that the address contained in this hex record corresponds to the word-oriented
      //address of the target flash block
      if(!bytesEncoded){
         targBlock.addressStart = targRecord.address / 2;  //Remember to convert to word-oriented address!
         //myLog.info("Block addres = %#02X", targBlock.addressStart);
      }
      //Encode data bytes of flash record
      while(bytesProcessed < targRecord.byteCount){
         int result = sscanf(targRecord.data + (bytesProcessed * 2), "%2x", &targBlock.dataBytes[bytesEncoded]);
         //Check that decoding worked
         if(result != 1){
            char myBuf[256];
            snprintf(myBuf, 256, "Invalid record data digits in record: %2s --> hex file corrupt!",targRecord.data + bytesProcessed);
            Serial.println(myBuf);

            return 0;
         }
         bytesProcessed ++;
         bytesEncoded ++;
         //Check that we haven't filled the block
         if(bytesEncoded >= sizeof(targBlock.dataBytes)){
            //myLog.info("Data block filled!");
            return bytesEncoded;
         }
      };
      //Check if there are any more records before consuming another one
      if(!moreBytesToConsume()){
         //myLog.info("Consumed all hex records");
         return bytesEncoded;
      }
   };

   return bytesEncoded;
}


/*= End of HexFileClass class functions =*/
/*=============================================<<<<<*/

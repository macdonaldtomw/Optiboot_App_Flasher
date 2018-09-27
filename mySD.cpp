//PARTICLE_STUDIO_READY


/**
*
*

This file contains implementations of functions for the mySD utility, which allows
the rest of our app to easily read and write data to the SD card in a csvs formatted way

*
*/


//---------------------------------- DEFINES -----------------------------------

// ----------------------------------INCLUDES-----------------------------------
#include "Arduino.h"
#include "mySD.h"
//#include TestingConfig.h

SPISettings my_spi_settings(SPI_SPEED_MHZ * MHZ, MSBFIRST, SPI_MODE0);



/*=============================================>>>>>
= EXTERNALLY LINKED GLOBALS DEFINITIONS =
===============================================>>>>>*/
// bool unhandled_SD_hard_fault = false;
char sdBuf[SD_BUF_SIZE];       //I think SDFat uses a 512 byte buffer for writing....
/*=============================================>>>>>
= GLOBAL VARIABLES =
===============================================>>>>>*/
//SDFat library objects
SdFat sd;            //The instance of the SDFat utility
SdFile file;   //Instance of the SdFile class (from SDFat library)

uint8_t chipSelect = 0;  //Use the default Electron Slave Select pin

JAZA_FILES_t currentlyOpenFile = NUM_TYPES_JAZA_FILES;

unsigned int lastGetEntryNum = 0;
int lastGetEntryStartPos = 0;

//Declare an array of JazaFile_t files that we are going to use
JazaFile_t myFiles[NUM_TYPES_JAZA_FILES]  = {
   [FILE_JAZAPACK_HEX_FILE] = JazaFile_t("jpFirmware.hex")
};

/*=============================================>>>>>
= Function forward declarations =
===============================================>>>>>*/

bool smartFileOpen(JAZA_FILES_t fileType);

/*= End of Function forward declarations =*/
/*=============================================<<<<<*/


/*=============================================>>>>>
= Function to call when an SD error is encountered =
===============================================>>>>>*/
void SD_error_handler(unsigned int lineNum){
   //Debug print
   #ifdef TEST_MODE_VERBOSE_SAFE_MODE_DEBUG
   myLog.warn("SD error handler!");
   #endif
   //Set global SD utility state variables
   SD_INITIALIZED = false;
   //Create warn message with error data payload
   int sdErrorCode = sd.cardErrorCode();
   int sdErrorData = sd.cardErrorData();
   snprintf(sdWriteBuf, SD_BUF_SIZE, "SD Error | %s | 0X%02X | 0X%02X", myFiles[currentlyOpenFile].name, sdErrorCode, sdErrorData);
   myLog.warn("%s", sdWriteBuf);
}

/*=============================================>>>>>
= Function for opening files =
===============================================>>>>>*/
//Function open the corresponding sdFat file for the passed myFile
bool smartFileOpen(JAZA_FILES_t fileType){

   // #ifdef TEST_MODE_VERBOSE_JAZASD_DEBUG_HEAVY_AF
   // myLog.info("smartFileOpen %s", myFiles[fileType].name);
   // #endif

   #ifdef TEST_MODE_SIMULATE_SD_CRASH_AFTER_TIME
   if(millis() > TEST_MODE_SIMULATE_SD_CRASH_AFTER_TIME){
      SD_error_handler(__LINE__);
      return false;
   }
   #endif


   //Check if the target file is already open
   // if(myFiles[fileType].isOpen){
   if(currentlyOpenFile == fileType){
      //File was already open!
      #ifdef TEST_MODE_VERBOSE_JAZASD_DEBUG_HEAVY_AF
      myLog.info("%s already open", myFiles[fileType].name);
      #endif
   }
   //File was not already open!
   else{

      if(file.isOpen()){
         //Debug
         #ifdef TEST_MODE_VERBOSE_JAZASD_DEBUG
         myLog.info("file.close(\"%s\")", myFiles[currentlyOpenFile].name);
         #endif
         //Set default currently open file value in case we actually fail at opening the file
         currentlyOpenFile = NUM_TYPES_JAZA_FILES;
         //Close the file
         if(!file.close()){
            //Error writing cache back to SD card!
            SD_error_handler(__LINE__);
            //Return
            return false;
         }
         //Successfully closed the file!
      }
      #ifdef TEST_MODE_VERBOSE_JAZASD_DEBUG
      myLog.info("file.open(\"%s\") - L%u", myFiles[fileType].name, __LINE__);
      #endif
      //Open the target file
      if( !file.open( myFiles[fileType].name , (O_RDWR | O_CREAT) ) ){
         //Error opening the target file!
         SD_error_handler(__LINE__);
         //Return
         return false;
      }
   }

   //Debug
   // #ifdef TEST_MODE_VERBOSE_JAZASD_DEBUG_HEAVY_AF
   // myLog.trace("Done opening file");
   // #endif

   //Save the fact that this file is now open
   // myFiles[fileType].isOpen = true;
   currentlyOpenFile = fileType;
   //Success!
   return true;
}


/*=============================================>>>>>
= INITIALIZATION FUNCTIONS =
===============================================>>>>>*/

bool SD_INITIALIZED = false;

void MySDClass::begin(unsigned int _chipSelect){

   chipSelect = _chipSelect;
   //Initialize the SD card object
   if (!sd.begin(chipSelect, my_spi_settings)) {
      //Initialization error
      SD_error_handler(__LINE__);
      //Done
      return;
   }
   SD_INITIALIZED = true;
}



/*=============================================>>>>>
= END INITIALIZATION FUNCTIONS =
===============================================>>>>>*/

/*=============================================>>>>>
= Function to read a set number of bytes from a file =
===============================================>>>>>*/
char* MySDClass::readBytes(JAZA_FILES_t fileType, uint32_t startByte, uint32_t numReadBytes){
   if(!SD_INITIALIZED) return NULL;
   //Sanity check on buffer size
   if(SD_BUF_SIZE < (numReadBytes - 1)){
      printError(myLog, __LINE__, mes_buf_Small);
      return NULL;
   }

   #ifdef TEST_MODE_VERBOSE_JAZASD_DEBUG_HEAVY
   myLog.trace("readBytes(\"%s\") - %u to %u", myFiles[fileType].name, startByte, startByte+numReadBytes);
   #endif

   //Open the target file
   if(smartFileOpen(fileType)){
      //Set file position
      #ifdef TEST_MODE_VERBOSE_JAZASD_DEBUG_SUPER_HEAVY_AF
      myLog.info("file.seekSet() - L%u", __LINE__);
      #endif
      if(file.seekSet(startByte)){
         //Read target bytes into buffer
         #ifdef TEST_MODE_VERBOSE_JAZASD_DEBUG_SUPER_HEAVY_AF
         myLog.info("file.read() - L%u", __LINE__);
         printFreeMem();
         #endif
         int bytesRead = file.read(sdBuf, numReadBytes);
         //Append null terminator
         sdBuf[bytesRead] = '\0';
         //Check if all bytes desired were read
         if( (unsigned int)(bytesRead) == numReadBytes){
            //Read all the bytes we were looking to read!
            return sdBuf;
         }
         else if(bytesRead < 0){
            printError(myLog, __LINE__, mes_sd_readError);
         }
      }
   }

   return NULL;
}

/*=============================================>>>>>
= Function to return the number of bytes in the file =
===============================================>>>>>*/
uint32_t MySDClass::bytesInFile(JAZA_FILES_t fileType){
   if(!SD_INITIALIZED) return 0;

   #ifdef TEST_MODE_VERBOSE_JAZASD_DEBUG_HEAVY
   myLog.trace("bytesInFile(\"%s\")", myFiles[fileType].name);
   #endif

   //Open the file
   if(smartFileOpen(fileType)){
      //Return the bytes in the file
      #ifdef TEST_MODE_VERBOSE_JAZASD_DEBUG_SUPER_HEAVY_AF
      myLog.info("file.fileSize() - L%u", __LINE__);
      #endif
      return file.fileSize();
   }
   else{
      #ifdef TEST_MODE_VERBOSE_JAZASD_DEBUG_HEAVY
      printError(myLog, __LINE__, "Error opening file");
      #endif
   }
   return 0;
}

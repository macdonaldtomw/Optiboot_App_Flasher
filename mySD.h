//PARTICLE_STUDIO_READY


/**
 *
 *

This file contains implementations of class for the mySD utility, which allows
the rest of our app to easily read and write data to the SD card in a csv formatted way

 *
 */

//Header guards
#ifndef JazaSD_h
#define JazaSD_h

/*=============================================>>>>>
= Dependencies =
===============================================>>>>>*/
#include "SdFat/SdFat.h"

/*=============================================>>>>>
= Configuration settings =
===============================================>>>>>*/
#define SPI_SPEED_MHZ 10
#define SD_BUF_SIZE 2049   //4 pages of SD memory (each page is 512 bytes)


// #define NUM_TYPES_JAZA_FILES 7
enum JAZA_FILES_t{
   FILE_JAZAPACK_HEX_FILE,
   NUM_TYPES_JAZA_FILES
};


/*=============================================>>>>>
= JazaFile_t data structure =
===============================================>>>>>*/

//Date structure for holding data related to each file type in the mySD specification
struct JazaFile_t{
   JazaFile_t(const char* fileName){
      name = fileName;
   }
   const char* name = NULL;
   // bool isOpen = false;
};


/*=============================================>>>>>
= myPublish class =
===============================================>>>>>*/
class MySDClass{

public:

   /*=============================================>>>>>
   = Initialization functions =
   ===============================================>>>>>*/
   void begin();
   //Byte-based read
   char* readBytes(JAZA_FILES_t fileType, uint32_t startByte, uint32_t numReadBytes);
   //Read number of bytes in file
   uint32_t bytesInFile(JAZA_FILES_t fileType);

   /*=============================================>>>>>
   = PUBLIC VARIABLES =
   ===============================================>>>>>*/

private:
   bool ready = false; //True if all systems check out when initializing mySD

};

//Externally linked global utiliity declaration
extern MySDClass mySD;

#endif

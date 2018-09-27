
/**
*
*

This file contains an application that runs on the Arduino which attempts to reset an ATmega328PB
with an optiboot bootloader pre-loaded onto it, and then immediately query the ATmega328PB
for its signature bytes to confirm that the bootloader is functioning correctly.

The target MCU is reset using only the UART TX/RX signals, by way of additional
watchdog sub-circuit that you must have on the target ATmega.

Logic is:  if UART TX (from host Arduino) gets pulled low for more than 800 ms,
the target MCU will be reset by an external watchdog circuit.

Programming is initialzed by typing a "0" into the serial monitor

*
*/

/*=============================================>>>>>
= Dependencies =
===============================================>>>>>*/
#include "SD/mySD.h"
#include "STK_500_Programmer.h"
/*=============================================>>>>>
= Definitions =
===============================================>>>>>*/
#define SD_CHIPSELECT  A2
/*=============================================>>>>>
= Global variables =
===============================================>>>>>*/
MySDClass mySD;   //The global utility of the MySDClass class (with external linkage)

enum serial_test_cmd_codes_t{
   CMD_PROGRAM_TARGET
};
STK_Programmer stk500;  //Programmer class object to use to program external ATmega




/*=============================================>>>>>
= SETUP function =
===============================================>>>>>*/
void setup(){

   //Initialize SPI peripheral
   delay(10);
   SPI.begin();
   SPI.setBitOrder(MSBFIRST);
   SPI.setDataMode(SPI_MODE0);

   //Start UART peripheral communicating with attached PC via Electron USB port
   Serial.begin(115200);
   delay(1000);
   Serial.println("Starting setup()");
   //Start UART peripheral communicating with attached ATmega
   Serial1.begin(OPTIBOOT_BAUD_RATE);
   //Begin SPI communication with the SD card
   mySD.begin();
   Serial.println("Done initializing SD card");

}
/*= End of SETUP function =*/
/*=============================================<<<<<*/

/*=============================================>>>>>
= MAIN LOOP =
===============================================>>>>>*/
void loop(){

   if(Serial.available()){
      byte cmd_byte = (byte)(Serial.read());
      //Convert ascii byte into number
      cmd_byte -= 48;

      switch(cmd_byte){

         case CMD_PROGRAM_TARGET:
         {
            //When no JAZAFILES_t object is passed, default is --> FILE_JAZAPACK_HEX_FILE --> "jpFirmware.hex"
            Serial.println("starting flash");

            unsigned int timeStart = millis();

            if(stk500.programTarget()){
               Serial.print("flash success! --> took ");
               Serial.print((millis() - timeStart), DEC);
               Serial.println(" ms");
            }
            else{
               Serial.println("Flash failed!");
            }
            break;
         }

         default:
         {
            Serial.println("Invalid PC command");
         }
      }//End switch
   }//End if
}
/*= End of MAIN LOOP =*/
/*=============================================<<<<<*/

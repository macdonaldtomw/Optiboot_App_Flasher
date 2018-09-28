


/**
*
*

This file contains an application that runs on the Arduino which attempts to reset an ATmega328PB
with an optiboot bootloader pre-loaded onto it, and then immediately query the ATmega328PB
for its signature bytes to confirm that the bootloader is functioning correctly.

The target MCU is reset using only the UART TX/RX signals, by way of additional
watchdog sub-circuit that you must have on the target target MCU.

Logic is:  if UART TX (from host Arduino) gets pulled low for more than 800 ms,
the target MCU will be reset by an external watchdog circuit.

Programming is initialzed by typing a "0" into the serial monitor

*
*/

/*=============================================>>>>>
= Dependencies =
===============================================>>>>>*/
#include "STK_500_Programmer.h"
/*=============================================>>>>>
= Definitions =
===============================================>>>>>*/
#define OPTIBOOT_BAUD_RATE 38400
/*=============================================>>>>>
= Global variables =
===============================================>>>>>*/

//List of command codes you can enter from your PC serial monitor to control things.
enum serial_test_cmd_codes_t{
   CMD_PROGRAM_TARGET
};


// constructor using standard SPI chip select pin for SD card and external watchdog circuit on target MCU which resets the target MCU when the UART TX line of this arduino is held low for more than 1 second
// STK_PROG_PAGE stk500;

//constructor using standard SPI chip select pin for SD card and the specified reset pin to reset target MCU
// STK_Programmer stk500(targPin, CS);  //Programmer class object to use to program external target MCU using default chip select pin

//constructor using custom SPI chip select pin for SD card and the specified reset pin to reset target MCU
// STK_Programmer stk500(targpin, CS);  //Programmer class object to use to program external target MCU using default chip select pin

//constructor using custom SPI chip select pin for SD card and the specified reset pin to reset target MCU, and custom baud rate
// STK_Programmer stk500(targpin, CS, target_baud_rate);  //Programmer class object to use to program external target MCU using default chip select pin

STK_Programmer stk500;  //Programmer class object to use to program external target MCU using default chip select pin


/*=============================================>>>>>
= SETUP function =
===============================================>>>>>*/
void setup(){

   //Start UART peripheral communicating with attached PC via Arduino USB port
   Serial.begin(115200);
   delay(1000);
   Serial.println("Starting setup()");
   //Start UART peripheral communicating with attached target MCU Optiboot bootloader
   Serial1.begin(OPTIBOOT_BAUD_RATE);
   //Begin SPI communication with the SD card
   Serial.println("Done initializing SD card");
}
/*= End of SETUP function =*/
/*=============================================<<<<<*/

/*=============================================>>>>>
= MAIN LOOP =
===============================================>>>>>*/
void loop(){

   if(Serial.available()){
      byte cmd_byte = Serial.read();
      //Convert ascii byte into number
      cmd_byte -= 48;

      switch(cmd_byte){

         case CMD_PROGRAM_TARGET:
         {
            //When no JAZAFILES_t object is passed, default is --> FILE_JAZAPACK_HEX_FILE --> "jpFirmware.hex"
            Serial.println("starting flash");

            unsigned int timeStart = millis();
            //Program target.... parameter is the filepath of the hex file on the SD card
            if(stk500.programTarget("firmware.hex")){
               Serial.println("flash success!");
            }
            else{
               Serial.println("Flash failed!");
            }
            Serial.print("Process took ");
            Serial.print((millis() - timeStart), DEC);
            Serial.println(" ms");
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

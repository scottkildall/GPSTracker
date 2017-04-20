/*
 *  GPS Tracker
 *  by Scott Kildall
 *  
 *  Uses Adafruit GPS Logger to write GPS coordinates to an SD card
 *  
 *  
 *  Adapted from Ladyada's logger modified by Bill Greiman to use the SdFat library
 *  Designer for Adafruit Ultimate GPS Shield using MTK33x9 chipset
 *
 *  Some key concepts / notes
 *  - uses Adafruit_GPS_mod files, which I altered from Adafruit's Adafruit_GPS due to a logging bug
 *  - no interrupt code (messy)
 *  - this uses the Adafruit GPS Logger with a soldered Red LED + 220 Ohm resistor at pin 6, Green LED + 220 Ohm resitstor at pin 4
 *  - the Red LED + Green LED are much more visible than the built-in and can be used for status update (see comments below)
 *  - built-in LED will be used for error codes
 *  - uses my MSTimer class to log to a GPS every 2 seconds (this interval can be changed), this is dictated through the dataSaveTimer
 *  - will make a series of data files, sequentially numbered so that one of them doesn't get too large. 
 *      You can alter this with the TIME_BEFORE_NEWFILE #define
 *  
 */
 
/*********************************************************************************************************
 * 
 * Status Codes:
 * 
 * - Both LEDS on: this is the startup, 1 or 2 seconds
 * - Both LEDS on while built-in LED flashes: error, most likely SD card isn't insterted
 * - Green LED then Red LED (while green is off): can't find a GPS fix, this may take a while, or go to a better location
 * - Green LED then Red LED (while green stays on): logging data
 * 
 *********************************************************************************************************/


// DATA should be something like: 35.655856,-105.977923

#include <SPI.h>
#include "Adafruit_GPS_mod.h"
#include <SoftwareSerial.h>
#include <SD.h>

#include "MSTimer.h"

// Arudino Uno, use this
SoftwareSerial mySerial(8, 7);

// Arduino Mega 2560, use this
//#define mySerial Serial1
Adafruit_GPS GPS(&mySerial);

// Set GPSECHO to 'false' to turn off echoing the GPS data to the Serial console
// Set to 'true' if you want to debug and listen to the raw GPS sentences
#define GPSECHO  true
/* set to true to only log to SD when GPS has a fix, for debugging, keep it false */
#define LOG_FIXONLY false  

// Set the pins used
#define chipSelect (10)
#define ledPin (13)
#define redLEDPin (6)
#define greenLEDPin (4)

File datafile;
MSTimer dataSaveTimer;
const unsigned long dataSaveTime = 2000;

//-- this will save sequential data files, keeping the milliseconds field intact. The reason for this is so that one datafile doesn't get
//-- too large or corrupted. You can copy and paste these files together or use a file-stitcher program.
MSTimer newFileTimer;
#define TIME_BEFORE_NEWFILE (1000 * 60 * 60)    // in ms, e.g. 1000 * 60 * 60 = 1 hour

//-- show debug messages (slower but helpful)
#define SERIAL_DEBUG true

void setup() {
  
  // connect at 115200 so we can read the GPS fast enough and echo without dropping chars
  // also spit it out
  if(SERIAL_DEBUG) {
    Serial.begin(115200);
    Serial.println("\r\nGPS Tracker by Scott Kildall");
  }

  // Set output pins
  pinMode(ledPin, OUTPUT);
  pinMode(redLEDPin, OUTPUT);
  pinMode(greenLEDPin, OUTPUT);

  // initial status is Red LED and Green LED both on
  digitalWrite(redLEDPin, true);
  digitalWrite(greenLEDPin, true);
  
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(chipSelect, OUTPUT);

  // SD Card initialization
  initSDCard();
  createSDFile();

  // GPS initialization
  initGPS();
  
  dataSaveTimer.setTimer(dataSaveTime);
  dataSaveTimer.start();

  newFileTimer.setTimer(TIME_BEFORE_NEWFILE);
  newFileTimer.start();

  if(SERIAL_DEBUG) {
    Serial.println("Successful initialization");
  }

  // Turn off red LED at this point
  digitalWrite(redLEDPin, false);
}



void loop() {
    readGPSData();
    
    //-- note by Scott, this needs to be cleaned up

  // parsing code from Adafruit
  // if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived()) {
    
    // a tricky thing here is if we print the NMEA sentence, or data
    // we end up not listening and catching other sentences! 
    // so be very wary if using OUTPUT_ALLDATA and trying to print out data
    
    // Don't call lastNMEA more than once between parse calls!  Calling lastNMEA 
    // will clear the received flag and can cause very subtle race conditions if
    // new data comes in before parse is called again.
    char *stringptr = GPS.lastNMEA();
    
    if (!GPS.parse(stringptr))   // this also sets the newNMEAreceived() flag to false
      return;  // we can fail to parse a sentence in which case we should just wait for another

    // Sentence parsed! 
    if(SERIAL_DEBUG) 
      Serial.println("OK");
    if (LOG_FIXONLY && !GPS.fix) {
      if(SERIAL_DEBUG)  
        Serial.print("No Fix");
      return;
    }
       
    printGPSData();

    // Data save timer
    if( dataSaveTimer.isExpired() ) {
        // if the latitude != 0.0 then we have a gix 
        if( GPS.latitude != 0.0 ) {
          digitalWrite(redLEDPin, true);
          writeData();
          digitalWrite(redLEDPin, false);
        }
        else {
          // Lat/Long is 0.0, no fix
          digitalWrite(redLEDPin, true);
          digitalWrite(greenLEDPin, false);
          delay(100);
          digitalWrite(redLEDPin, false);
          digitalWrite(greenLEDPin, true);
          
        }
        
        dataSaveTimer.start();
    }

    // create a new file after the specified about of time
    if( newFileTimer.isExpired() ) {
      createSDFile();
      newFileTimer.start();
    }


    if(SERIAL_DEBUG) {
      Serial.println();
    }
  }
}

void writeData() {
  
  if(SERIAL_DEBUG) {
      Serial.println("Writing data -->");
      Serial.print("lat = " );
      Serial.println(GPS.latitude/100,8);
      Serial.print("lng = " );
      Serial.println(GPS.longitude/100,8);
      Serial.print("alt = " );
      Serial.println(GPS.altitude,8);
      Serial.println("----------------");
   }
    
  datafile.print(millis());
  datafile.print(",");
  datafile.print(GPS.latitude/100,8);
  datafile.print(",");
  datafile.println(GPS.longitude/100,8);
  datafile.print(",");
  datafile.println(GPS.altitude,8);
  datafile.flush();
}

//-- this is a raw data dump, from the original Adafruit code, I commented out the extra bits that I'm not using, e.g. time
void printGPSData() {
  if(SERIAL_DEBUG) {
    Serial.println("----------------------------");
    Serial.println("GPS INFO");
//   Serial.print("\nTime: ");
//    Serial.print(GPS.hour, DEC); Serial.print(':');
//    Serial.print(GPS.minute, DEC); Serial.print(':');
//    Serial.print(GPS.seconds, DEC); Serial.print('.');
//    Serial.println(GPS.milliseconds);
//    Serial.print("Date: ");
//    Serial.print(GPS.day, DEC); Serial.print('/');
//    Serial.print(GPS.month, DEC); Serial.print("/20");
//    Serial.println(GPS.year, DEC);
    Serial.print("Fix: ");
    Serial.println((int)GPS.fix);
    
    Serial.print(" quality: ");
    Serial.println((int)GPS.fixquality); 
    
    //if (GPS.fix) {
      //Serial.print("Location: ");
      Serial.print("Latitude: " );
      Serial.println(GPS.latitude/100, 4);
      Serial.print("Lat: " );
      Serial.println(GPS.lat);
      //Serial.print(", "); 

      Serial.print("Longitude: " );
      
      Serial.println(GPS.longitude/100, 4); 
      Serial.print("lon: " );
      Serial.println(GPS.lon);
      
      Serial.print("Altitude: ");
      Serial.println(GPS.altitude);
      
      Serial.print("Satellites: ");
      Serial.println((int)GPS.satellites);
      
    Serial.println("----------------------------");
    //}

  }
}


// read a Hex value and return the decimal equivalent
uint8_t parseHex(char c) {
  if (c < '0')
    return 0;
  if (c <= '9')
    return c - '0';
  if (c < 'A')
    return 0;
  if (c <= 'F')
    return (c - 'A')+10;
}



void initSDCard() {
  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect, 11, 12, 13)) {
    //if (!SD.begin(chipSelect)) {      // if you're using an UNO, you can use this line instead
    
    Serial.println("Card init. failed!");
    error(2);
  }
}

void createSDFile() {
  char filename[32];
  int fileNum = 1;
  
  // The SD Card needs an all caps filename
   while( true ) {
     sprintf(filename, "DATA_%d.CSV", fileNum);
     if( SD.exists(filename) == false ) {
       Serial.print("Opening new file: ");
       Serial.println(filename);
       datafile = SD.open(filename, FILE_WRITE);
       break;
     }
  
     fileNum++;
   }

   if( !datafile ) {
    if(SERIAL_DEBUG) {
      Serial.print("Couldnt create "); 
      Serial.println(filename);
    }
   }
}


void initGPS() {
  // connect to the GPS at the desired rate
  GPS.begin(9600);

  // uncomment this line to turn on RMC (recommended minimum) and GGA (fix data) including altitude
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  // uncomment this line to turn on only the "minimum recommended" data
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  // For logging data, we don't suggest using anything but either RMC only or RMC+GGA
  // to keep the log files at a reasonable size
  // Set the update rate
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);   // 100 millihertz (once every 10 seconds), 1Hz or 5Hz update rate

  // Turn off updates on antenna status, if the firmware permits it
  GPS.sendCommand(PGCMD_NOANTENNA);
}

void readGPSData() {
  // read data from the GPS in the 'main loop'
    char c = GPS.read();
    // if you want to debug, this is a good time to do it!
    if (GPSECHO) {
      if (c) {
          if(SERIAL_DEBUG) 
            Serial.print(c);
      }
    }
  
}

// blink out an error code
void error(uint8_t errno) {
  /*
  if (SD.errorCode()) {
   putstring("SD error: ");
   Serial.print(card.errorCode(), HEX);
   Serial.print(',');
   Serial.println(card.errorData(), HEX);
   }
   */
  while(1) {
    uint8_t i;
    for (i=0; i<errno; i++) {
      digitalWrite(ledPin, HIGH);
      delay(100);
      digitalWrite(ledPin, LOW);
      delay(100);
    }
    for (i=errno; i<10; i++) {
      delay(200);
    }
  }
}

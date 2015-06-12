#include <SoftwareSerial.h>

#include <Adafruit_GPS.h>

#include <Adafruit_FONA.h>

/****************************************************/

const int8_t TIME_ZONE = -4;
char PHONE_NUM[12]  = "12345678910";
                       
// GPS pins
const uint8_t GPS_RX = 11;
const uint8_t GPS_TX = 12;
const uint8_t GPS_EN = 9;

boolean GPS_on = true;
boolean GPS_fix = false;
char location_c[21];
char time_c[17];
char speed_mph_c[5];

SoftwareSerial gpsSS = SoftwareSerial(GPS_TX, GPS_RX);
Adafruit_GPS GPS = Adafruit_GPS(&gpsSS);

//***********//

// Fona pins
const uint8_t FONA_RX = 4;
const uint8_t FONA_TX = 5;
const uint8_t FONA_RST = 6;
const uint8_t FONA_RI_INTERRUPT = 1;

const uint8_t LED_pin = 13;

// this is a large buffer for replies
char replybuffer[255];

SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout = 0);

uint8_t last_sms;
boolean fona_interrupt_set = false;

//***********//

// Interrupt is called once a millisecond, looks for any new GPS data, and stores it
//SIGNAL(TIMER0_COMPA_vect) {
//  char c = GPS.read();
//}

//void useInterrupt(boolean v) {
//  if (v) {
//    // Timer0 is already used for millis() - we'll just interrupt somewhere
//    // in the middle and call the "Compare A" function above
//    OCR0A = 0xAF;
//    TIMSK0 |= _BV(OCIE0A);
//  } else {
//    // do not call the interrupt function COMPA anymore
//    TIMSK0 &= ~_BV(OCIE0A);
//  }
//}

void GPSpower(boolean v) {
  if (GPS_on != v){
    Serial.print(F("Turning GPS ")); 
    (v)? Serial.println(F("ON")) : Serial.println(F("OFF")); 
    digitalWrite(GPS_EN, v);
    //useInterrupt(v);
    GPS_on = v;
  }
}

void got_fona_interrupt()
{
  fona_interrupt_set = true;
  digitalWrite(LED_pin, fona_interrupt_set);
}

//***********//

void setup() {
  pinMode(GPS_EN, OUTPUT);
  digitalWrite(GPS_EN, GPS_on);

  // Fona Startup
  pinMode(LED_pin, OUTPUT);//
  Serial.begin(115200);
  Serial.println(F("Initializing....(May take 3 seconds)"));

  // See if the FONA is responding
  fonaSS.begin(4800);
  if (! fona.begin(fonaSS)) {  // make it slow so its easy to read!
    Serial.println(F("Couldn't find FONA"));
    while (1);
  }
  Serial.println(F("FONA is OK"));

  Serial.print(F("Waiting for GSM network..."));
  while (1) {
    uint8_t network_status = fona.getNetworkStatus();
    if (network_status == 1 || network_status == 5) break;
    delay(250);
  }

  Serial.print(F("Waiting for SMS..."));
  while (1) {
    int8_t smsnum = fona.getNumSMS();
    if (smsnum >= 0){
      Serial.print(smsnum); 
      Serial.println(F(" SMS's on SIM card!"));
      last_sms = smsnum;
      break;
    }
    delay(250);
  }

  // attach interrupt to pin 3 (interrupt 1) for ring interrupt 
  attachInterrupt(FONA_RI_INTERRUPT, got_fona_interrupt, LOW);

  // call our ring interrrupt upon new SMS messages!
  fona.setSMSInterrupt(1);

  // GPS Startup
  
  GPS.begin(9600);
  
  // turn on RMC (recommended minimum) and GGA (fix data) including altitude
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  
  // Set the update rate
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);   // 1 Hz update rate
  
  // Request updates on antenna status, comment out to keep quiet
  GPS.sendCommand(PGCMD_ANTENNA);

  //useInterrupt(true);
  
  printMenu();
}

void handle_sms(int8_t smsnum)
{
  fona_interrupt_set = false;
  
  fonaSS.listen();

  // go through each sms
  for (int8_t smsn=1; smsn<=smsnum; smsn++)
  {
    // read this SMS and check for the word "doorbell"
    Serial.print(F("\n\rReading SMS #")); 
    Serial.println(smsn);
    // Retrieve SMS sender address/phone number.
    if (! fona.getSMSSender(smsn, replybuffer, 250)) {
      Serial.println(F("Failed!"));
      break;
    }
    Serial.print(F("FROM: "));
    Serial.println(replybuffer);
    delay(100);// short numbers fail for some reason without delay.
    // Retrieve SMS value.
    uint16_t smslen;
    if (! fona.readSMS(smsn, replybuffer, 250, &smslen)) { // pass in buffer and max len!
      Serial.println(F("Failed!"));
      break;
    }
    Serial.print(F("***** SMS #")); 
    Serial.print(smsn); 
    Serial.print(" ("); 
    Serial.print(smslen); 
    Serial.println(F(") bytes *****"));
    Serial.println(replybuffer);
    Serial.println(F("*****"));
    
    // we use the C function `strstr` because we're dealing with a character array,
    // not the typical arduino "string" object
    if (strstr(replybuffer, "balance\0") != NULL) // Using Stored SMS comp
    {
      Serial.println(F("Keyword match"));
      sendStatusSms(PHONE_NUM);
    }
  }
    // update sms number
    last_sms = smsnum;
}

void printMenu(void) {
   Serial.println(F("-------------------------------------"));
   Serial.println(F("[?] Print this menu"));
   Serial.println(F("[g] Toggle GPS"));
   Serial.println(F("[i] read RSSI"));
   Serial.println(F("[c] make phone Call"));
   Serial.println(F("[h] Hang up phone"));
   Serial.println(F("[p] Pick up phone"));
   Serial.println(F("[N] Number of SMSs"));
   Serial.println(F("[r] Read SMS #"));
   Serial.println(F("[R] Read All SMS"));
   Serial.println(F("[d] Delete SMS #"));
   Serial.println(F("[s] Send SMS"));
   Serial.println(F("[S] Status"));      
   Serial.println(F("-------------------------------------"));
   Serial.println(F(""));
  
}

void sendStatusSms(char * sendto) {
  // send an SMS!
      char gpsStatus[4];
      if (GPS_on){
        if (GPS_fix){
          strcpy(gpsStatus,"YES");
        } else {
          strcpy(gpsStatus,"NO ");
        }
      } else {
        strcpy(gpsStatus,"OFF");
      }
      sprintf(replybuffer,"GPS: %s\n%s\n%s\n%s mph",gpsStatus,time_c,location_c,speed_mph_c);
      Serial.print(F("Send to #"));
      Serial.println(sendto);
      Serial.println(F("Message: "));
      Serial.println(replybuffer);

      // Not sending because prepaid.
      //if (!fona.sendSMS(sendto, message)) {
      //  Serial.println(F("Failed"));
      //} else {
      //  Serial.println(F("Sent!"));
      //}
}

void printStatus(void) {

  Serial.print(F("GPS: "));
  if (GPS_on){
    Serial.println(F("ON"));
    Serial.print(F("GPS Fix: ")); 
    if (GPS_fix){
      Serial.println(F("YES"));
      //Print location
      Serial.print(F("Time: "));
      Serial.println(time_c);
      Serial.println(F("Location:"));
      Serial.print(F("http://maps.google.com/maps?z=12&t=m&q=loc:"));
      Serial.println(location_c);
      Serial.print(F("Speed: ")); Serial.println(speed_mph_c);
    } else {
      Serial.println(F("NO"));
    }
  } else {
    Serial.println(F("OFF"));
  }

  Serial.println(F("FONA:"));
  fonaSS.listen();
  // battery voltage and percentage
  uint16_t vbat;
  if (! fona.getBattVoltage(&vbat)) {
    Serial.println(F("Failed to read Batt"));
  } else {
    Serial.print(F("VBat = ")); Serial.print(vbat); Serial.println(F(" mV"));
  }
  if (! fona.getBattPercent(&vbat)) {
    Serial.println(F("Failed to read Batt"));
  } else {
    Serial.print(F("VPct = ")); Serial.print(vbat); Serial.println(F("%"));
  }

  // network/cellular status
  uint8_t n = fona.getNetworkStatus();
  Serial.print(F("Network status ")); 
  Serial.print(n);
  Serial.print(F(": "));
  if (n == 0) Serial.println(F("Not registered"));
  if (n == 1) Serial.println(F("Registered (home)"));
  if (n == 2) Serial.println(F("Not registered (searching)"));
  if (n == 3) Serial.println(F("Denied"));
  if (n == 4) Serial.println(F("Unknown"));
  if (n == 5) Serial.println(F("Registered roaming"));

  // read the number of SMS's!
  int8_t smsnum = fona.getNumSMS();
  if (smsnum < 0) {
    Serial.println(F("Could not read # SMS"));
  } else {
    Serial.print(smsnum); 
    Serial.println(F(" SMS's on SIM card!"));
  }

}


uint32_t timer = millis();
uint32_t timera = millis();
void loop() {
  Serial.print(F("> "));
  while (! Serial.available() ){
    if (millis() - timera > 4400) {
      Serial.println(F("Loop")); //Running out of mem. Sometime freezes 
      fonaSS.listen();
      timera = millis();
    }
    digitalWrite(LED_pin, fona_interrupt_set);
    
    if (fona_interrupt_set){
      Serial.println(F("Handling Interrupt"));
      fonaSS.listen();
      int8_t smsnum = fona.getNumSMS();
      if (smsnum < 0) {
          Serial.println(F("Error getting SMS"));
          gpsSS.listen();
          return;
      }
      fona_interrupt_set = false;
      ////Debug
      Serial.print(last_sms); 
      Serial.println(F(" SMS's in mem"));
      Serial.print(smsnum); 
      Serial.println(F(" SMS's on SIM"));
      ////Debug
      if ( smsnum >= last_sms ){
        Serial.println(F("Is SMS. Handling"));
        handle_sms(smsnum);
      }
    }
    
    gpsSS.listen();
    char c = GPS.read(); //Poke GPS.
    if (GPS.newNMEAreceived() && GPS.parse(GPS.lastNMEA())) {
      GPS_fix = GPS.fix && GPS.HDOP < 5 && GPS.HDOP != 0;

      if (millis() - timer > 4400) {
        uint8_t satelliteCount = (uint8_t)GPS.satellites;
        Serial.print(F("Satellites: ")); Serial.println(satelliteCount);
        if (GPS_fix) {
          
          dtostrf(GPS.latitudeDegrees, 9, 4, location_c);
          
          char lon[10];
          dtostrf(fabs(GPS.longitudeDegrees), 9, 4, lon);
          sprintf(location_c,"%s+-%s",location_c,lon);
          
          //Pad with '0' since dtostrf does not (for pretty url)
          for( int i = 0 ; i < 20 ; ++i ){
            if(location_c[i] == ' '){
              location_c[i] = '0';
            }
          }

          //Serial.println(location_c);
          //satelliteCount = GPS.satellites
          dtostrf((GPS.speed * 1.15077945), 4, 1, speed_mph_c);
          //Serial.println(speed_mph_c);
          //Serial.print("\nTime: ");
          uint8_t hour = GPS.hour;
          uint8_t day = GPS.day;
          if (hour >= abs(TIME_ZONE)){
            hour = hour + TIME_ZONE ;
          } else {
            // Correct for day differences
            hour = (hour + 24) + TIME_ZONE ;
            day = (TIME_ZONE < 0)? day - 1 : day + 1; 
          }
          sprintf(time_c,"%u/%u/%u %u:%u:%u",day,GPS.month,GPS.year,hour,GPS.minute,GPS.seconds);
          //Serial.println(time_c);
          //Serial.print(hour, DEC); Serial.print(':');
          //Serial.print(GPS.minute, DEC); Serial.print(':');
          //Serial.print(GPS.seconds, DEC); //Serial.print('.');
          
          //Serial.println(GPS.milliseconds);
          //Serial.print("Date: ");
          //Serial.print(GPS.day, DEC); Serial.print('/');
          //Serial.print(GPS.month, DEC); Serial.print("/20");
          //Serial.println(GPS.year, DEC);
          //Serial.print("Fix: "); Serial.print((int)GPS.fix);
          //Serial.print(" quality: "); Serial.println((int)GPS.fixquality); 
          //Serial.print("Location: ");
          //Serial.print(GPS.latitude, 4); Serial.print(GPS.lat);
          //Serial.print(", "); 
          //Serial.print(GPS.longitude, 4); Serial.println(GPS.lon);
          //Serial.print("Location (in degrees, works with Google Maps): ");
          //Serial.print(GPS.latitudeDegrees, 4);
          //Serial.print(", "); 
          //Serial.println(GPS.longitudeDegrees, 4);
          //Serial.print("Speed (knots): "); Serial.println(GPS.speed);
          //Serial.print("Angle: "); Serial.println(GPS.angle);
          //Serial.print("Altitude: "); Serial.println(GPS.altitude);
          //Serial.print("Satellites: "); Serial.println((int)GPS.satellites);
        } else {
          //Serial.println(F("No valid fix."));
        }
        timer = millis();
      }
    }
  }

  //Left a chunk of adafruit menu for testing.
  char command = Serial.read();
  Serial.println(command);
  fonaSS.listen();
  switch (command) {
    case '?': {
      printMenu();
      break;
    }
    
    case 'g': {
      // read the ADC
      GPS_on? GPSpower(false) : GPSpower(true);
      break;
    }

    case 'i': {
        // read the RSSI
        uint8_t n = fona.getRSSI();
        int8_t r;
        
        Serial.print(F("RSSI = ")); Serial.print(n); Serial.print(": ");
        if (n == 0) r = -115;
        if (n == 1) r = -111;
        if (n == 31) r = -52;
        if ((n >= 2) && (n <= 30)) {
          r = map(n, 2, 30, -110, -54);
        }
        Serial.print(r); Serial.println(F(" dBm"));
       
        break;
    }

    /*** Call ***/
    case 'c': {      
      // call a phone!
      char number[30];
      flushSerial();
      Serial.print(F("Call #"));
      readline(number, 30);
      Serial.println();
      Serial.print(F("Calling ")); Serial.println(number);
      if (!fona.callPhone(number)) {
        Serial.println(F("Failed"));
      } else {
        Serial.println(F("Sent!"));
      }
      
      break;
    }
    case 'h': {
       // hang up! 
      if (! fona.hangUp()) {
        Serial.println(F("Failed"));
      } else {
        Serial.println(F("OK!"));
      }
      break;     
    }

    case 'p': {
       // pick up! 
      if (! fona.pickUp()) {
        Serial.println(F("Failed"));
      } else {
        Serial.println(F("OK!"));
      }
      break;     
    }
    
    /*** SMS ***/
    
    case 'N': {
        // read the number of SMS's!
        int8_t smsnum = fona.getNumSMS();
        if (smsnum < 0) {
          Serial.println(F("Could not read # SMS"));
        } else {
          Serial.print(smsnum); 
          Serial.println(F(" SMS's on SIM card!"));
        }
        break;
    }
    case 'r': {
      // read an SMS
      flushSerial();
      Serial.print(F("Read #"));
      uint8_t smsn = readnumber();
      Serial.print(F("\n\rReading SMS #")); Serial.println(smsn);

      // Retrieve SMS sender address/phone number.
      if (! fona.getSMSSender(smsn, replybuffer, 250)) {
        Serial.println("Failed!");
        break;
      }
      Serial.print(F("FROM: ")); Serial.println(replybuffer);
      delay(100);
      // Retrieve SMS value.
      uint16_t smslen;
      if (! fona.readSMS(smsn, replybuffer, 250, &smslen)) { // pass in buffer and max len!
        Serial.println("Failed!");
        break;
      }
      Serial.print(F("***** SMS #")); Serial.print(smsn); 
      Serial.print(" ("); Serial.print(smslen); Serial.println(F(") bytes *****"));
      Serial.println(replybuffer);
      Serial.println(F("*****"));
      
      break;
    }
    case 'R': {
      // read all SMS
      int8_t smsnum = fona.getNumSMS();
      uint16_t smslen;
      for (int8_t smsn=1; smsn<=smsnum; smsn++) {
        Serial.print(F("\n\rReading SMS #")); Serial.println(smsn);
        if (!fona.readSMS(smsn, replybuffer, 250, &smslen)) {  // pass in buffer and max len!
           Serial.println(F("Failed!"));
           break;
        }
        // if the length is zero, its a special case where the index number is higher
        // so increase the max we'll look at!
        if (smslen == 0) {
          Serial.println(F("[empty slot]"));
          smsnum++;
          continue;
        }
        
        Serial.print(F("***** SMS #")); Serial.print(smsn); 
        Serial.print(" ("); Serial.print(smslen); Serial.println(F(") bytes *****"));
        Serial.println(replybuffer);
        Serial.println(F("*****"));
      }
      break;
    }

    case 'd': {
      // delete an SMS
      flushSerial();
      Serial.print(F("Delete #"));
      uint8_t smsn = readnumber();
      
      Serial.print(F("\n\rDeleting SMS #")); Serial.println(smsn);
      if (fona.deleteSMS(smsn)) {
        Serial.println(F("OK!"));
      } else {
        Serial.println(F("Couldn't delete"));
      }
      break;
    }
    
    case 's': {
      // send an SMS!
      char sendto[21], message[141];
      flushSerial();
      Serial.print(F("Send to #"));
      readline(sendto, 20);
      Serial.println(sendto);
      Serial.print(F("Type out one-line message (140 char): "));
      readline(message, 140);
      Serial.println(message);
      if (!fona.sendSMS(sendto, message)) {
        Serial.println(F("Failed"));
      } else {
        Serial.println(F("Sent!"));
      }
      
      break;
    }

    case 'S': {
      printStatus();
      break;
    }

    case 'z': {
        sendStatusSms(PHONE_NUM);
      break;
    }
    
    /*****************************************/
    
    default: {
      Serial.println(F("Unknown command"));
      printMenu();
      break;
    }
  }
  // flush input
  flushSerial();
  fonaSS.listen();
  while (fona.available()) {
    Serial.println(F("1"));
    Serial.write(fona.read());
  }
  gpsSS.listen();
  while (gpsSS.available()) {
    Serial.println(F("2"));
    Serial.write(gpsSS.read());
  }
}

void flushSerial() {
    while (Serial.available()) 
    Serial.read();
}

char readBlocking() {
  while (!Serial.available());  
  return Serial.read();
}
uint16_t readnumber() {
  uint16_t x = 0;
  char c;
  while (! isdigit(c = readBlocking())) {
    //Serial.print(c);
  }
  Serial.print(c);
  x = c - '0';
  while (isdigit(c = readBlocking())) {
    Serial.print(c);
    x *= 10;
    x += c - '0';
  }
  return x;
}
  
uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout) {
  uint16_t buffidx = 0;
  boolean timeoutvalid = true;
  if (timeout == 0) timeoutvalid = false;
  
  while (true) {
    if (buffidx > maxbuff) {
      //Serial.println(F("SPACE"));
      break;
    }

    while(Serial.available()) {
      char c =  Serial.read();

      //Serial.print(c, HEX); Serial.print("#"); Serial.println(c);

      if (c == '\r') continue;
      if (c == 0xA) {
        if (buffidx == 0)   // the first 0x0A is ignored
          continue;
        
        timeout = 0;         // the second 0x0A is the end of the line
        timeoutvalid = true;
        break;
      }
      buff[buffidx] = c;
      buffidx++;
    }
    
    if (timeoutvalid && timeout == 0) {
      //Serial.println(F("TIMEOUT"));
      break;
    }
    delay(1);
  }
  buff[buffidx] = 0;  // null term
  return buffidx;
}

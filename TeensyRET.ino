/*
 GEV-RET.ino

 Created: 7/2/2014 10:10:14 PM
 Author: Collin Kidder

Copyright (c) 2014-2015 Collin Kidder, Michael Neuweiler, Charles Galpin

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#include "TeensyRET.h"
#include "config.h"
#include "SerialConsole.h"
#include <EEPROM.h>
#include <FlexCAN.h>
#include <Wire.h>
//#include <SdFat.h>
#include <evilOLED.h>
int sda = 18;
int scl = 19;
evilOLED disp(sda, scl);
bool rxactivitytoggle = false;
int framecounter = 0;


byte i = 0;

byte serialBuffer[SER_BUFF_SIZE];
int serialBufferLength = 0; //not creating a ring buffer. The buffer should be large enough to never overflow
uint32_t lastFlushMicros = 0;

EEPROMSettings settings;
SystemSettings SysSettings;
DigitalCANToggleSettings digToggleSettings;

// file system on sdcard
//SdFatSdio sd;

SerialConsole console;

bool digTogglePinState;
uint8_t digTogglePinCounter;

//initializes all the system EEPROM values. Chances are this should be broken out a bit but
//there is only one checksum check for all of them so it's simple to do it all here.
void loadSettings() {
  EEPROM.get(EEPROM_ADDRESS, settings);

  if (settings.version != EEPROM_VER) //if settings are not the current version then erase them and set defaults
  {
    Logger::console("Resetting to factory defaults");
    settings.version = EEPROM_VER;
    settings.appendFile = false;
    settings.Can0Speed = 500000;
    settings.Can0_Enabled = true;
    sprintf((char *)settings.fileNameBase, "CANBUS");
    sprintf((char *)settings.fileNameExt, "TXT");
    settings.fileNum = 1;
    for (int i = 0; i < 6; i++) 
    {
      settings.Can0Filters[i].enabled = true;
      settings.Can0Filters[i].extended = true;
      settings.Can0Filters[i].id = 0;
      settings.Can0Filters[i].mask = 0;
      settings.Can0Filters[i].enabled = true;
      settings.Can0Filters[i].extended = true;
      settings.Can0Filters[i].id = 0;
      settings.Can0Filters[i].mask = 0;
    }
    settings.fileOutputType = CRTD;
    settings.useBinarySerialComm = false;
    settings.autoStartLogging = false;
    settings.logLevel = 1; //info
    settings.sysType = 0; //CANDUE as default
    settings.valid = 0; //not used right now
    settings.Can0ListenOnly = false;
    EEPROM.put(EEPROM_ADDRESS, settings);
  }
  else {
    Logger::console("Using stored values from EEPROM");
        if (settings.Can1ListenOnly > 1) settings.Can0ListenOnly = 0;
        if (settings.Can1ListenOnly > 1) settings.Can1ListenOnly = 0;
  }
  
  EEPROM.get(EEPROM_ADDRESS + 500, digToggleSettings);
  if (digToggleSettings.mode == 255)
    {
        Logger::console("Resetting digital toggling system to defaults");
        digToggleSettings.enabled = false;
        digToggleSettings.length = 0;
        digToggleSettings.mode = 0;
        digToggleSettings.pin = 1;
        digToggleSettings.rxTxID = 0x700;
        for (int c=0 ; c<8 ; c++) digToggleSettings.payload[c] = 0;
        EEPROM.put(EEPROM_ADDRESS + 500, digToggleSettings);        
    }
    else
    {
        Logger::console("Using stored values for digital toggling system");
    }
    
  Logger::setLoglevel((Logger::LogLevel)settings.logLevel);

  SysSettings.SDCardInserted = false;

  //switch (settings.sysType) {
        
    //case 1:  
      Logger::console("Running on Teensy Hardware");
      SysSettings.Can0EnablePin = 2;
      SysSettings.LED_CANTX = 13; //We do have an LED at pin 13. Use it for both
      SysSettings.LED_CANRX = 13; //RX and TX.
      SysSettings.LED_LOGGING = 255; //we just don't have an LED to use for this.
      pinMode(13, OUTPUT);
      digitalWrite(13, LOW);
      //break;
    //}
  if (SysSettings.Can0EnablePin != 255) pinMode(SysSettings.Can0EnablePin, OUTPUT);
}

void setup() {

  CAN_filter_t allPassFilter; // Enables extended addresses
  allPassFilter.id = 0;
  allPassFilter.ext = 1;
  allPassFilter.rtr = 0;

  pinMode(sda, OUTPUT);
  pinMode(scl, OUTPUT);
  evilOLED disp(sda, scl);
  delay(100);
  disp.cls(0x00);
  disp.setCursor(2, 3); // sets text cursor (x,y)
  disp.putString("INITIALISING");
  delay(100);
  
  delay(1000); //just for testing. Don't use in production
  pinMode(BLINK_LED, OUTPUT);
  digitalWrite(BLINK_LED, LOW);

  Serial.begin(115200);
  //Wire.begin();

  loadSettings();

    /*
    if (SysSettings.useSD) {  
    //if (!sd.begin()) {
    //  Logger::error("Could not initialize SDCard! No file logging will be possible!");
    //}
    else SysSettings.SDCardInserted = true;
    if (settings.autoStartLogging) {
      SysSettings.logToFile = true;
      Logger::info("Automatically logging to file.");
    }
  }
 */

    Serial.print("Build number: ");
    Serial.println(CFG_BUILD_NUM);
    
    if (digToggleSettings.enabled)
    {
        Serial.println("Digital Toggle System Enabled");
        if (digToggleSettings.mode & 1) { //input CAN and output pin state mode
            Serial.println("In Output Mode");
            pinMode(digToggleSettings.pin, OUTPUT);
            if (digToggleSettings.mode & 0x80) {
                digitalWrite(digToggleSettings.pin, LOW);
                digTogglePinState = false;
            }
            else {
                digitalWrite(digToggleSettings.pin, HIGH);
                digTogglePinState = true;
            }
        }
        else { //read pin and output CAN mode
            Serial.println("In Input Mode");
            pinMode(digToggleSettings.pin, INPUT);
            digTogglePinCounter = 0;
            if (digToggleSettings.mode & 0x80) digTogglePinState = false;
            else digTogglePinState = true;          
        }
    }

  if (true)
  {
    Serial.println("Init Can0");
    Can0.begin(settings.Can0Speed);
        if (SysSettings.Can0EnablePin < 255) 
        {
            pinMode(SysSettings.Can0EnablePin, OUTPUT);
            digitalWrite(SysSettings.Can0EnablePin, HIGH);
        }
        if (settings.Can0ListenOnly)
        {
            Can0.setListenOnly(true);
        }
        else
        {
            Can0.setListenOnly(false);
        }
  }
  else {
        Serial.println("Can0 disabled.");
        //TODO: apparently calling end while it isn't inialized actually locks it up
        //Can0.end();
    }
    /*
  for (int i = 0; i < 7; i++) 
  {
    if (settings.Can1Filters[i].enabled) 
    {
      Can0.setRXFilter(i, settings.Can1Filters[i].id,
        settings.Can1Filters[i].mask, settings.Can1Filters[i].extended);
    }
    if (settings.Can1Filters[i].enabled)
    {
      Can0.setRXFilter(i, settings.Can1Filters[i].id,
        settings.Can1Filters[i].mask, settings.Can1Filters[i].extended);
    }
  }
  */
  
  SysSettings.lawicelMode = false;
  SysSettings.lawicelAutoPoll = false;
  SysSettings.lawicelTimestamping = false;
  SysSettings.lawicelPollCounter = 0;

  disp.cls(0x00);
  disp.setCursor(3, 2);
  disp.putString("SEBS TEENSY");
  disp.setCursor(5, 3);
  disp.putString("REVERSE");
  disp.setCursor(3, 4);
  disp.putString("ENGINEERING");
  delay(3000);
  
  //disp.cls(0x00);
  //delay(100);

  for (int filterNum = 4; filterNum < 16; filterNum++) {
    Can0.setFilter(allPassFilter, filterNum);
  }

  Serial.print("Done with init\n");
  digitalWrite(BLINK_LED, HIGH);
  
}

void setPromiscuousMode() {
   //By default there are 7 mailboxes for each device that are RX boxes
  //This sets each mailbox to have an open filter that will accept extended
  //or standard frames
  int filter;
  //extended
  /*
  for (filter = 0; filter < 3; filter++) {
  Can0.setRXFilter(filter, 0, 0, true);
  Can0.setRXFilter(filter, 0, 0, true);
  }  
  //standard
  for (filter = 3; filter < 7; filter++) {
  Can0.setRXFilter(filter, 0, 0, false);
  Can0.setRXFilter(filter, 0, 0, false);
  } 
  */
}

  //Get the value of XOR'ing all the bytes together. This creates a reasonable checksum that can be used
  //to make sure nothing too stupid has happened on the comm.
  uint8_t checksumCalc(uint8_t *buffer, int length) {
  uint8_t valu = 0;
  for (int c = 0; c < length; c++) {
    valu ^= buffer[c];
  }
  return valu;
}

void toggleRXLED(){
  SysSettings.rxToggle = !SysSettings.rxToggle;
  //setLED(SysSettings.LED_CANRX, SysSettings.rxToggle);

  disp.setCursor(3, 5);
  disp.putString(framecounter);
  framecounter++;
  

  if (rxactivitytoggle == true){
    //disp.setCursor(3, 5);
    //disp.putString("ACTIVITY:><");
    digitalWrite(BLINK_LED, HIGH);
    rxactivitytoggle = false;
  } else {
    //disp.setCursor(3, 5);
    //disp.putString("ACTIVITY:<>");
    digitalWrite(BLINK_LED, LOW);
    rxactivitytoggle = true;
  }
  
}

void sendFrameToUSB(CAN_message_t &frame, int whichBus) {
  uint8_t buff[22];
  uint8_t temp;
  uint32_t now = micros();
  
  if (SysSettings.lawicelMode)
  {
    if (frame.ext)
    {
      Serial.print("T");
      sprintf((char *)buff, "%08x", frame.id);
      Serial.print((char *)buff);
    }
    else
    {
      Serial.print("t");
      sprintf((char *)buff, "%03x", frame.id);
      Serial.print((char *)buff);
    }
    Serial.print(frame.len);
    for (int i = 0; i < frame.len; i++)
    {
      sprintf((char *)buff, "%02x", frame.buf[i]);
      Serial.print((char *)buff);
    }
    if (SysSettings.lawicelTimestamping)
    {
      uint16_t timestamp = (uint16_t)millis();
      sprintf((char *)buff, "%04x", timestamp);
      Serial.print((char *)buff);
    }
    Serial.write(13);
  }
  else
  {
    if (settings.useBinarySerialComm) {
      if (frame.ext) frame.id |= 1 << 31;
      serialBuffer[serialBufferLength++] = 0xF1;
      serialBuffer[serialBufferLength++] = 0; //0 = canbus frame sending
      serialBuffer[serialBufferLength++] = (uint8_t)(now & 0xFF);
      serialBuffer[serialBufferLength++] = (uint8_t)(now >> 8);
      serialBuffer[serialBufferLength++] = (uint8_t)(now >> 16);
      serialBuffer[serialBufferLength++] = (uint8_t)(now >> 24);
      serialBuffer[serialBufferLength++] = (uint8_t)(frame.id & 0xFF);
      serialBuffer[serialBufferLength++] = (uint8_t)(frame.id >> 8);
      serialBuffer[serialBufferLength++] = (uint8_t)(frame.id >> 16);
      serialBuffer[serialBufferLength++] = (uint8_t)(frame.id >> 24);
      serialBuffer[serialBufferLength++] = frame.len + (uint8_t)(whichBus << 4);
      for (int c = 0; c < frame.len; c++)
      {
        serialBuffer[serialBufferLength++] = frame.buf[c];
      }
      //temp = checksumCalc(buff, 11 + frame.length);
      temp = 0;
      serialBuffer[serialBufferLength++] = temp;
      //SerialUSB.write(buff, 12 + frame.length);
    }
    else 
    {
      Serial.print(micros());
      Serial.print(" - ");
      Serial.print(frame.id, HEX);
      if (frame.ext) Serial.print(" X ");
      else Serial.print(" S ");
      Serial.print(whichBus);
      Serial.print(" ");
      Serial.print(frame.len);
      for (int c = 0; c < frame.len; c++)
      {
        Serial.print(" ");
        Serial.print(frame.buf[c], HEX);
      }
      Serial.println();
    }
  }
}

void sendFrameToFile(CAN_message_t &frame, int whichBus){
  uint8_t buff[40];
  uint8_t temp;
  uint32_t timestamp;
  if (settings.fileOutputType == BINARYFILE) {
    if (frame.ext) frame.id |= 1 << 31;
    timestamp = micros();
    buff[0] = (uint8_t)(timestamp & 0xFF);
    buff[1] = (uint8_t)(timestamp >> 8);
    buff[2] = (uint8_t)(timestamp >> 16);
    buff[3] = (uint8_t)(timestamp >> 24);
    buff[4] = (uint8_t)(frame.id & 0xFF);
    buff[5] = (uint8_t)(frame.id >> 8);
    buff[6] = (uint8_t)(frame.id >> 16);
    buff[7] = (uint8_t)(frame.id >> 24);
    buff[8] = frame.len + (uint8_t)(whichBus << 4);
    for (int c = 0; c < frame.len; c++)
    {
      buff[9 + c] = frame.buf[c];
    }
    Logger::fileRaw(buff, 9 + frame.len);
  }
  else if (settings.fileOutputType == GVRET)
  {
    sprintf((char *)buff, "%i,%x,%i,%i,%i", millis(), frame.id, frame.ext, whichBus, frame.len);
    Logger::fileRaw(buff, strlen((char *)buff));

    for (int c = 0; c < frame.len; c++)
    {
      sprintf((char *) buff, ",%x", frame.buf[c]);
      Logger::fileRaw(buff, strlen((char *)buff));
    }
    buff[0] = '\r';
    buff[1] = '\n';
    Logger::fileRaw(buff, 2);
  }
  else if (settings.fileOutputType == CRTD)
  {
    int idBits = 11;
    if (frame.ext) idBits = 29;
    sprintf((char *)buff, "%f R%i %x", millis() / 1000.0f, idBits, frame.id);
    Logger::fileRaw(buff, strlen((char *)buff));

    for (int c = 0; c < frame.len; c++)
    {
      sprintf((char *) buff, " %x", frame.buf[c]);
      Logger::fileRaw(buff, strlen((char *)buff));
    }
    buff[0] = '\r';
    buff[1] = '\n';
    Logger::fileRaw(buff, 2);
  }
}

void processDigToggleFrame(CAN_message_t &frame){
    bool gotFrame = false;
    if (digToggleSettings.rxTxID == frame.id)
    {
        if (digToggleSettings.length == 0) gotFrame = true;
        else {
            gotFrame = true;
            for (int c = 0; c < digToggleSettings.length; c++) {
                if (digToggleSettings.payload[c] != frame.buf[c]) {
                    gotFrame = false;
                    break;
                }
            }
        }
    }
    
    if (gotFrame) { //then toggle output pin
        Logger::console("Got special digital toggle frame. Toggling the output!");
        digitalWrite(digToggleSettings.pin, digTogglePinState?LOW:HIGH);
        digTogglePinState = !digTogglePinState;
    }
}

void sendDigToggleMsg() {
    CAN_message_t frame;
    Serial.println("Got digital input trigger.");
    frame.id = digToggleSettings.rxTxID;
    if (frame.id > 0x7FF) frame.ext = 1;
    else frame.ext = 0;
    frame.len = digToggleSettings.length;
    for (int c = 0; c < frame.len; c++) frame.buf[c] = digToggleSettings.payload[c];
    if (digToggleSettings.mode & 2) {
        Serial.println("Sending digital toggle message on Can1");
        Can0.write(frame);
    }
}


void loop(){
    /*
  Loop executes as often as possible all the while interrupts fire in the background.
  The serial comm protocol is as follows:
  All commands start with 0xF1 this helps to synchronize if there were comm issues
  Then the next byte specifies which command this is. 
  Then the command data bytes which are specific to the command
  Lastly, there is a checksum byte just to be sure there are no missed or duped bytes
  Any bytes between checksum and 0xF1 are thrown away
  
  Yes, this should probably have been done more neatly but this way is likely to be the
  fastest and safest with limited function calls
  */

  static int loops = 0;
  CAN_message_t incoming;
  static CAN_message_t build_out_frame;
  static int out_bus;
  int in_byte;
  static byte buff[20];
  static int step = 0;
  static STATE state = IDLE;
  static uint32_t build_int;
  uint8_t temp8;
  uint16_t temp16;
  static bool markToggle = false;
  bool isConnected = false;
  int serialCnt;
  uint32_t now = micros();

  /*if (SerialUSB)*/ isConnected = true;

  //if (!SysSettings.lawicelMode || SysSettings.lawicelAutoPoll || SysSettings.lawicelPollCounter > 0)
  //{
    if (Can0.available()) {
      Can0.read(incoming);
      toggleRXLED();
      if (isConnected) sendFrameToUSB(incoming, 0);
      if (SysSettings.logToFile) sendFrameToFile(incoming, 0);
            if (digToggleSettings.enabled && (digToggleSettings.mode & 1) && (digToggleSettings.mode & 2)) processDigToggleFrame(incoming);
      //fwGotFrame(&incoming);
    }
    if (SysSettings.lawicelPollCounter > 0) SysSettings.lawicelPollCounter--;
  //}
        
  if (digToggleSettings.enabled && !(digToggleSettings.mode & 1)) {
      if (digTogglePinState) { //pin currently high. Look for it going low
          if (!digitalRead(digToggleSettings.pin)) digTogglePinCounter++; //went low, increment debouncing counter 
          else digTogglePinCounter = 0; //whoops, it bounced or never transitioned, reset counter to 0
            
          if (digTogglePinCounter > 3) { //transitioned to LOW for 4 checks in a row. We'll believe it then.
              digTogglePinState = false;
              sendDigToggleMsg();
          }                
      }
      else { //pin currently low. Look for it going high
          if (digitalRead(digToggleSettings.pin)) digTogglePinCounter++; //went high, increment debouncing counter 
          else digTogglePinCounter = 0; //whoops, it bounced or never transitioned, reset counter to 0
            
          if (digTogglePinCounter > 3) { //transitioned to HIGH for 4 checks in a row. We'll believe it then.
              digTogglePinState = true;
              sendDigToggleMsg();
          }                          
      }      
  }

  if (micros() - lastFlushMicros > SER_BUFF_FLUSH_INTERVAL){
    if (serialBufferLength > 0)
    {
      Serial.write(serialBuffer, serialBufferLength);
            serialBufferLength = 0;
      lastFlushMicros = micros();
    }
  }

  serialCnt = 0;
  while (isConnected && (Serial.available() > 0) && serialCnt < 128) {
  serialCnt++;
  in_byte = Serial.read();
     switch (state) {
     case IDLE:
       if (in_byte == 0xF1) state = GET_COMMAND;
       else if (in_byte == 0xE7){
        settings.useBinarySerialComm = true;
        SysSettings.lawicelMode = false;
      }
       else {console.rcvCharacter((uint8_t)in_byte);}
       break;
     case GET_COMMAND:
       switch (in_byte) {
       case 0:
         state = BUILD_CAN_FRAME;
         buff[0] = 0xF1;
         step = 0;
         break;
       case 1:
         state = TIME_SYNC;
         step = 0;
         buff[0] = 0xF1;
         buff[1] = 1; //time sync
         buff[2] = (uint8_t)(now & 0xFF);
         buff[3] = (uint8_t)(now >> 8);
         buff[4] = (uint8_t)(now >> 16);
         buff[5] = (uint8_t)(now >> 24);
         Serial.write(buff, 6);
         break;
       case 2:
         //immediately return the data for digital inputs
               /*
         temp8 = getDigital(0) + (getDigital(1) << 1) + (getDigital(2) << 2) + (getDigital(3) << 3);
         buff[0] = 0xF1;
         buff[1] = 2; //digital inputs
         buff[2] = temp8;
         temp8 = checksumCalc(buff, 2);
         buff[3] = temp8;
         Serial.write(buff, 4);
         */
         state = IDLE;
         break;
       case 3:
         //immediately return data on analog inputs
               /*
         temp16 = getAnalog(0);
         buff[0] = 0xF1;
         buff[1] = 3;
         buff[2] = temp16 & 0xFF;
         buff[3] = uint8_t(temp16 >> 8);
         temp16 = getAnalog(1);
         buff[4] = temp16 & 0xFF;
         buff[5] = uint8_t(temp16 >> 8);
         temp16 = getAnalog(2);
         buff[6] = temp16 & 0xFF;
         buff[7] = uint8_t(temp16 >> 8);
         temp16 = getAnalog(3);
         buff[8] = temp16 & 0xFF;
         buff[9] = uint8_t(temp16 >> 8);
         temp8 = checksumCalc(buff, 9);
         buff[10] = temp8;
         Serial.write(buff, 11);
         */
         state = IDLE;
         break;
       case 4:
         state = SET_DIG_OUTPUTS;
         buff[0] = 0xF1;
         break;
       case 5:
         state = SETUP_CANBUS;
         step = 0;
         buff[0] = 0xF1;
         break;
       case 6:
         //immediately return data on canbus params
         buff[0] = 0xF1;
         buff[1] = 6;
         buff[2] = settings.Can1_Enabled + ((unsigned char)settings.Can1ListenOnly << 4);
         buff[3] = settings.Can1Speed;
         buff[4] = settings.Can1Speed >> 8;
         buff[5] = settings.Can1Speed >> 16;
         buff[6] = settings.Can1Speed >> 24;
         buff[7] = settings.Can1_Enabled + ((unsigned char)settings.Can1ListenOnly << 4);
         buff[8] = settings.Can1Speed;
         buff[9] = settings.Can1Speed >> 8;
         buff[10] = settings.Can1Speed >> 16;
         buff[11] = settings.Can1Speed >> 24;
         Serial.write(buff, 12);
         state = IDLE;
         break;
       case 7:
         //immediately return device information
         buff[0] = 0xF1;
         buff[1] = 7;
         buff[2] = CFG_BUILD_NUM & 0xFF;
         buff[3] = (CFG_BUILD_NUM >> 8);
         buff[4] = EEPROM_VER;
         buff[5] = (unsigned char)settings.fileOutputType;
         buff[6] = (unsigned char)settings.autoStartLogging;
         buff[7] = 0;
         Serial.write(buff, 8);
         state = IDLE;
         break; 
       case 8:
         buff[0] = 0xF1;
         state = IDLE;//SET_SINGLEWIRE_MODE;
         step = 0;
         break;
       case 9:
         buff[0] = 0xF1;
         buff[1] = 0x09;
         buff[2] = 0xDE;
         buff[3] = 0xAD;
         Serial.write(buff, 4);
         state = IDLE;
         break;
       case 10:
         buff[0] = 0xF1;
         state = SET_SYSTYPE;
         step = 0;
         break;
       case 11:
         state = ECHO_CAN_FRAME;
         buff[0] = 0xF1;
         step = 0;
         break;
       }
       break;
     case BUILD_CAN_FRAME:
       buff[1 + step] = in_byte;
       switch (step) {
       case 0:
         build_out_frame.id = in_byte;
         break;
       case 1:
         build_out_frame.id |= in_byte << 8;
         break;
       case 2:
         build_out_frame.id |= in_byte << 16;
         break;
       case 3:
         build_out_frame.id |= in_byte << 24;
         if (build_out_frame.id & 1 << 31) 
         {
           build_out_frame.id &= 0x7FFFFFFF;
           build_out_frame.ext = 1;
         }
         else build_out_frame.ext = 0;
         break;
       case 4:
           out_bus = in_byte & 1;
           break;
       case 5:
         build_out_frame.len = in_byte & 0xF;
         if (build_out_frame.len > 8) build_out_frame.len = 8;
         break;
       default:
         if (step < build_out_frame.len + 6)
         {
            build_out_frame.buf[step - 6] = in_byte;
         }
         else 
         {
           state = IDLE;
           //this would be the checksum byte. Compute and compare.
           temp8 = checksumCalc(buff, step);
           //if (temp8 == in_byte) 
           //{
           if (out_bus == 0) Can0.write(build_out_frame);
           if (out_bus == 1) Can0.write(build_out_frame);
           //}
         }
         break;
       }
       step++;
       break;
     case TIME_SYNC:
       state = IDLE;
       break;
     case SET_DIG_OUTPUTS: //todo: validate the XOR byte
       buff[1] = in_byte;
       //temp8 = checksumCalc(buff, 2);
           /*
       for (int c = 0; c < 8; c++) 
       {
         if (in_byte & (1 << c)) setOutput(c, true);
         else setOutput(c, false);
       }
       */
       state = IDLE;
       break;
     case SETUP_CANBUS: //todo: validate checksum
       switch (step)
       {
       case 0:
         build_int = in_byte;
         break;
       case 1:
         build_int |= in_byte << 8;
         break;
       case 2:
         build_int |= in_byte << 16;
         break;
       case 3:
         build_int |= in_byte << 24;
         if (build_int > 0) 
         {
           if (build_int & 0x80000000) //signals that enabled and listen only status are also being passed
           {
             if (build_int & 0x40000000)
             {
               settings.Can1_Enabled = true;
             }
             else
             {
               settings.Can1_Enabled = false;
             }
             if (build_int & 0x20000000)
             {
               settings.Can1ListenOnly = true;
               Can0.setListenOnly(true);
             }
             else
             {
               settings.Can1ListenOnly = false;
               Can0.setListenOnly(false);
             }
           }
           else
           {
             settings.Can1_Enabled = true;
           }
           build_int = build_int & 0xFFFFF;
           if (build_int > 1000000) build_int = 1000000;           
           Can0.begin(build_int);
                   if (SysSettings.Can1EnablePin < 255 && settings.Can1_Enabled)
                   {
                       pinMode(SysSettings.Can1EnablePin, OUTPUT);
                       digitalWrite(SysSettings.Can1EnablePin, HIGH);
                   }
                   else digitalWrite(SysSettings.Can1EnablePin, LOW);
           //Can0.set_baudrate(build_int);
           settings.Can1Speed = build_int;           
         }
         else //disable first canbus
         {
           Can0.end();
                   digitalWrite(SysSettings.Can1EnablePin, LOW);
           settings.Can1_Enabled = false;
         }
         break;
       case 4:
         build_int = in_byte;
         break;
       case 5:
         build_int |= in_byte << 8;
         break;
       case 6:
         build_int |= in_byte << 16;
         break;
       case 7:
         build_int |= in_byte << 24;
         if (build_int > 0) 
         {
           if (build_int & 0x80000000) //signals that enabled and listen only status are also being passed
           {
             if (build_int & 0x40000000)
             {
               settings.Can1_Enabled = true;
             }
             else
             {
               settings.Can1_Enabled = false;
             }
             if (build_int & 0x20000000)
             {
               settings.Can1ListenOnly = true;
               Can0.setListenOnly(true);
             }
             else
             {
               settings.Can1ListenOnly = false;
               Can0.setListenOnly(false);
             }
           }
           else
           {
             settings.Can1_Enabled = true;
           }
           build_int = build_int & 0xFFFFF;
           if (build_int > 1000000) build_int = 1000000;
           Can0.begin(build_int);
                   if (SysSettings.Can1EnablePin < 255 && settings.Can1_Enabled)
                   {
                       pinMode(SysSettings.Can1EnablePin, OUTPUT);
                       digitalWrite(SysSettings.Can1EnablePin, HIGH);
                   }
                   else digitalWrite(SysSettings.Can1EnablePin, LOW);
           //Can0.set_baudrate(build_int);

           settings.Can1Speed = build_int;           
         }
         else //disable second canbus
         {
           Can0.end();
                   digitalWrite(SysSettings.Can1EnablePin, LOW);
           settings.Can1_Enabled = false;
         }
         state = IDLE;
          //now, write out the new canbus settings to EEPROM
        EEPROM.put(EEPROM_ADDRESS, settings);
        setPromiscuousMode();
         break;
       }
       step++;
       break;
     case SET_SINGLEWIRE_MODE:
       state = IDLE;
       break;
     case SET_SYSTYPE:
       settings.sysType = in_byte;       
       EEPROM.put(EEPROM_ADDRESS, settings);
       loadSettings();
       state = IDLE;
       break;
     case ECHO_CAN_FRAME:
       buff[1 + step] = in_byte;
       switch (step) {
       case 0:
         build_out_frame.id = in_byte;
         break;
       case 1:
         build_out_frame.id |= in_byte << 8;
         break;
       case 2:
         build_out_frame.id |= in_byte << 16;
         break;
       case 3:
         build_out_frame.id |= in_byte << 24;
         if (build_out_frame.id & 1 << 31) 
         {
           build_out_frame.id &= 0x7FFFFFFF;
           build_out_frame.ext = 1;
         }
         else build_out_frame.ext = 0;
         break;
       case 4:
           out_bus = in_byte & 1;
           break;
       case 5:
         build_out_frame.len = in_byte & 0xF;
         if (build_out_frame.len > 8) build_out_frame.len = 8;
         break;
       default:
         if (step < build_out_frame.len + 6)
         {
            build_out_frame.buf[step - 6] = in_byte;
         }
         else 
         {
           state = IDLE;
           //this would be the checksum byte. Compute and compare.
           temp8 = checksumCalc(buff, step);
           //if (temp8 == in_byte) 
           //{
           toggleRXLED();
           if (isConnected) sendFrameToUSB(build_out_frame, 0);
           //}
         }
         break;
       }
       step++;
       break;
     }
  }
  Logger::loop();
}

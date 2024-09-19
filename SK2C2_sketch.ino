

// =======================================================================================
// /////////////////////////Padawan360 Body Code - Mega I2C v2.0 ////////////////////////////////////
// =======================================================================================
/*
by Dan Kraus
dskraus@gmail.com
Astromech: danomite4047
Project Site: https://github.com/dankraus/padawan360/

Heavily influenced by DanF's Padwan code which was built for Arduino+Wireless PS2
controller leveraging Bill Porter's PS2X Library. I was running into frequent disconnect
issues with 4 different controllers working in various capacities or not at all. I decided
that PS2 Controllers were going to be more difficult to come by every day, so I explored
some existing libraries out there to leverage and came across the USB Host Shield and it's
support for PS3 and Xbox 360 controllers. Bluetooth dongles were inconsistent as well
so I wanted to be able to have something with parts that other builder's could easily track
down and buy parts even at your local big box store.

v2.0 Changes:
- Makes left analog stick default drive control stick. Configurable between left or right stick via isLeftStickDrive 

Hardware:
***Arduino Mega 2560***
USB Host Shield from circuits@home
Microsoft Xbox 360 Controller
Xbox 360 USB Wireless Reciver
Sabertooth Motor Controller
Syren Motor Controller
Sparkfun MP3 Trigger

This sketch supports I2C and calls events on many sound effect actions to control lights and sounds.
It is NOT set up for Dan's method of using the serial packet to transfer data up to the dome
to trigger some light effects.It uses Hardware Serial pins on the Mega to control Sabertooth and Syren

Set Sabertooth 2x25/2x12 Dip Switches 1 and 2 Down, All Others Up
For SyRen Simple Serial Set Switches 1 and 2 Down, All Others Up
For SyRen Simple Serial Set Switchs 2 & 4 Down, All Others Up
Placed a 10K ohm resistor between S1 & GND on the SyRen 10 itself

HCR INSTRUCTIONS
** Order of set up ** 
// Initialize Vocalizer

void setup() {
  // Setup Vocalizer Parameters
}

void loop() {
  // Issue Commands to Vocalizer
  // Update Vocaliser
  // Get Responses from Vocalizer
}
*/

// ************************** Options, Configurations, and Settings ***********************************


// SPEED AND TURN SPEEDS
//set these 3 to whatever speeds work for you. 0-stop, 127-full speed.
const byte DRIVESPEED1 = 50;
// Recommend beginner: 50 to 75, experienced: 100 to 127, I like 100. 
// These may vary based on your drive system and power system
const byte DRIVESPEED2 = 100;
//Set to 0 if you only want 2 speeds.
const byte DRIVESPEED3 = 127;

// Default drive speed at startup
byte drivespeed = DRIVESPEED1;

// the higher this number the faster the droid will spin in place, lower - easier to control.
// Recommend beginner: 40 to 50, experienced: 50 $ up, I like 70
// This may vary based on your drive system and power system
const byte TURNSPEED = 70;

// Set isLeftStickDrive to true for driving  with the left stick
// Set isLeftStickDrive to false for driving with the right stick (legacy and original configuration)
boolean isLeftStickDrive = true; 

// If using a speed controller for the dome, sets the top speed. You'll want to vary it potenitally
// depending on your motor. My Pittman is really fast so I dial this down a ways from top speed.
// Use a number up to 127 for serial
const byte DOMESPEED = 110;

// Ramping- the lower this number the longer R2 will take to speedup or slow down,
// change this by incriments of 1
const byte RAMPING = 5;

// Compensation is for deadband/deadzone checking. There's a little play in the neutral zone
// which gets a reading of a value of something other than 0 when you're not moving the stick.
// It may vary a bit across controllers and how broken in they are, sometimex 360 controllers
// develop a little bit of play in the stick at the center position. You can do this with the
// direct method calls against the Syren/Sabertooth library itself but it's not supported in all
// serial modes so just manage and check it in software here
// use the lowest number with no drift
// DOMEDEADZONERANGE for the left stick, DRIVEDEADZONERANGE for the right stick
const byte DOMEDEADZONERANGE = 20;
const byte DRIVEDEADZONERANGE = 20;

// Set the baude rate for the Sabertooth motor controller (feet)
// 9600 is the default baud rate for Sabertooth packet serial.
// for packetized options are: 2400, 9600, 19200 and 38400. I think you need to pick one that works
// and I think it varies across different firmware versions.
const int SABERTOOTHBAUDRATE = 9600;

// Set the baude rate for the Syren motor controller (dome)
// for packetized options are: 2400, 9600, 19200 and 38400. I think you need to pick one that works
// and I think it varies across different firmware versions.
const int DOMEBAUDRATE = 9600;

// Default sound volume at startup
// 0 = full volume, 255 off
byte vol = 20;


// Automation Delays
// set automateDelay to min and max seconds between sounds
byte automateDelay = random(5, 20); 
//How much the dome may turn during automation.
int turnDirection = 20;

// Pin number to pull a relay high/low to trigger my upside down compressed air like R2's extinguisher
#define EXTINGUISHERPIN 3



#include <Sabertooth.h>
#include <Wire.h>
#include <XBOXRECV.h>
#include <hcr.h>
#include <Usb.h>
#include <SyRenSimplified.h>

/////////////////////////////////////////////////////////////////
Sabertooth Sabertooth2x(128, Serial1);
Sabertooth Syren10(128, Serial2);
// Connection via a Serial Port on certain pins
HCRVocalizer HCR(19,18,9600); // Serial (RX Pin, TX Pin, baud rate)

// Satisfy IDE, which only needs to see the include statment in the ino.
#ifdef dobogusinclude
#include <spi4teensy3.h>
#endif

// Set some defaults for start up
// false = drive motors off ( right stick disabled ) at start
boolean isDriveEnabled = false;

// Automated functionality
// Used as a boolean to turn on/off automated functions like periodic random sounds and periodic dome turns
boolean isInAutomationMode = false;
unsigned long automateMillis = 0;
// Action number used to randomly choose a sound effect or a dome turn
byte automateAction = 0;


int driveThrottle = 0;
int throttleStickValue = 0;
int domeThrottle = 0;
int turnThrottle = 0;

boolean firstLoadOnConnect = false;

AnalogHatEnum throttleAxis;
AnalogHatEnum turnAxis;
AnalogHatEnum domeAxis;
ButtonEnum speedSelectButton;
ButtonEnum hpLightToggleButton;


// this is legacy right now. The rest of the sketch isn't set to send any of this
// data to another arduino like the original Padawan sketch does
// right now just using it to track whether or not the HP light is on so we can
// fire the correct I2C event to turn on/off the HP light.
//struct SEND_DATA_STRUCTURE{
//  //put your variable definitions here for the data you want to send
//  //THIS MUST BE EXACTLY THE SAME ON THE OTHER ARDUINO
//  int hpl; // hp light
//  int dsp; // 0 = random, 1 = alarm, 5 = leia, 11 = alarm2, 100 = no change
//};
//SEND_DATA_STRUCTURE domeData;//give a name to the group of data

boolean isHPOn = false;


USB Usb;
XBOXRECV Xbox(&Usb);

      
void setup() {
  Serial1.begin(SABERTOOTHBAUDRATE);
  Serial2.begin(DOMEBAUDRATE);
  HCR.begin(125); // Refresh Speed (Default:125 ms)
  HCR.OverrideEmotions(1); // Override emotions and prevent emotions normalising
  HCR.SetEmotion(HAPPY,89); // Set Happy to 89%
  HCR.SetEmotion(SAD,22); // Set Sadness to 22%
  HCR.SetMuse(false); // Disable Emotion Override
  HCR.Stimulate(HAPPY,EMOTE_MODERATE);

  
}

#if defined(SYRENSIMPLE)
  Syren10.motor(0);
#else
  Syren10.autobaud();
#endif

  // Send the autobaud command to the Sabertooth controller(s).
  /* NOTE: *Not all* Sabertooth controllers need this command.
  It doesn't hurt anything, but V2 controllers use an
  EEPROM setting (changeable with the function setBaudRate) to set
  the baud rate instead of detecting with autobaud.
  If you have a 2x12, 2x25 V2, 2x60 or SyRen 50, you can remove
  the autobaud line and save yourself two seconds of startup delay.
  */
  Sabertooth2x.autobaud();
  // The Sabertooth won't act on mixed mode packet serial commands until
  // it has received power levels for BOTH throttle and turning, since it
  // mixes the two together to get diff-drive power levels for both motors.
  Sabertooth2x.drive(0);
  Sabertooth2x.turn(0);


  Sabertooth2x.setTimeout(950);
  Syren10.setTimeout(950);

  pinMode(EXTINGUISHERPIN, OUTPUT);
  digitalWrite(EXTINGUISHERPIN, HIGH);

  HCR.SetVolume(CH_V,99);
  HCR.SetVolume(CH_A,50);
  HCR.SetVolume(CH_B,50);

  if(isLeftStickDrive) {
    throttleAxis = LeftHatY;
    turnAxis = LeftHatX;
    domeAxis = RightHatX;
    speedSelectButton = L3;
    hpLightToggleButton = R3;

  } else {
    throttleAxis = RightHatY;
    turnAxis = RightHatX;
    domeAxis = LeftHatX;
    speedSelectButton = R3;
    hpLightToggleButton = L3;
  }

}
unsigned long Wait = millis();

void loop() {
  Usb.Task();
  // if we're not connected, return so we don't bother doing anything else.
  // set all movement to 0 so if we lose connection we don't have a runaway droid!
  // a restraining bolt and jawa droid caller won't save us here!
  if (!Xbox.XboxReceiverConnected || !Xbox.Xbox360Connected[0]) {
    Sabertooth2x.drive(0);
    Sabertooth2x.turn(0);
    Syren10.motor(1, 0);
    firstLoadOnConnect = false;
    return;
  }

  // After the controller connects, Blink all the LEDs so we know drives are disengaged at start
  if (!firstLoadOnConnect) {
    firstLoadOnConnect = true;
    HCR.Stimulate(MAD,EMOTE_MODERATE);
    Xbox.setLedMode(ROTATING, 0);
  }
  
  if (Xbox.getButtonClick(XBOX, 0)) {
    if(Xbox.getButtonPress(L1, 0) && Xbox.getButtonPress(R1, 0)){ 
      Xbox.disconnect(0);
    }
  }

  // enable / disable right stick (droid movement) & play a sound to signal motor state
  if (Xbox.getButtonClick(START, 0)) {
    if (isDriveEnabled) {
      isDriveEnabled = false;
      Xbox.setLedMode(ROTATING, 0);
      HCR.Stimulate(SCARED,EMOTE_MODERATE);
    } else {
      isDriveEnabled = true;
      HCR.Overload();
      // //When the drive is enabled, set our LED accordingly to indicate speed
      if (drivespeed == DRIVESPEED1) {
        Xbox.setLedOn(LED1, 0);
      } else if (drivespeed == DRIVESPEED2 && (DRIVESPEED3 != 0)) {
        Xbox.setLedOn(LED2, 0);
      } else {
        Xbox.setLedOn(LED3, 0);
      }
    }
  }

  //Toggle automation mode with the BACK button
  if (Xbox.getButtonClick(BACK, 0)) {
    if (isInAutomationMode) {
      isInAutomationMode = false;
      automateAction = 0;
      HCR.Stimulate(SAD,EMOTE_MODERATE);
    } else {
      isInAutomationMode = true;
      HCR.Stimulate(HAPPY,EMOTE_MODERATE);
       // Plays random sounds or dome movements for automations when in automation mode
  if (isInAutomationMode) {

    if (currentMillis - automateMillis > (automateDelay * 1000)) {
      automateMillis = millis();
      automateAction = random(1, 5);

      if (automateAction > 1) {
        mp3Trigger.play(random(32, 52));
      }
      if (automateAction < 4) {
    }
  }

  // If 10 Seconds have passed
  if (millis() > Wait) {
    // Stimulate Mild Happy Response
    //HCR.Stimulate(HAPPY,EMOTE_MODERATE);

    // Stimulate Strong Sad Response
    // HCR.Stimulate(SAD,EMOTE_STRONG);

    // Trigger Overload
    // HCR.Overload();

    // Stimulate Random Response
     HCR.Stimulate(random(0,4),random(0,2));

    // Reset Timer    
    Wait = millis() + 10000;
  }

  HCR.update();
}
  
#if defined(SYRENSIMPLE)
        Syren10.motor(turnDirection);
#else
        Syren10.motor(1, turnDirection);
#endif

        delay(750);

#if defined(SYRENSIMPLE)
        Syren10.motor(0);
#else
        Syren10.motor(1, 0);
#endif

        if (turnDirection > 0) {
          turnDirection = -45;
        } else {
          turnDirection = 45;
        }
      }

      // sets the mix, max seconds between automation actions - sounds and dome movement
      automateDelay = random(3,10);
    }
  }

  // Volume Control of MP3 Trigger
  // Hold R1 and Press Up/down on D-pad to increase/decrease volume
  if (Xbox.getButtonClick(UP, 0)) {
    // volume up
    if (Xbox.getButtonPress(R1, 0)) {
      if (vol > 0) {
        vol--;
       HCR.SetVolume(vol);
      }
    }
  }
  if (Xbox.getButtonClick(DOWN, 0)) {
    //volume down
    if (Xbox.getButtonPress(R1, 0)) {
      if (vol < 255) {
        vol++;
         HCR.SetVolume(vol));
      }
    }
  }

  // Logic display brightness.
  // Hold L1 and press up/down on dpad to increase/decrease brightness
  if (Xbox.getButtonClick(UP, 0)) {
    if (Xbox.getButtonPress(L1, 0)) {
      triggerI2C(10, 24);
    }
  }
  if (Xbox.getButtonClick(DOWN, 0)) {
    if (Xbox.getButtonPress(L1, 0)) {
      triggerI2C(10, 25);
    }
  }


  //FIRE EXTINGUISHER
  // When holding L2-UP, extinguisher is spraying. WHen released, stop spraying

  // TODO: ADD SERVO DOOR OPEN FIRST. ONLY ALLOW EXTINGUISHER ONCE IT'S SET TO 'OPENED'
  // THEN CLOSE THE SERVO DOOR
  if (Xbox.getButtonPress(L1, 0)) {
    if (Xbox.getButtonPress(UP, 0)) {
      digitalWrite(EXTINGUISHERPIN, LOW);
    } else {
      digitalWrite(EXTINGUISHERPIN, HIGH);
    }
  }


  // GENERAL SOUND PLAYBACK AND DISPLAY CHANGING

  // Y Button and Y combo buttons
  if (Xbox.getButtonClick(Y, 0)) {
    if (Xbox.getButtonPress(L1, 0)) {
      HCR.Stimulate(SAD,EMOTE_STRONG);
      //logic lights, random
      triggerI2C(10, 0);
    } else if (Xbox.getButtonPress(L2, 0)) {
      HCR.Stimulate(MAD,EMOTE_STRONG);
      //logic lights, random
      triggerI2C(10, 0);
    } else if (Xbox.getButtonPress(R1, 0)) {
      HCR.Stimulate(SCARED,EMOTE_MODERATE);
      //logic lights, random
      triggerI2C(10, 0);
    } else {
      HCR.Stimulate(HAPPY,EMOTE_STRONG);
      //logic lights, random
      triggerI2C(10, 0);
    }
  }

  // A Button and A combo Buttons
  if (Xbox.getButtonClick(A, 0)) {
    if (Xbox.getButtonPress(L1, 0)) {
      HCR.PlayWAV(CH_A,"0001");
      //logic lights
      triggerI2C(10, 6);
      // HPEvent 11 - SystemFailure - I2C
      triggerI2C(25, 11);
      triggerI2C(26, 11);
      triggerI2C(27, 11);
    } else if (Xbox.getButtonPress(L2, 0)) {
      HCR.PlayWAV(CH_A,"0002");
      //logic lights, alarm
      triggerI2C(10, 1);
      //  HPEvent 3 - alarm - I2C
      triggerI2C(25, 3);
      triggerI2C(26, 3);
      triggerI2C(27, 3);
    } else if (Xbox.getButtonPress(R1, 0)) {
      HCR.PlayWAV(CH_A,"0003");
      //logic lights, alarm2Display
      triggerI2C(10, 11);
    } else {
      HCR.PlayWAV(CH_A,"0004");
      //logic lights, random
      triggerI2C(10, 0);
    }
  }

  // B Button and B combo Buttons
  if (Xbox.getButtonClick(B, 0)) {
    if (Xbox.getButtonPress(L1, 0)) {
      HCR.PlayWAV(CH_A,"0005");
      //logic lights, random
      triggerI2C(10, 0);
    } else if (Xbox.getButtonPress(L2, 0)) {
      HCR.StopWAV(CH_A,FADE);
      //logic lights, random
      triggerI2C(10, 0);
    } else if (Xbox.getButtonPress(R1, 0)) {
      HCR.StopWAV(CH_B);
      //logic lights bargrap
      triggerI2C(10, 10);
      // HPEvent 1 - Disco - I2C
      triggerI2C(25, 10);
      triggerI2C(26, 10);
      triggerI2C(27, 10);
    } else {
      HCR.SetMuse(1);
      //logic lights, random
      triggerI2C(10, 0);
    }
  }

  // X Button and X combo Buttons
  if (Xbox.getButtonClick(X, 0)) {
    // leia message L1+X
    if (Xbox.getButtonPress(L1, 0)) {
      HCR.SetMuse(0);
      //logic lights, leia message
      triggerI2C(10, 5);
      // Front HPEvent 1 - HoloMessage - I2C -leia message
      triggerI2C(25, 9);
    } else if (Xbox.getButtonPress(L2, 0)) {
      HCR.Overload();
      //logic lights
      triggerI2C(10, 4);
    } else if (Xbox.getButtonPress(R1, 0)) {
      HCR.Stimulate(SCARED,STRONG);
      //logic lights, random
      triggerI2C(10, 0);
    } else {
      HCR.Stimulate(random(0,4),random(0,2)));
      //logic lights, random
      triggerI2C(10, 0);
    }
  }

  // turn hp light on & off with Right Analog Stick Press (R3) for left stick drive mode
  // turn hp light on & off with Left Analog Stick Press (L3) for right stick drive mode
  if (Xbox.getButtonClick(hpLightToggleButton, 0))  {
    // if hp light is on, turn it off
    if (isHPOn) {
      isHPOn = false;
      // turn hp light off
      // Front HPEvent 2 - ledOFF - I2C
      triggerI2C(25, 2);
    } else {
      isHPOn = true;
      // turn hp light on
      // Front HPEvent 4 - whiteOn - I2C
      triggerI2C(25, 1);
    }
  }


  // Change drivespeed if drive is enabled
  // Press Left Analog Stick (L3) for left stick drive mode
  // Press Right Analog Stick (R3) for right stick drive mode
  // Set LEDs for speed - 1 LED, Low. 2 LED - Med. 3 LED High
  if (Xbox.getButtonClick(speedSelectButton, 0) && isDriveEnabled) {
    //if in lowest speed
    if (drivespeed == DRIVESPEED1) {
      //change to medium speed and play sound 3-tone
      drivespeed = DRIVESPEED2;
      Xbox.setLedOn(LED2, 0);
      HCR.Stimulate(HAPPY,EMOTE_MODERATE);
      triggerI2C(10, 22);
    } else if (drivespeed == DRIVESPEED2 && (DRIVESPEED3 != 0)) {
      //change to high speed and play sound scream
      drivespeed = DRIVESPEED3;
      Xbox.setLedOn(LED3, 0);
      HCR.Stimulate(MAD,EMOTE_MODERATE);
      triggerI2C(10, 23);
    } else {
      //we must be in high speed
      //change to low speed and play sound 2-tone
      drivespeed = DRIVESPEED1;
      Xbox.setLedOn(LED1, 0);
      HCR.Stimulate(SCARED,EMOTE_MODERATE);
      triggerI2C(10, 21);
    }
  }


 
  // FOOT DRIVES
  // Xbox 360 analog stick values are signed 16 bit integer value
  // Sabertooth runs at 8 bit signed. -127 to 127 for speed (full speed reverse and  full speed forward)
  // Map the 360 stick values to our min/max current drive speed
  throttleStickValue = (map(Xbox.getAnalogHat(throttleAxis, 0), -32768, 32767, -drivespeed, drivespeed));
  if (throttleStickValue > -DRIVEDEADZONERANGE && throttleStickValue < DRIVEDEADZONERANGE) {
    // stick is in dead zone - don't drive
    driveThrottle = 0;
  } else {
    if (driveThrottle < throttleStickValue) {
      if (throttleStickValue - driveThrottle < (RAMPING + 1) ) {
        driveThrottle += RAMPING;
      } else {
        driveThrottle = throttleStickValue;
      }
    } else if (driveThrottle > throttleStickValue) {
      if (driveThrottle - throttleStickValue < (RAMPING + 1) ) {
        driveThrottle -= RAMPING;
      } else {
        driveThrottle = throttleStickValue;
      }
    }
  }

  turnThrottle = map(Xbox.getAnalogHat(turnAxis, 0), -32768, 32767, -TURNSPEED, TURNSPEED);

  // DRIVE!
  // right stick (drive)
  if (isDriveEnabled) {
    // Only do deadzone check for turning here. Our Drive throttle speed has some math applied
    // for RAMPING and stuff, so just keep it separate here
    if (turnThrottle > -DRIVEDEADZONERANGE && turnThrottle < DRIVEDEADZONERANGE) {
      // stick is in dead zone - don't turn
      turnThrottle = 0;
    }
    Sabertooth2x.turn(-turnThrottle);
    Sabertooth2x.drive(driveThrottle);
  }

  // DOME DRIVE!
  domeThrottle = (map(Xbox.getAnalogHat(domeAxis, 0), -32768, 32767, DOMESPEED, -DOMESPEED));
  if (domeThrottle > -DOMEDEADZONERANGE && domeThrottle < DOMEDEADZONERANGE) {
    //stick in dead zone - don't spin dome
    domeThrottle = 0;
  }

  Syren10.motor(1, domeThrottle);
} // END loop()

void triggerI2C(byte deviceID, byte eventID) {
  Wire.beginTransmission(deviceID);
  Wire.write(eventID);
  Wire.endTransmission();
}

/************************************************************************
 * File Name          : RemoteControl
 * Author             : Evan
 * Updated            : Scott Gray, b3sigma
 * Version            : V0.0.2 SG Version
 * Date               : 21 May, 2014
 * Revision Date      : 22 May, 2014
 * Description        : Mouse Control or Leap Motion Control(Processing)
 * License            :
 * Copyright(C) 2014 UFactory Team. All right reserved.
 *************************************************************************/
#include <Servo.h>
#include <EEPROM.h>
#include <UF_uArmSG.h>

//SG-> Added a slew rate time constant to prevent arm from moving too quickly
#define SLEW_RATE   10  //time in milliseconds between new arm movements
int heightTemp = 0, stretchTemp = 0, rotationTemp = 0, handRotTemp = 0;

//SG-> Added variables to hold the current values of the arm position
int heightCurr = 0, stretchCurr = 0, rotationCurr = 0, handRotCurr = 0;

//SG-> Below is angle variable I will be using soon to control keeping the servos from binding
int angle = 0;

//SG-> Added a variable to keep time to know when to move arm
unsigned long currTime; //limit: 50days

enum StateMachine {
  FrameTypeDection = 0,
  BinaryFrameSecondByte = 1,
  BinaryUpdatePacket = 2,
  TextFrameType = 4,
  TextFrameData = 5,
};

// These are handy if you want to use the serial monitor in the ide
// because you don't have an external app yet or you want aside channel.
enum TextFrameTypes {
  TextFrameGrabber = 'g',
  TextFrameHeight = 'h',
  TextFrameRotate = 'r',
  TextFramePrintState = 'p',
  TextFrameStretch = 's',
};

char textFrameType = 0;
char stateMachine = FrameTypeDection;
char counter = 0;
char dataBuf[9] = { 0 };

UF_uArm uarm; // initialize the uArm library

void setup() {
  Serial.begin(9600); // start serial port at 9600 bps
  while (!Serial) {
    ;
  } // wait for serial port to connect. Needed for Leonardo only
  Serial.println("Hello uArm world!");
  uarm.init(); // initialize the uArm position
  currTime = millis();
}

void printCurrentState() {
  //int rot = uarm.readAngle(SERVO_ROT);
  Serial.print("Base Rotation:");
  Serial.println(rotationCurr);
  Serial.print("Strect:");
  Serial.println(stretchCurr);
  Serial.print("Height:");
  Serial.println(heightCurr);
  Serial.print("Hand:");
  Serial.println(handRotCurr);
  //SG-> Ignore the below :)
  /*
   Serial.write(13);
   angle = uarm.readAngle(SERVO_ROT);
   Serial.print(angle,DEC);
   Serial.write(32);
   angle = uarm.readAngle(SERVO_L);
   Serial.print(angle,DEC);
   Serial.write(32);
   angle = uarm.readAngle(SERVO_R);
   Serial.print(angle,DEC);
   Serial.write(32);
   angle = uarm.readAngle(SERVO_HAND_ROT);
   Serial.print(angle,DEC);
   Serial.write(32);
   angle = uarm.readAngle(SERVO_HAND);
   Serial.print(angle,DEC);
   Serial.write(13);
   */
}

void loop() {
  uarm.calibration(); // if corrected, you could remove it, no harm though
  if (Serial.available()) {
    byte rxBuf = Serial.read();
    if (stateMachine == FrameTypeDection) {
      if (rxBuf == 0xFF) {
        stateMachine = BinaryFrameSecondByte;
        Serial.println("Binary frame detected");
      } else if (rxBuf == 't') {
        stateMachine = TextFrameType;
        Serial.println("Text frame detected");
      } else {
        stateMachine = FrameTypeDection;
      }
    } else if (stateMachine == BinaryFrameSecondByte) {
      stateMachine = rxBuf == 0xAA ? BinaryUpdatePacket : FrameTypeDection;
    } else if (stateMachine == BinaryUpdatePacket) {
      dataBuf[counter++] = rxBuf;
      if (counter > 8) { // receive 9 byte data
        stateMachine = FrameTypeDection;
        counter = 0;
        *((char *) (&rotationTemp)) = dataBuf[1]; // recevive 1byte
        *((char *) (&rotationTemp) + 1) = dataBuf[0];
        *((char *) (&stretchTemp)) = dataBuf[3];
        *((char *) (&stretchTemp) + 1) = dataBuf[2];
        *((char *) (&heightTemp)) = dataBuf[5];
        *((char *) (&heightTemp) + 1) = dataBuf[4];
        *((char *) (&handRotTemp)) = dataBuf[7];
        *((char *) (&handRotTemp) + 1) = dataBuf[6];

        /* pump action, Valve Stop. */
        if (dataBuf[8] & CATCH) {
          uarm.gripperCatch();
        }
        /* pump stop, Valve action. 
         Note: The air relief valve can not work for a long time,
         should be less than ten minutes. */
        if (dataBuf[8] & RELEASE) {
          uarm.gripperRelease();
        }

        //printCurrentState();
      }
    } else if (stateMachine == TextFrameType) {
      textFrameType = rxBuf;
      switch(textFrameType) {
        case TextFramePrintState:
          printCurrentState();
          stateMachine = FrameTypeDection;
          break;
        default:
          stateMachine = TextFrameData;
          break;
      }
    } else if (stateMachine == TextFrameData) {
      stateMachine = FrameTypeDection;

      char amount = 0;
      switch((char)rxBuf) {
        case 'w':
          amount += 10;
          break;
        case 's':
          amount -= 10;
          break;
      }

      switch(textFrameType) {
        case TextFrameRotate: {
          rotationTemp = rotationCurr += amount;
        } break;
        case TextFrameGrabber: {
          handRotTemp = handRotCurr += amount;
        } break;
        case TextFrameHeight: {
          heightTemp = heightCurr += amount;
        } break;
        case TextFrameStretch: {
          stretchTemp = stretchCurr += amount;
        } break;
        default: {
          Serial.println("Invalid text frame type");
        } break;
      }
    }
  }

//SG-> This code checks for enough time to have passed to update the position of the arm
  if (millis() - currTime > SLEW_RATE) {
    stretchCurr = stretchCurr < stretchTemp ? ++stretchCurr : stretchCurr;
    stretchCurr = stretchCurr > stretchTemp ? --stretchCurr : stretchCurr;

    rotationCurr = rotationCurr < rotationTemp ? ++rotationCurr : rotationCurr;
    rotationCurr = rotationCurr > rotationTemp ? --rotationCurr : rotationCurr;

    heightCurr = heightCurr < heightTemp ? ++heightCurr : heightCurr;
    heightCurr = heightCurr > heightTemp ? --heightCurr : heightCurr;

    handRotCurr = handRotCurr < handRotTemp ? ++handRotCurr : handRotCurr;
    handRotCurr = handRotCurr > handRotTemp ? --handRotCurr : handRotCurr;

    //SG-> Set the new position
    uarm.setPosition(stretchCurr, heightCurr, rotationCurr, handRotCurr);

    //SG-> Get the latest time for next wait period
    currTime = millis();
  }

  /* delay release valve, this function must be in the main loop */
  uarm.gripperDetach();

}


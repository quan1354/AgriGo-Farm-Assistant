#include <Wire.h>
#include <Servo.h>
#include "HUSKYLENS.h"

HUSKYLENS huskylens;
//HUSKYLENS green line >> SDA (A4); blue line >> SCL(A5)

void printResult(HUSKYLENSResult result);

Servo panServo;   // Pan servo
Servo tiltServo;  // Tilt servo

// TiltAxis is Y-Axis
// 95 is 0
// 110 is 1
// 125 is 2

// PanAxis is X-Axis
// 118 is 0
// 95 is 1
// 70 is 2


int PanAxis[10] = { 118, 95, 70, 70, 95, 118, 118, 95, 70 };
int TiltAxis[10] = { 125, 125, 125, 110, 110, 110, 95, 95, 95 };
char *Position[10] = { "A", "B", "C", "D", "E", "F", "G", "H", "I" };
// TODO: Change words accordingly to the card board
char *Picture[10] = { "","Book", "Calculator", "Cat", "Leaf", "Wave", "People", "Heart", "Zebra", "House" };
String pos[10];
int Pic[3];
int StoredVal[10];

int currentPan = 90;  // Start centered
int currentTilt = 90;
int sweep;
int targetPan, targetTilt;

unsigned long myTime, PreTime;

String inputString = "";
bool stringComplete = false;

void setup() {
  panServo.attach(9);    // Pan servo on pin 9
  tiltServo.attach(10);  // Tilt servo on pin 10
  panServo.write(currentPan);
  tiltServo.write(currentTilt);

  Serial.begin(9600);
  Wire.begin();

  while (!huskylens.begin(Wire)) {
    Serial.println(F("Begin failed!"));
    Serial.println(F("1.Please recheck the \"Protocol Type\" in HUSKYLENS"));
    Serial.println(F("2.Please recheck the connection."));
    delay(100);
  }

  huskylens.writeAlgorithm(ALGORITHM_OBJECT_CLASSIFICATION);
  Serial.println("Enter your code as: pos1, pos2, pos3 (e.g. A,B,C)>>>");
}

void loop() {
  if (stringComplete) {
    // Parse the input string
    int firstComma = inputString.indexOf(',');
    int secondComma = inputString.indexOf(',', firstComma + 1);

    if (firstComma > 0 && secondComma > firstComma) {
      pos[0] = inputString.substring(0, firstComma);
      pos[1] = inputString.substring(firstComma + 1, secondComma);
      pos[2] = inputString.substring(secondComma + 1);

      Serial.println("Position coordinates:");
      
      for (int j = 0; j < 3; j++) {
        for (int i = 0; i < 9; i++) {  // Changed to 9 since you have 9 positions
          if (pos[j] == Position[i]) {
            // Calculate grid coordinates
            int x = i % 3;  // Column (0,1,2)
            int y = i / 3;  // Row (0,1,2)
            
            Serial.print("The letter ");
            Serial.print(pos[j]);
            Serial.print(" is at position (");
            Serial.print(x);
            Serial.print(",");
            Serial.print(y);
            Serial.println(")");
            
            Pic[j] = i;
            break;  // Added break to exit inner loop once found
          }
        }
      }

      SCANseq();
      Serial.print("Your final OUTPUT is: ");
      Serial.print(Picture[StoredVal[Pic[0]]]);
      Serial.print(", ");
      Serial.print(Picture[StoredVal[Pic[1]]]);
      Serial.print(", ");
      Serial.println(Picture[StoredVal[Pic[2]]]);
    } else {
      Serial.println("Invalid input!");
    }

    inputString = "";
    stringComplete = false;
  }
}

void SCANseq() {
  Serial.println("Start time");
  PreTime = millis();

  for (sweep = 0; sweep < 9; sweep++) {

    MOVE(PanAxis[sweep], TiltAxis[sweep]);
    delay(1500);

    if (!huskylens.request()) {
    } else if (!huskylens.isLearned()) {
    } else if (!huskylens.available()) {
    } else {
      while (huskylens.available()) {
        HUSKYLENSResult result = huskylens.read();
        printResult(result);
      }
    }
    delay(100);
  }

  myTime = millis() - PreTime;
  
  // Convert milliseconds to seconds
  float myTimeInSeconds = myTime / 1000.0;
  
  Serial.print("Completed in: ");
  Serial.print(myTimeInSeconds);
  Serial.println(" seconds");
  Serial.println("");
}


void printResult(HUSKYLENSResult result) {
  if (result.command == COMMAND_RETURN_BLOCK) {
    //Serial.println(String()+F("Block:xCenter=")+result.xCenter+F(",yCenter=")+result.yCenter+F(",width=")+result.width+F(",height=")+result.height+F(",ID=")+result.ID);
    Serial.println(String() + F(" ,ID=") + Picture[result.ID]);
    StoredVal[sweep] = result.ID;

  } else if (result.command == COMMAND_RETURN_ARROW) {
    //Serial.println(String()+F("Arrow:xOrigin=")+result.xOrigin+F(",yOrigin=")+result.yOrigin+F(",xTarget=")+result.xTarget+F(",yTarget=")+result.yTarget+F(",ID=")+result.ID);
  } else {
    Serial.println("Object unknown!");
  }
}


void MOVE(int PanAngle, int TiltAngle) {
  targetPan = PanAngle;
  targetTilt = TiltAngle;
  if (targetPan >= 0 && targetPan <= 180 && targetTilt >= 0 && targetTilt <= 180) {
    Serial.print("P: ");
    Serial.print(targetPan);
    Serial.print(" | T: ");
    Serial.print(targetTilt);

    // Smooth pan movement
    while (currentPan != targetPan || currentTilt != targetTilt) {
      if (currentPan < targetPan) currentPan++;
      else if (currentPan > targetPan) currentPan--;

      if (currentTilt < targetTilt) currentTilt++;
      else if (currentTilt > targetTilt) currentTilt--;

      panServo.write(currentPan);
      tiltServo.write(currentTilt);

      //delay(10);  // Adjust speed here
    }
  }
}


void serialEvent() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();

    if (inChar == '\n' || inChar == '\r') {
      stringComplete = true;
    } else {
      inputString += inChar;
    }
  }
}
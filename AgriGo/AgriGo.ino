#include "HUSKYLENS.h"
#include "Wire.h"
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <DHT.h>
#include <Adafruit_NeoPixel.h>
#include "CytronMotorDriver.h"

// DHT Sensor setup
#define DHT_PIN 7
#define DHT_TYPE DHT22

// NeoPixel setup
#define NEOPIXEL_PIN 2
#define NUM_LEDS 8

HUSKYLENS huskylens;
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Configure the motor driver.
CytronMD motor1(PWM_PWM, 3, 9);    // PWM 1A = Pin 3, PWM 1B = Pin 9.
CytronMD motor2(PWM_PWM, 10, 11);  // PWM 2A = Pin 10, PWM 2B = Pin 11.

// Counters for each object
int countCarrot = 0;
int countCorn = 0;
int countPineapple = 0;
int countPumpkin = 0;
int lastID = -1;

String currentAlgorithm = "ALGORITHM_TAG_RECOGNITION";
int farm[5] = { 1, 2, 3, 4, 5 };
int finishedFarm[5] = {};
int currentTargetIndex = 0;     // Index to track which farm ID we're looking for
bool isSpinning = true;         // Flag to control spinning behavior
bool isMovingForward = false;   // Flag to control forward movement
bool isDetectingFruit = false;  // New flag for fruit detection phase
unsigned long fruitDetectionStartTime = 0;
const unsigned long FRUIT_DETECTION_TIME = 5000;  // 5 seconds for fruit detection

void controlNeoPixel(float temp);
void spinInPlace();
void moveForward();
void stopMotors();
long getUltrasonicDistance();

// The setup routine runs once when you press reset.
void setup() {
  Serial.begin(115200);
  Wire.begin();

  dht.begin();
  strip.begin();
  strip.show();

  while (!huskylens.begin(Wire)) {
    Serial.println(F("Begin failed!"));
    lcd.setCursor(0, 1);
    lcd.print("Init Failed");
    delay(1000);
  }

  huskylens.writeAlgorithm(ALGORITHM_TAG_RECOGNITION);
  currentAlgorithm = "ALGORITHM_TAG_RECOGNITION";
  Serial.println(currentAlgorithm);

  pinMode(6, OUTPUT);  // Trigger pin (D6)
  pinMode(4, INPUT);   // Echo pin (D4)

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("HUSKYLENS Ready");
  delay(1000);
  lcd.clear();
  lcd.print("System Ready!");
}

// The loop routine runs over and over again forever.
void loop() {
  // State machine for different behaviors
  if (isSpinning) {
    spinInPlace();
  } else if (isMovingForward) {
    moveForward();

    // Check ultrasonic distance while moving forward
    long distance = getUltrasonicDistance();
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" cm");

    if (distance > 0 && distance < 7) {  // Stop when within 15cm
      stopMotors();
      isMovingForward = false;
      delay(1000);


      // Add current target to finished farm
      finishedFarm[currentTargetIndex] = farm[currentTargetIndex];
      Serial.print("Reached farm ID: ");
      Serial.println(finishedFarm[currentTargetIndex]);
      int currentID = finishedFarm[currentTargetIndex];

      Serial.println(currentID);


      // if (currentID == 5) {
      //   Serial.println("Farm ID 5 detected — skipping fruit detection and returning.");
      //   moveBackward();  // Start moving backward
      //   delay(2000);     // Move backward for 2 seconds (adjust as needed)
      //   stopMotors();    // Stop
      //   delay(500);
      //   // Move to next farm
      //   currentTargetIndex++;
      //   if (currentTargetIndex >= 5) {
      //     memset(finishedFarm, 0, sizeof(finishedFarm));
      //     currentTargetIndex = 0;
      //     Serial.println("All farms completed! Restarting cycle.");
      //   }
      //   // Resume spinning
      //   huskylens.writeAlgorithm(ALGORITHM_TAG_RECOGNITION);
      //   currentAlgorithm = "ALGORITHM_TAG_RECOGNITION";
      //   isSpinning = true;
      //   Serial.println("Resuming spin for next target.");
      // } else {
      //   // --- Normal fruit detection process for other IDs ---
      //   isDetectingFruit = true;
      //   huskylens.writeAlgorithm(ALGORITHM_OBJECT_CLASSIFICATION);
      //   currentAlgorithm = "ALGORITHM_OBJECT_CLASSIFICATION";
      //   fruitDetectionStartTime = millis();
      //   Serial.println("Switched to OBJECT_CLASSIFICATION for fruit detection");
      // }
      // Switch to object classification to detect fruit

      // TODO: I want skip fruit counting when ID is 5 at the stage, direct continue to find Tag ID but I can't add 1 more level of if statement here, it can't work, help me
      isDetectingFruit = true;
      huskylens.writeAlgorithm(ALGORITHM_OBJECT_CLASSIFICATION);
      currentAlgorithm = "ALGORITHM_OBJECT_CLASSIFICATION";
      fruitDetectionStartTime = millis();
      Serial.println("Switched to OBJECT_CLASSIFICATION for fruit detection");
    }
  } else if (isDetectingFruit) {
    // Stop motors during fruit detection
    stopMotors();

    // Check if fruit detection time is over
    if (millis() - fruitDetectionStartTime >= FRUIT_DETECTION_TIME) {
      // Time's up, move to next farm
      isDetectingFruit = false;

      // Move to next target
      currentTargetIndex++;

      // Check if we've completed all farms
      if (currentTargetIndex >= 5) {
        // Reset for new cycle
        memset(finishedFarm, 0, sizeof(finishedFarm));
        currentTargetIndex = 0;
        countCarrot = 0;
        countCorn = 0;
        countPineapple = 0;
        countPumpkin = 0;

        Serial.println("All farms completed! Restarting cycle.");
      }

      moveBackward();
      delay(1000);
      stopMotors();
      delay(500);


      // Switch back to tag recognition for next farm
      huskylens.writeAlgorithm(ALGORITHM_TAG_RECOGNITION);
      currentAlgorithm = "ALGORITHM_TAG_RECOGNITION";

      // Resume spinning to find next target
      isSpinning = true;
      Serial.println("Switched back to TAG_RECOGNITION, resuming spin");
    }
  }

  // --- HUSKYLENS Object Detection ---
  if (huskylens.request()) {
    if (huskylens.available()) {
      HUSKYLENSResult result = huskylens.read();

      if (result.command == COMMAND_RETURN_BLOCK && result.ID != lastID) {
        int currentID = finishedFarm[currentTargetIndex];
        if (isDetectingFruit && currentID != 5) {
          // Fruit detection and counting
          lastID = result.ID;

          if (result.ID == 1) countCarrot++;
          else if (result.ID == 2) countCorn++;
          else if (result.ID == 3) countPineapple++;
          else if (result.ID == 4) countPumpkin++;

          Serial.println("-------------------");
          Serial.print("Carrot: ");
          Serial.println(countCarrot);
          Serial.print("Corn: ");
          Serial.println(countCorn);
          Serial.print("Pineapple: ");
          Serial.println(countPineapple);
          Serial.print("Pumpkin: ");
          Serial.println(countPumpkin);
          Serial.println(String() + F("Arrow:xOrigin=") + result.xOrigin + F(",yOrigin=") + result.yOrigin + F(",xTarget=") + result.xTarget + F(",yTarget=") + result.yTarget + F(",ID=") + result.ID);

        } else if (currentAlgorithm == "ALGORITHM_TAG_RECOGNITION" && isSpinning) {
          int currentTargetID = farm[currentTargetIndex];

          // Check if this is the target we're looking for and it's in the correct x-range
          if (result.ID == currentTargetID) {
            Serial.println(String() + F("Target found! ID=") + result.ID + F(" at x=") + result.xOrigin);

            // Stop spinning and start moving forward
            isSpinning = false;
            isMovingForward = true;
            stopMotors();  // Ensure clean transition
            delay(2000);
          }

          Serial.println(String() + F("Arrow:xOrigin=") + result.xOrigin + F(",yOrigin=") + result.yOrigin + F(",xTarget=") + result.xTarget + F(",yTarget=") + result.yTarget + F(",ID=") + result.ID);
        }
      }
    } else {
      lastID = -1;
    }
  }

  // --- Read DHT Sensor ---
  float temperature = dht.readTemperature();
  // --- LCD Display: Show all fruit counts and temperature ---
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("C:");
  lcd.print(countCarrot);
  lcd.print(" Co:");
  lcd.print(countCorn);
  lcd.print(" ");
  // Weather indicator based on temperature
  if (temperature < 20) {
    lcd.print("C");
  } else if (temperature >= 20 && temperature <= 30) {
    lcd.print("N");
  } else {
    lcd.print("H");
  }
  lcd.setCursor(0, 1);
  lcd.print("P:");
  lcd.print(countPineapple);
  lcd.print(" Pu:");
  lcd.print(countPumpkin);
  lcd.print(" ");
  lcd.print(temperature, 1);
  lcd.print((char)223);  // degree symbol
  // Show current state on LCD second line if needed
  lcd.setCursor(12, 0);
  if (isSpinning) lcd.print("SPIN");
  else if (isMovingForward) lcd.print("MOVE");
  else if (isDetectingFruit) lcd.print("FRUT");
  delay(100);  // Main loop delay
}

// Spin in place at speed 80
void spinInPlace() {
  motor1.setSpeed(-160);
  motor2.setSpeed(-160);  
}

// Move forward at speed 80
void moveForward() {
  motor1.setSpeed(-130);
  motor2.setSpeed(130);
  Serial.println("Is Moving Forward");
}

void moveBackward() {
  motor1.setSpeed(130);
  motor2.setSpeed(-130);  
}

// Stop both motors
void stopMotors() {
  motor1.setSpeed(0);
  motor2.setSpeed(0);
}

// Get ultrasonic distance measurement
long getUltrasonicDistance() {
  digitalWrite(6, LOW);
  delayMicroseconds(2);
  digitalWrite(6, HIGH);
  delayMicroseconds(10);
  digitalWrite(6, LOW);

  long duration = pulseIn(4, HIGH);
  long cm = duration / 29 / 2;

  // Return -1 if measurement fails
  if (cm <= 0 || cm > 400) {
    return -1;
  }

  return cm;
}

// --- NeoPixel Function ---
void controlNeoPixel(float temp) {
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }

  uint32_t color;
  int ledsToLight;

  if (temp > 0 && temp < 20) {
    color = strip.Color(0, 0, 255);  // Blue
    ledsToLight = 2;
  } else if (temp >= 20 && temp <= 30) {
    color = strip.Color(0, 255, 0);  // Green
    ledsToLight = 4;
  } else if (temp > 30 && temp <= 60) {
    color = strip.Color(255, 255, 0);  // Yellow
    ledsToLight = 6;
  } else if (temp > 60) {
    color = strip.Color(255, 0, 0);  // Red
    ledsToLight = 8;
  } else {
    ledsToLight = 0;
  }

  for (int i = 0; i < ledsToLight; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}
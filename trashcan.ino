#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <LiquidCrystal.h>

// ----- LCD Setup -----
LiquidCrystal lcd(12, 11, 10, 9, 8, 7); // rs, en, d4, d5, d6, d7

// ----- Button Pins -----
const int buttonLeftPin = 6;  // YES (Active LOW)
const int buttonRightPin = 5; // NO (Active LOW)

// ----- Ultrasonic + Servo Pins -----
const int TRIG_PIN = 4;
const int ECHO_PIN = 3;

// ----- Buzzer -----
const int buzzerPin = 2; // Active Buzzer on pin 2

// ----- PCA9685 Setup -----
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
const uint8_t SERVO_CHANNEL = 0; // Using channel 0
const int SERVO_MIN = 150;  // Calibrate this (usually ~150)
const int SERVO_MAX = 600;  // Calibrate this (usually ~600)

// ----- Behavior Settings -----
const int NEAR_CM = 25;
const int HYSTERESIS_CM = 6;
const int OPEN_ANGLE = 100;
const int CLOSED_ANGLE = 10;
const int SAMPLE_MS = 120;
const long PULSE_TIMEOUT = 30000;

bool lidIsOpen = false;
unsigned long lastSampleMs = 0;

// ----- LCD Timing -----
unsigned long previousMillis = 0;
const long interval = 3000;
bool showFirstScreen = true;

// ================== SETUP ==================
void setup() {
  Serial.begin(9600);

  // LCD & Buttons
  lcd.begin(16, 2);
  pinMode(buttonLeftPin, INPUT_PULLUP);
  pinMode(buttonRightPin, INPUT_PULLUP);

  // Ultrasonic
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  // Buzzer
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW); // Ensure buzzer is off at startup

  // PCA9685 Servo Driver
  pwm.begin();
  pwm.setPWMFreq(50); // Standard 50 Hz for servos
  delay(10);
  goTo(CLOSED_ANGLE);

  // Initial LCD Message
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("RECYCLE BIN");
}

// ================== MAIN LOOP ==================
void loop() {
  unsigned long now = millis();

  // ---------- SENSOR/ACTUATOR SECTION ----------
  if (now - lastSampleMs >= SAMPLE_MS) {
    lastSampleMs = now;

    int d1 = readDistanceCm();
    int d2 = readDistanceCm();
    int d3 = readDistanceCm();
    int dist = median3(d1, d2, d3);

    Serial.print("Distance: ");
    if (dist > 0) {
      Serial.print(dist); Serial.println(" cm");
    } else {
      Serial.println("(no echo)");
    }

    if (!lidIsOpen && dist <= NEAR_CM && dist > 0) {
      goTo(OPEN_ANGLE);
      lidIsOpen = true;
      Serial.println("Action: OPEN");
      previousMillis = millis(); // reset LCD timer
      displayFirstScreen();      // start LCD interaction
    } 
    else if (lidIsOpen && dist > (NEAR_CM + HYSTERESIS_CM)) {
      goTo(CLOSED_ANGLE);
      lidIsOpen = false;
      Serial.println("Action: CLOSE");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("RECYCLE BIN");
    }
  }

  // ---------- LCD + BUTTON SECTION ----------
  if (lidIsOpen) {
    // Check buttons (active LOW)
    if (digitalRead(buttonLeftPin) == LOW) {
      handleInput(true);  // YES
    } 
    else if (digitalRead(buttonRightPin) == LOW) {
      handleInput(false); // NO
    }

    // Rotate LCD screens every few seconds
    if (millis() - previousMillis >= interval) {
      previousMillis = millis();

      if (showFirstScreen) {
        displaySecondScreen();
      } else {
        displayFirstScreen();
      }
      showFirstScreen = !showFirstScreen;
    }
  }
}

// ================== LCD FUNCTIONS ==================
void displayFirstScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Is this item");
  lcd.setCursor(0, 1);
  lcd.print("Recycleable?");
}

void displaySecondScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Left for YES");
  lcd.setCursor(0, 1);
  lcd.print("Right for NO");
}

// ================== HANDLE BUTTON INPUT ==================
void handleInput(bool isYes) {
  lcd.clear();

  if (isYes) {
    lcd.setCursor(0, 0);
    lcd.print("You said - YES");
    lcd.setCursor(0, 1);
    lcd.print("GOOD JOB!");
  } else {
    lcd.setCursor(0, 0);
    lcd.print("BUZZER OF");
    lcd.setCursor(0, 1);
    lcd.print("SHAME");

    // 3 x 2-second beeps with short pauses
    for (int i = 0; i < 3; i++) {
      digitalWrite(buzzerPin, HIGH);
      delay(2000);
      digitalWrite(buzzerPin, LOW);
      delay(500); // pause between beeps
    }
  }

  delay(1000); // Brief pause before closing

  // Close the lid after response
  goTo(CLOSED_ANGLE);
  lidIsOpen = false;

  // Reset LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready");

  // Reset timing state
  previousMillis = millis();
  showFirstScreen = true;
}

// ================== SENSOR & SERVO HELPERS ==================
int readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(5);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, PULSE_TIMEOUT);
  if (duration == 0) return 0;

  long cm = (duration * 34L) / 2000L;
  if (cm < 1 || cm > 500) return 0;
  return (int)cm;
}

int median3(int a, int b, int c) {
  if (a > b) { int t = a; a = b; b = t; }
  if (b > c) { int t = b; b = c; c = t; }
  if (a > b) { int t = a; a = b; b = t; }
  return b;
}

void goTo(int angle) {
  angle = constrain(angle, 0, 180);
  int pulse = map(angle, 0, 180, SERVO_MIN, SERVO_MAX);
  pwm.setPWM(SERVO_CHANNEL, 0, pulse);
}

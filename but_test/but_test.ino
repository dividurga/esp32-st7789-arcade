// FireBeetle ESP32 V4.0 button test
// Buttons are wired from GPIO pin to GND
// Uses internal pullups, so pressed = LOW

const int buttonPins[] = {26, 22, 27, 13, 4};
const char* buttonNames[] = {"A", "RIGHT", "UP", "DOWN", "LEFT"};

const int numButtons = 5;

bool lastState[numButtons];
unsigned long lastDebounceTime[numButtons];

const unsigned long debounceDelay = 50;

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("Button test starting...");

  for (int i = 0; i < numButtons; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    lastState[i] = digitalRead(buttonPins[i]);
    lastDebounceTime[i] = 0;
  }
}

void loop() {
  for (int i = 0; i < numButtons; i++) {
    bool currentState = digitalRead(buttonPins[i]);

    // State changed
    if (currentState != lastState[i]) {
      lastDebounceTime[i] = millis();
      lastState[i] = currentState;
    }

    // After debounce delay, check if button is pressed
    if ((millis() - lastDebounceTime[i]) > debounceDelay) {
      if (currentState == LOW) {
        Serial.print(buttonNames[i]);
        Serial.print(" pressed on GPIO ");
        Serial.println(buttonPins[i]);

        // Wait until button is released so it prints once per press
        while (digitalRead(buttonPins[i]) == LOW) {
          delay(10);
        }

        delay(50); // small release debounce
      }
    }
  }
}
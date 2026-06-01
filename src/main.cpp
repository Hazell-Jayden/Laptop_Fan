#include <Arduino.h>

// ---------------- PIN MAP ----------------
#define POT_PIN        4   // potentiometer input
#define BUTTON_PIN     5   // on/off toggle button

// fan 2 is daisy-chained through fan 1, so these pins aren't used directly
// #define FAN2_PWM_PIN   6
// #define FAN2_TACH_PIN  7
// #define FAN2_PWR_PIN   21

#define FAN1_PWM_PIN   10  // fan 1 speed control
#define FAN1_TACH_PIN  9   // fan 1 tach (unused for now)
#define FAN1_PWR_PIN   8   // fan 1 power relay

// ---------------- PWM CONFIG ----------------
#define PWM_FREQ       25000  // 25kHz keeps the fans quiet
#define PWM_RES        8      // 8-bit = 0–255

#define MIN_DUTY       26    // ~10%
#define MAX_DUTY       255   // 100%


bool systemOn = true;          // tracks whether the fans are running
bool lastButtonState = HIGH;   // used to detect button press edges

static unsigned long lastPrint = 0;  // throttles serial output

bool systemSwitchedState = true;     // flags when on/off state changes so we only act once

int startUpTimeDuration = 500;       // how long the startup ramp runs (ms)

unsigned long lastButtonChange = 0;
const unsigned long debounceMs = 100;  // ignores button noise under 100ms

int minPWMPercentage = 17;  // minimum speed to keep fans spinning
int maxPWMPercentage = 95;  // cap to avoid running fans flat-out

int lastPotValue;            // previous pot reading, used to detect big jumps
int currentPotValue;         // latest pot reading
int potValueDifference = 40; // minimum change to register as intentional movement

const int potOffThreshold = 300;  // pot below this → turn fans off
const int potOnThreshold  = 380;  // pot above this → turn fans on

uint8_t targetPercent = 0;   // where we want fan speed to be
uint8_t currentPercent = 0;  // where fan speed actually is (ramps toward target)

unsigned long lastRampUpdate = 0;
const unsigned long rampIntervalMs = 150;  // update every 50 ms
const uint8_t rampStep = 1;               // change by 0.5% each update

// ---------------- PWM SETUP ----------------
void setupPWM() {
    ledcSetup(0, PWM_FREQ, PWM_RES);
    ledcAttachPin(FAN1_PWM_PIN, 0);

    // ledcSetup(1, PWM_FREQ, PWM_RES);
    // ledcAttachPin(FAN2_PWM_PIN, 1);
}

// ---------------- SET PWM ----------------
// writes a speed percentage to fan 1 (fan 2 follows via daisy-chain)
void applyPWM(uint8_t pwmPercentage) {
    if (pwmPercentage == 0) {
        ledcWrite(0, 0);
        // ledcWrite(1, 0);
        return;
    }

    pwmPercentage = constrain(pwmPercentage, minPWMPercentage, maxPWMPercentage);
    uint8_t rawPWM = map(pwmPercentage, 0, 100, 0, 255);

    ledcWrite(0, rawPWM);
    // ledcWrite(1, rawPWM);
}

// ---------------- SET FAN POWER ----------------
// cuts or restores power via fan 1's relay (fan 2 is daisy-chained)
void setFanPower(bool on) {
    digitalWrite(FAN1_PWR_PIN, on);
    // digitalWrite(FAN2_PWR_PIN, on);
}

// averages 8 pot readings to smooth out noise
int readPot() {
    long total = 0;
    for (int i = 0; i < 8; i++) {
        total += analogRead(POT_PIN);
    }
    return total / 8;
}

// checks the button and pot, toggles system on/off as needed
void readButton() {
    bool state = digitalRead(BUTTON_PIN);
    currentPotValue = readPot();

    if (state != lastButtonState && millis() - lastButtonChange > debounceMs) {
        lastButtonChange = millis();

        if (lastButtonState == LOW && state == HIGH) {
            Serial.println("---- BUTTON PRESSED ----");
            lastPotValue = readPot();
            systemOn = !systemOn;
            systemSwitchedState = true;
        }

        lastButtonState = state;
    }

    // turning the pot up also switches the system back on
    if (!systemOn && currentPotValue >= potOnThreshold) {
        systemOn = true;
        systemSwitchedState = true;
    }
}

// slower ramp interval at low speeds, faster at high speeds
unsigned long getRampInterval(uint8_t currentPercent) {
    return map(currentPercent, 0, 100, 40, 500);
}

// reads the pot and smoothly steps fan speed toward the target
void setFanSpeed() {
    if (millis() - lastRampUpdate < getRampInterval(currentPercent)) return;
    lastRampUpdate = millis();

    int potRaw = readPot();

    // pot dialled down far enough → shut off
    if (potRaw <= potOffThreshold) {
        systemOn = false;
        systemSwitchedState = true;
        return;
    }

    targetPercent = map(potRaw, 0, 4095, 0, 100);

    if (currentPercent < targetPercent) {
        currentPercent += 1;
        if (currentPercent > targetPercent) currentPercent = targetPercent;
    } else if (currentPercent > targetPercent) {
        currentPercent = targetPercent;
    }

    if (millis() - lastPrint > 500) {
        lastPrint = millis();
        Serial.print("Pot Value: ");
        Serial.print(potRaw);
        Serial.print(" || Target Speed: ");
        Serial.print(targetPercent);
        Serial.print("%");
        Serial.print(" || Current Speed: ");
        Serial.print(currentPercent);
        Serial.println("%");
    }

    applyPWM(currentPercent);
}

// spins fans up to minimum speed for a short burst on startup
void handleStartup() {
    applyPWM(minPWMPercentage);

    unsigned long startUpTime = millis();

    while (millis() - startUpTime <= startUpTimeDuration) {
        readButton();

        if (systemOn == false) {
            return;
        }
    }
}

// main state machine — handles on/off transitions and normal running
void stateMachine() {

    readButton();

    if (systemOn) {

        if (systemSwitchedState == true) {
            Serial.println("---- SYSTEM TURNED ON ----");
            systemSwitchedState = false;
            setFanPower(true);
            delay(100);
            handleStartup();

            if (systemOn == false) {
                return;
            }
        }

        setFanSpeed();

    } else {

        if (systemSwitchedState == true) {
            Serial.println("---- SYSTEM TURNED OFF ----");
            systemSwitchedState = false;
            setFanPower(false);
            delay(100);
            applyPWM(0);
            lastPotValue = readPot();
        }

    }
}

// ---------------- SETUP ----------------
void setup() {
    Serial.begin(115200);

    pinMode(BUTTON_PIN, INPUT);
    pinMode(POT_PIN, INPUT);

    pinMode(FAN1_PWR_PIN, OUTPUT);
    // pinMode(FAN2_PWR_PIN, OUTPUT);

    pinMode(FAN1_TACH_PIN, INPUT_PULLUP);
    // pinMode(FAN2_TACH_PIN, INPUT_PULLUP);

    setupPWM();

    setFanPower(false);
    applyPWM(0);
    delay(500);

    lastPotValue = analogRead(POT_PIN);

    Serial.println("Finished Setup");
}

void loop() {
    stateMachine();
}

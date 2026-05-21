#include <Arduino.h>

// ---------------- PIN MAP ----------------
#define POT_PIN        4
#define BUTTON_PIN     5

#define FAN2_PWM_PIN   6
#define FAN2_TACH_PIN  7
#define FAN2_PWR_PIN   21

#define FAN1_PWM_PIN   10
#define FAN1_TACH_PIN  9
#define FAN1_PWR_PIN   8

// ---------------- PWM CONFIG ----------------
#define PWM_FREQ       25000
#define PWM_RES        8

#define MIN_DUTY       26    // ~10%
#define MAX_DUTY       255   // 100%


bool systemOn = true;
bool lastButtonState = HIGH;

static unsigned long lastPrint = 0;

bool systemSwitchedState = true;

int startUpTimeDuration = 500;

unsigned long lastButtonChange = 0;
const unsigned long debounceMs = 100;

int minPWMPercentage = 17;
int maxPWMPercentage = 95;

int lastPotValue;
int currentPotValue;
int potValueDifference = 40;
const int potOffThreshold = 300;
const int potOnThreshold  = 380;

uint8_t targetPercent = 0;
uint8_t currentPercent = 0;

unsigned long lastRampUpdate = 0;
const unsigned long rampIntervalMs = 150;  // update every 50 ms
const uint8_t rampStep = 1;               // change by 0.5% each update

// ---------------- PWM SETUP ----------------
void setupPWM() {
    ledcSetup(0, PWM_FREQ, PWM_RES);
    ledcAttachPin(FAN1_PWM_PIN, 0);

    ledcSetup(1, PWM_FREQ, PWM_RES);
    ledcAttachPin(FAN2_PWM_PIN, 1);
}

// ---------------- SET PWM ----------------
void applyPWM(uint8_t pwmPercentage) {
    if (pwmPercentage == 0) {
        ledcWrite(0, 0);
        ledcWrite(1, 0);
        return;
    }

    pwmPercentage = constrain(pwmPercentage, minPWMPercentage, maxPWMPercentage);
    uint8_t rawPWM = map(pwmPercentage, 0, 100, 0, 255);

    ledcWrite(0, rawPWM);
    ledcWrite(1, rawPWM);
}
// ---------------- SET FAN POWER ----------------
void setFanPower(bool on) {
    digitalWrite(FAN1_PWR_PIN, on);
    digitalWrite(FAN2_PWR_PIN, on);
}

int readPot() {
    long total = 0;
    for (int i = 0; i < 8; i++) {
        total += analogRead(POT_PIN);
    }
    return total / 8;
}

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

    if (!systemOn && currentPotValue >= potOnThreshold) {
        systemOn = true;
        systemSwitchedState = true;
    }
}

unsigned long getRampInterval(uint8_t currentPercent) {
    return map(currentPercent, 0, 100, 40, 500);
}

void setFanSpeed() {
    if (millis() - lastRampUpdate < getRampInterval(currentPercent)) return;
    lastRampUpdate = millis();

    int potRaw = readPot();
    
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
    pinMode(FAN2_PWR_PIN, OUTPUT);

    pinMode(FAN1_TACH_PIN, INPUT_PULLUP);
    pinMode(FAN2_TACH_PIN, INPUT_PULLUP);

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
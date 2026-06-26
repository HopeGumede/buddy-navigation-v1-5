#include <LiquidCrystal.h>

// ========================================================================
// HARDWARE PIN DEFINITIONS
// ========================================================================

// Bilateral Ultrasonic Sensors (HC-SR04)
const int TRIGGER_L = 9;
const int ECHO_L    = 8;
const int TRIGGER_R = 11;
const int ECHO_R    = 10;

// Dynamic Boundary Potentiometers ("Virtual Walls")
const int POT_WALL_L = A0;
const int POT_WALL_R = A1;

// DC Motor Actuator Pins (Assumes H-Bridge Driver like L293D/L298N)
// Left Motor Control Lines
const int MOTOR_L_FND = A2; // Left Motor Forward
const int MOTOR_L_REV = A3; // Left Motor Reverse

// Right Motor Control Lines
const int MOTOR_R_FND = A4; // Right Motor Forward
const int MOTOR_R_REV = A5; // Right Motor Reverse

// LCD Telemetry Layout Configuration (RS, E, D4, D5, D6, D7)
LiquidCrystal lcd(7, 6, 5, 4, 3, 2);

// ========================================================================
// SYSTEM STATE MACHINE DEFINITIONS
// ========================================================================
enum NavigationState {
    FORWARD,
    STEER_LEFT,
    STEER_RIGHT,
    REVERSE
};

// Global Tracking Metrics
NavigationState currentState = FORWARD;
unsigned long distanceLeft = 0;
unsigned long distanceRight = 0;
unsigned long thresholdLeft = 0;
unsigned long thresholdRight = 0;

// Function Prototypes
void evaluateNavigationLogic();
void driveMotors();
void updateLCDTelemetry();

// ========================================================================
// INITIALIZATION
// ========================================================================
void setup() {
    // Initialize Bilateral Sensor Control Lines
    pinMode(TRIGGER_L, OUTPUT);
    pinMode(ECHO_L, INPUT);
    pinMode(TRIGGER_R, OUTPUT);
    pinMode(ECHO_R, INPUT);
    
    // Initialize DC Motor Control Pins as Outputs
    pinMode(MOTOR_L_FND, OUTPUT);
    pinMode(MOTOR_L_REV, OUTPUT);
    pinMode(MOTOR_R_FND, OUTPUT);
    pinMode(MOTOR_R_REV, OUTPUT);
    
    // Ensure triggers and motors start in a clean, safe LOW/OFF state
    digitalWrite(TRIGGER_L, LOW);
    digitalWrite(TRIGGER_R, LOW);
    digitalWrite(MOTOR_L_FND, LOW);
    digitalWrite(MOTOR_L_REV, LOW);
    digitalWrite(MOTOR_R_FND, LOW);
    digitalWrite(MOTOR_R_REV, LOW);
    
    // Initialize Visual Telemetry Window
    lcd.begin(16, 2);
    lcd.clear();
    lcd.print("BUDDY RUNNING V1.5");
    lcd.setCursor(0, 1);
    lcd.print("ACTUATORS: ENGRD");
    
    delay(1500); // Stabilization delay for Proteus simulation engines
    lcd.clear();
}

// ========================================================================
// MAIN CONTROL LOOP
// ========================================================================
void loop() {
    // --------------------------------------------------------------------
    // STEP 1: DYNAMIC THRESHOLD SAMPLING (THE VIRTUAL WALLS)
    // --------------------------------------------------------------------
    int rawPotL = analogRead(POT_WALL_L);
    int rawPotR = analogRead(POT_WALL_R);
    
    // Map raw 10-bit ADC variables to a practical obstacle distance (20cm - 100cm)
    thresholdLeft  = map(rawPotL, 0, 1023, 20, 100);
    thresholdRight = map(rawPotR, 0, 1023, 20, 100);

    // --------------------------------------------------------------------
    // STEP 2: INTERLEAVED SENSOR SAMPLING (PREVENTS ACOUSTIC CROSS-TALK)
    // --------------------------------------------------------------------
    // Trigger and measure LEFT Sensor
    digitalWrite(TRIGGER_L, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIGGER_L, LOW);
    unsigned long durationL = pulseIn(ECHO_L, HIGH, 30000); // 30ms timeout limit
    distanceLeft = (durationL == 0) ? 999 : durationL / 58;

    delay(20); // Acoustic dissipation buffer (allows room echo to decay)

    // Trigger and measure RIGHT Sensor
    digitalWrite(TRIGGER_R, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIGGER_R, LOW);
    unsigned long durationR = pulseIn(ECHO_R, HIGH, 30000); 
    distanceRight = (durationR == 0) ? 999 : durationR / 58;

    // --------------------------------------------------------------------
    // STEP 3: STATE MACHINE LOGIC EVALUATION
    // --------------------------------------------------------------------
    evaluateNavigationLogic();

    // --------------------------------------------------------------------
    // STEP 4: PHYSICAL ACTUATOR EXECUTION
    // --------------------------------------------------------------------
    driveMotors();

    // --------------------------------------------------------------------
    // STEP 5: VISUAL TELEMETRY RENDER
    // --------------------------------------------------------------------
    updateLCDTelemetry();
    
    delay(50); // Loop execution pacing for simulation stability
}

// ========================================================================
// SUBSYSTEM LOGIC ROUTINES
// ========================================================================
void evaluateNavigationLogic() {
    bool leftBlocked  = (distanceLeft < thresholdLeft);
    bool rightBlocked = (distanceRight < thresholdRight);

    if (!leftBlocked && !rightBlocked) {
        currentState = FORWARD;
    } 
    else if (leftBlocked && !rightBlocked) {
        currentState = STEER_RIGHT; // Escape right if left wall is breached
    } 
    else if (!leftBlocked && rightBlocked) {
        currentState = STEER_LEFT;  // Escape left if right wall is breached
    } 
    else if (leftBlocked && rightBlocked) {
        currentState = REVERSE;     // Backup if completely trapped
    }
}

void driveMotors() {
    switch(currentState) {
        case FORWARD:
            // Both motors spin forward
            digitalWrite(MOTOR_L_FND, HIGH);
            digitalWrite(MOTOR_L_REV, LOW);
            digitalWrite(MOTOR_R_FND, HIGH);
            digitalWrite(MOTOR_R_REV, LOW);
            break;
            
        case STEER_LEFT:
            // Pivot Left: Right wheel spins forward, Left wheel goes reverse (or stops)
            digitalWrite(MOTOR_L_FND, LOW);
            digitalWrite(MOTOR_L_REV, HIGH);
            digitalWrite(MOTOR_R_FND, HIGH);
            digitalWrite(MOTOR_R_REV, LOW);
            break;
            
        case STEER_RIGHT:
            // Pivot Right: Left wheel spins forward, Right wheel goes reverse (or stops)
            digitalWrite(MOTOR_L_FND, HIGH);
            digitalWrite(MOTOR_L_REV, LOW);
            digitalWrite(MOTOR_R_FND, LOW);
            digitalWrite(MOTOR_R_REV, HIGH);
            break;
            
        case REVERSE:
            // Both motors spin backward
            digitalWrite(MOTOR_L_FND, LOW);
            digitalWrite(MOTOR_L_REV, HIGH);
            digitalWrite(MOTOR_R_FND, LOW);
            digitalWrite(MOTOR_R_REV, HIGH);
            break;
    }
}

void updateLCDTelemetry() {
    // Line 1: Live Distance / Active Calibration Threshold Target
    lcd.setCursor(0, 0);
    lcd.print("L:");
    lcd.print(distanceLeft);
    lcd.print("/");
    lcd.print(thresholdLeft);
    lcd.print("  "); // Blank padding prevents trailing text artifacts
    
    lcd.setCursor(8, 0);
    lcd.print("R:");
    lcd.print(distanceRight);
    lcd.print("/");
    lcd.print(thresholdRight);
    lcd.print("  ");

    // Line 2: Active Navigation State Telemetry
    lcd.setCursor(0, 1);
    lcd.print("STATE: ");
    switch(currentState) {
        case FORWARD:     lcd.print("FORWARD    "); break;
        case STEER_LEFT:  lcd.print("STEER LEFT "); break;
        case STEER_RIGHT: lcd.print("STEER RIGHT"); break;
        case REVERSE:     lcd.print("REVERSE    "); break;
    }
}
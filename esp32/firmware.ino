#include <Arduino.h>

// ===== PIN DEFINITIONS =====
#define REAR_PWM   25
#define REAR_DIR   26
#define STEER_IN1  12
#define STEER_IN2  13
#define STEER_EN   21
#define STEER_POT  34

#define TRIG_F     32
#define ECHO_F     33
#define TRIG_L     4
#define ECHO_L     5
#define TRIG_R     18
#define ECHO_R     14

// ===== PARAMETERS =====
#define OBSTACLE_DIST_CM  30
#define SIDE_CLEAR_CM     20
#define AVOID_TURN_TIME   800
#define DRIVE_SPEED       200

const int POT_MIN = 200;
const int POT_MAX = 3800;
const int STRAIGHT_ANGLE = 90;
const int TURN_ANGLE = 45;

// PID Parameters
float Kp = 2.5, Ki = 0.05, Kd = 0.5;
int integral = 0, last_error = 0;

// ===== GLOBALS =====
bool avoiding = false;
unsigned long avoidStartTime = 0;
int globalTargetAngle = STRAIGHT_ANGLE;

enum RobotMode {
    MODE_IDLE,
    MODE_MANUAL,
    MODE_AUTO_PATROL
};

RobotMode robotMode = MODE_IDLE;
bool isDrivingForward = false;

// Ultrasonic state
struct USState {
    unsigned long trigTime;
    bool measuring;
    float lastDistance;
    bool echoHigh;
    unsigned long echoStart;
};

USState usF = {0, false, 400.0, false, 0};
USState usL = {0, false, 400.0, false, 0};
USState usR = {0, false, 400.0, false, 0};

int frontObstacleCount = 0;
const int DEBOUNCE_THRESHOLD = 3;

// ===== SETUP =====
void setup() {
    Serial.begin(115200);
    
    pinMode(REAR_DIR, OUTPUT);
    pinMode(STEER_IN1, OUTPUT);
    pinMode(STEER_IN2, OUTPUT);
    
    ledcSetup(0, 5000, 8);
    ledcAttachPin(REAR_PWM, 0);
    ledcSetup(1, 5000, 8);
    ledcAttachPin(STEER_EN, 1);
    
    pinMode(TRIG_F, OUTPUT);
    pinMode(ECHO_F, INPUT);
    pinMode(TRIG_L, OUTPUT);
    pinMode(ECHO_L, INPUT);
    pinMode(TRIG_R, OUTPUT);
    pinMode(ECHO_R, INPUT);
    
    digitalWrite(TRIG_F, LOW);
    digitalWrite(TRIG_L, LOW);
    digitalWrite(TRIG_R, LOW);
    analogReadResolution(12);
    
    stopMotors();
    Serial.println("ESP32 Ready");
}

// ===== LOOP =====
void parseSerialCommand(const String& commandLine);

void loop() {
    // Ultrasonic reads (non-blocking)
    static unsigned long lastFrontRead = 0;
    static unsigned long lastSideRead = 0;
    static String serialBuffer = "";
    
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (serialBuffer.length() > 0) {
                parseSerialCommand(serialBuffer);
                serialBuffer = "";
            }
        } else {
            if (serialBuffer.length() < 64) {
                serialBuffer += c;
            }
        }
    }
    
    if (millis() - lastFrontRead >= 50) {
        initUS(TRIG_F, usF);
        lastFrontRead = millis();
    }
    if (millis() - lastSideRead >= 100) {
        initUS(TRIG_L, usL);
        initUS(TRIG_R, usR);
        lastSideRead = millis();
    }
    
    updateUS(ECHO_F, usF);
    updateUS(ECHO_L, usL);
    updateUS(ECHO_R, usR);
    
    checkObstacles();

    if (robotMode == MODE_AUTO_PATROL && !avoiding) {
        driveForward();
        isDrivingForward = true;
    }
    
    updateSteeringPID();
    delay(1);
}

// ===== ULTRASONIC =====
void initUS(int trig, USState& s) {
    if (!s.measuring) {
        digitalWrite(trig, LOW);
        delayMicroseconds(2);
        digitalWrite(trig, HIGH);
        delayMicroseconds(10);
        digitalWrite(trig, LOW);
        s.trigTime = micros();
        s.measuring = true;
        s.echoHigh = false;
        s.echoStart = 0;
    }
}

bool updateUS(int echo, USState& s) {
    if (!s.measuring) return false;
    
    int val = digitalRead(echo);
    unsigned long now = micros();
    
    if (val && !s.echoHigh) {
        s.echoHigh = true;
        s.echoStart = now;
        return false;
    }
    if (!val && s.echoHigh) {
        unsigned long duration = now - s.echoStart;
        s.lastDistance = (duration / 2.0) / 29.1;
        s.measuring = false;
        s.echoHigh = false;
        return true;
    }
    if (now - s.trigTime > 23000) {
        s.measuring = false;
        s.echoHigh = false;
        s.lastDistance = 400.0;
        return true;
    }
    return false;
}

// ===== OBSTACLE AVOIDANCE =====
void checkObstacles() {
    if (avoiding) {
        if (millis() - avoidStartTime >= AVOID_TURN_TIME) {
            globalTargetAngle = STRAIGHT_ANGLE;
            avoiding = false;
            if (robotMode == MODE_AUTO_PATROL) {
                driveForward();
                isDrivingForward = true;
            } else {
                stopMotors();
                isDrivingForward = false;
            }
            Serial.println("AVOIDANCE_COMPLETE");
        }
        return;
    }
    
    if (usF.lastDistance < OBSTACLE_DIST_CM) {
        frontObstacleCount++;
    } else {
        frontObstacleCount = 0;
    }
    
    if (frontObstacleCount >= DEBOUNCE_THRESHOLD) {
        stopMotors();
        frontObstacleCount = 0;
        Serial.print("OBSTACLE_DETECTED,");
        Serial.print(usF.lastDistance, 1);
        Serial.println("cm");
        
        bool turnLeft = (usL.lastDistance > usR.lastDistance && usL.lastDistance > SIDE_CLEAR_CM);
        if (turnLeft) {
            globalTargetAngle = STRAIGHT_ANGLE - TURN_ANGLE;
            Serial.println("TURNING_LEFT");
        } else {
            globalTargetAngle = STRAIGHT_ANGLE + TURN_ANGLE;
            Serial.println("TURNING_RIGHT");
        }
        
        driveForward();
        avoiding = true;
        avoidStartTime = millis();
    }
}

// ===== STEERING PID =====
void updateSteeringPID() {
    int current = potToAngle(analogRead(STEER_POT));
    int error = globalTargetAngle - current;
    
    if (abs(error) <= 2) {
        setSteering(0, 0);
        integral = 0;
        return;
    }
    
    integral = constrain(integral + error, -1000, 1000);
    int derivative = error - last_error;
    last_error = error;
    
    float output = Kp * error + Ki * integral + Kd * derivative;
    int speed = constrain(abs(output), 0, 255);
    
    if (error > 0) setSteering(speed, 0);
    else setSteering(0, speed);
}

int potToAngle(int val) {
    val = constrain(val, POT_MIN, POT_MAX);
    return map(val, POT_MIN, POT_MAX, 0, 180);
}

void setSteering(int r, int l) {
    if (r > 0) {
        digitalWrite(STEER_IN1, HIGH);
        digitalWrite(STEER_IN2, LOW);
        ledcWrite(1, r);
    } else if (l > 0) {
        digitalWrite(STEER_IN1, LOW);
        digitalWrite(STEER_IN2, HIGH);
        ledcWrite(1, l);
    } else {
        digitalWrite(STEER_IN1, LOW);
        digitalWrite(STEER_IN2, LOW);
        ledcWrite(1, 0);
    }
}

// ===== MOTOR =====
void driveForward() {
    digitalWrite(REAR_DIR, HIGH);
    ledcWrite(0, DRIVE_SPEED);
}

void driveBackward() {
    digitalWrite(REAR_DIR, LOW);
    ledcWrite(0, DRIVE_SPEED);
}

void stopMotors() {
    ledcWrite(0, 0);
}

// ===== COMMANDS =====
void setMode(RobotMode mode) {
    if (robotMode == mode) return;
    robotMode = mode;
    switch (mode) {
        case MODE_IDLE:
            stopMotors();
            isDrivingForward = false;
            Serial.println("MODE_IDLE");
            break;
        case MODE_MANUAL:
            isDrivingForward = false;
            Serial.println("MODE_MANUAL");
            break;
        case MODE_AUTO_PATROL:
            Serial.println("MODE_AUTO_PATROL");
            driveForward();
            isDrivingForward = true;
            break;
    }
}

void executeCommand(char cmd) {
    if (avoiding && cmd != 'S' && cmd != 'P' && cmd != 'M') return;
    
    switch (cmd) {
        case 'F':
            setMode(MODE_MANUAL);
            globalTargetAngle = STRAIGHT_ANGLE;
            driveForward();
            isDrivingForward = true;
            Serial.println("CMD_FORWARD");
            break;
        case 'B':
            setMode(MODE_MANUAL);
            globalTargetAngle = STRAIGHT_ANGLE;
            driveBackward();
            isDrivingForward = false;
            Serial.println("CMD_BACKWARD");
            break;
        case 'L':
            setMode(MODE_MANUAL);
            globalTargetAngle = STRAIGHT_ANGLE - TURN_ANGLE;
            driveForward();
            isDrivingForward = true;
            Serial.println("CMD_LEFT_TURN");
            break;
        case 'R':
            setMode(MODE_MANUAL);
            globalTargetAngle = STRAIGHT_ANGLE + TURN_ANGLE;
            driveForward();
            isDrivingForward = true;
            Serial.println("CMD_RIGHT_TURN");
            break;
        case 'S':
            setMode(MODE_IDLE);
            globalTargetAngle = STRAIGHT_ANGLE;
            stopMotors();
            Serial.println("CMD_STOP");
            break;
        case 'P':
            setMode(MODE_AUTO_PATROL);
            break;
        case 'M':
            setMode(MODE_MANUAL);
            break;
        case 'C':
            if (Serial.available()) {
                int angle = Serial.parseInt();
                globalTargetAngle = constrain(angle, 0, 180);
                Serial.print("ANGLE_SET,");
                Serial.println(angle);
            }
            break;
    }
}

void parseSerialCommand(const String& commandLine) {
    String cmd = commandLine;
    cmd.trim();
    if (cmd.length() == 0) return;

    if (cmd.equalsIgnoreCase("PATROL") || cmd.equalsIgnoreCase("AUTO") || cmd.equalsIgnoreCase("P")) {
        setMode(MODE_AUTO_PATROL);
        return;
    }
    if (cmd.equalsIgnoreCase("STOP") || cmd.equalsIgnoreCase("PAUSE") || cmd.equalsIgnoreCase("S")) {
        executeCommand('S');
        return;
    }
    if (cmd.equalsIgnoreCase("RESUME") || cmd.equalsIgnoreCase("START") || cmd.equalsIgnoreCase("F") || cmd.equalsIgnoreCase("FORWARD")) {
        executeCommand('F');
        return;
    }
    if (cmd.equalsIgnoreCase("MANUAL") || cmd.equalsIgnoreCase("M")) {
        setMode(MODE_MANUAL);
        return;
    }
    if (cmd.equalsIgnoreCase("LEFT") || cmd.equalsIgnoreCase("L")) {
        executeCommand('L');
        return;
    }
    if (cmd.equalsIgnoreCase("RIGHT") || cmd.equalsIgnoreCase("R")) {
        executeCommand('R');
        return;
    }
    if (cmd.equalsIgnoreCase("BACKWARD") || cmd.equalsIgnoreCase("B")) {
        executeCommand('B');
        return;
    }
    if (cmd.startsWith("ANGLE ")) {
        int angle = cmd.substring(6).toInt();
        angle = constrain(angle, 0, 180);
        globalTargetAngle = angle;
        Serial.print("ANGLE_SET,");
        Serial.println(angle);
        return;
    }
    if (cmd.length() == 1) {
        executeCommand(cmd.charAt(0));
        return;
    }
    Serial.print("UNKNOWN_COMMAND,");
    Serial.println(cmd);
}
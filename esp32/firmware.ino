#include <Arduino.h>

// ===== PIN DEFINITIONS =====
#define LEFT_RPWM  25
#define LEFT_LPWM  26
#define RIGHT_RPWM 27
#define RIGHT_LPWM 13

#define TRIG_F     32
#define ECHO_F     33
#define TRIG_L     4
#define ECHO_L     5
#define TRIG_R     18
#define ECHO_R     14

// ===== PARAMETERS =====
#define OBSTACLE_DIST_CM   30
#define SIDE_CLEAR_CM      20
#define AVOID_TURN_TIME    800
#define DRIVE_SPEED        70
#define TURN_SPEED         65
#define REMOTE_SPEED_LIMIT 55
#define US_INTERVAL_MS     30      
#define US_TIMEOUT_US      25000   
#define DEBOUNCE_THRESHOLD 2       
#define COMMAND_TIMEOUT_MS 2000    // Stop if Pi stops sending commands for 2s

// ===== GLOBALS =====
bool avoiding = false;
unsigned long avoidStartTime = 0;
unsigned long lastCommandTime = 0; // Timer for Pi connection check

enum RobotMode {
    MODE_IDLE,
    MODE_MANUAL,
    MODE_REMOTE,
    MODE_AUTO_PATROL
};

RobotMode robotMode = MODE_IDLE;
bool isDrivingForward = false;
bool lastTurnLeft = false;

// Ultrasonic state
struct USState {
    uint8_t trig;
    uint8_t echo;
    volatile uint32_t echoStart;
    volatile uint32_t echoDuration;
    volatile bool echoDone;
    bool measuring;
    uint32_t trigTime;
    float lastDistance;
};

USState usF = {TRIG_F, ECHO_F, 0, 0, false, false, 0, 400.0f};
USState usL = {TRIG_L, ECHO_L, 0, 0, false, false, 0, 400.0f};
USState usR = {TRIG_R, ECHO_R, 0, 0, false, false, 0, 400.0f};

USState* const sensors[3] = {&usF, &usL, &usR};
const uint8_t sensorSequence[4] = {0, 1, 0, 2};   

int frontObstacleCount = 0;

// ===== FUNCTION PROTOTYPES =====
void parseSerialCommand(const String& commandLine);
void IRAM_ATTR echoISR(void* arg);
void triggerUS(USState& s);
bool updateUS(USState& s);
bool chooseTurnDirection();
void checkObstacles();
void setSideSpeed(int rpwmPin, int lpwmPin, int speed);
void driveSides(int left, int right);
void driveForward();
void driveBackward();
void performTurnLeft();
void turnRight();
void stopMotors();
void setMode(RobotMode mode);
void executeCommand(char cmd);

bool parsePwmValue(const String& text, int& value) {
    if (text.length() == 0 || text.length() > 4) return false;
    unsigned int firstDigit = text.charAt(0) == '-' ? 1 : 0;
    if (firstDigit == text.length()) return false;
    for (unsigned int i = firstDigit; i < text.length(); i++) {
        if (text.charAt(i) < '0' || text.charAt(i) > '9') return false;
    }

    long parsed = text.toInt();
    if (parsed < -255 || parsed > 255) return false;
    value = static_cast<int>(parsed);
    return true;
}

// ===== SETUP =====
void setup() {
    Serial.begin(115200);

    // ESP32 Arduino Core v3.x LEDC API
    ledcAttach(LEFT_RPWM, 5000, 8);
    ledcAttach(LEFT_LPWM, 5000, 8);
    ledcAttach(RIGHT_RPWM, 5000, 8);
    ledcAttach(RIGHT_LPWM, 5000, 8);

    for (int i = 0; i < 3; i++) {
        pinMode(sensors[i]->trig, OUTPUT);
        pinMode(sensors[i]->echo, INPUT);
        digitalWrite(sensors[i]->trig, LOW);
        attachInterruptArg(digitalPinToInterrupt(sensors[i]->echo), echoISR, sensors[i], CHANGE);
    }

    stopMotors();
    Serial.println("ESP32 Ready - 4WD skid steer with Timeout");
}

// ===== LOOP =====
void loop() {
    static unsigned long lastTrigger = 0;
    static uint8_t seqIdx = 0;
    static String serialBuffer = "";

    // --- 1. Serial input (from Pi or Laptop) ---
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (serialBuffer.length() > 0) {
                parseSerialCommand(serialBuffer);
                serialBuffer = "";
                lastCommandTime = millis(); // Reset timeout timer on any command
            }
        } else if (serialBuffer.length() < 64) {
            serialBuffer += c;
        }
    }

    // --- 2. Safety Timeout (Pi Watchdog) ---
    // If Pi goes silent for 2s while driving, stop the robot.
    if (robotMode != MODE_IDLE && (millis() - lastCommandTime > COMMAND_TIMEOUT_MS)) {
        if (!avoiding) {
            stopMotors();
            robotMode = MODE_IDLE;
            Serial.println("ERR: PI_TIMEOUT (Stopping motors)");
        }
    }

    // --- 3. Fire ONE ultrasonic sensor at a time ---
    if (millis() - lastTrigger >= US_INTERVAL_MS) {
        triggerUS(*sensors[sensorSequence[seqIdx]]);
        seqIdx = (seqIdx + 1) % 4;
        lastTrigger = millis();
    }

    // --- 4. Collect ultrasonic results ---
    bool frontNew = updateUS(usF);
    updateUS(usL);
    updateUS(usR);

    if (frontNew) {
        if (usF.lastDistance < OBSTACLE_DIST_CM) frontObstacleCount++;
        else frontObstacleCount = 0;
    }

    // --- 5. Check Obstacles ---
    checkObstacles();

    // --- 6. Continuous Patrol Logic ---
    if (robotMode == MODE_AUTO_PATROL && !avoiding && !isDrivingForward) {
        driveForward();
        isDrivingForward = true;
    }

    delay(1);
}

// ===== ULTRASONIC =====
void IRAM_ATTR echoISR(void* arg) {
    USState* s = static_cast<USState*>(arg);
    if (digitalRead(s->echo)) {
        s->echoStart = micros();
    } else {
        s->echoDuration = micros() - s->echoStart;
        s->echoDone = true;
    }
}

void triggerUS(USState& s) {
    if (s.measuring) return;
    s.echoDone = false;
    digitalWrite(s.trig, LOW);
    delayMicroseconds(2);
    digitalWrite(s.trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(s.trig, LOW);
    s.trigTime = micros();
    s.measuring = true;
}

bool updateUS(USState& s) {
    if (!s.measuring) return false;

    if (s.echoDone) {
        float d = s.echoDuration / 58.2f;   
        s.lastDistance = (d > 400.0f) ? 400.0f : d;
        s.measuring = false;
        return true;
    }
    if (micros() - s.trigTime > US_TIMEOUT_US) {
        s.lastDistance = 400.0f;            
        s.measuring = false;
        return true;
    }
    return false;
}

// ===== OBSTACLE AVOIDANCE =====
bool chooseTurnDirection() {
    float leftClear = usL.lastDistance;
    float rightClear = usR.lastDistance;

    if (leftClear > SIDE_CLEAR_CM && rightClear > SIDE_CLEAR_CM) {
        if (leftClear > rightClear) return true;
        if (rightClear > leftClear) return false;
        return !lastTurnLeft;
    }

    if (leftClear > SIDE_CLEAR_CM) return true;
    if (rightClear > SIDE_CLEAR_CM) return false;

    return !lastTurnLeft;
}

void checkObstacles() {
    if (avoiding) {
        if (millis() - avoidStartTime >= AVOID_TURN_TIME) {
            avoiding = false;
            frontObstacleCount = 0;   
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

    // Only react to obstacles while actually driving forward
    if (robotMode == MODE_IDLE || !isDrivingForward) {
        frontObstacleCount = 0;
        return;
    }

    if (frontObstacleCount >= DEBOUNCE_THRESHOLD) {
        stopMotors();
        frontObstacleCount = 0;
        isDrivingForward = false;
        Serial.print("OBSTACLE_DETECTED,");
        Serial.print(usF.lastDistance, 1);
        Serial.println("cm");

        if (chooseTurnDirection()) {
            performTurnLeft();
            lastTurnLeft = true;
            Serial.println("TURNING_LEFT");
        } else {
            turnRight();
            lastTurnLeft = false;
            Serial.println("TURNING_RIGHT");
        }

        avoiding = true;
        avoidStartTime = millis();
    }
}

// ===== 4WD MOTOR DRIVE =====
void setSideSpeed(int rpwmPin, int lpwmPin, int speed) {
    speed = constrain(speed, -255, 255);
    if (speed >= 0) {
        ledcWrite(rpwmPin, speed);
        ledcWrite(lpwmPin, 0);
    } else {
        ledcWrite(rpwmPin, 0);
        ledcWrite(lpwmPin, -speed);
    }
}

void driveSides(int left, int right) {
    setSideSpeed(LEFT_RPWM, LEFT_LPWM, left);
    setSideSpeed(RIGHT_RPWM, RIGHT_LPWM, right);
}

void driveForward()   { driveSides(DRIVE_SPEED, DRIVE_SPEED); }
void driveBackward()  { driveSides(-DRIVE_SPEED, -DRIVE_SPEED); }
void performTurnLeft(){ driveSides(-TURN_SPEED, TURN_SPEED); }
void turnRight()      { driveSides(TURN_SPEED, -TURN_SPEED); }
void stopMotors()     { driveSides(0, 0); }

// ===== COMMANDS =====
void setMode(RobotMode mode) {
    if (robotMode == mode) return;
    robotMode = mode;
    switch (mode) {
        case MODE_IDLE:
            avoiding = false;
            stopMotors();
            isDrivingForward = false;
            Serial.println("MODE_IDLE");
            break;
        case MODE_MANUAL:
            isDrivingForward = false;
            if (!avoiding) stopMotors();
            Serial.println("MODE_MANUAL");
            break;
        case MODE_REMOTE:
            isDrivingForward = false;
            if (!avoiding) stopMotors();
            Serial.println("MODE_REMOTE");
            break;
        case MODE_AUTO_PATROL:
            Serial.println("MODE_AUTO_PATROL");
            if (!avoiding) {              
                driveForward();
                isDrivingForward = true;
            }
            break;
    }
}

void executeCommand(char cmd) {
    if (avoiding && cmd != 'S' && cmd != 'P' && cmd != 'M') {
        Serial.println("BUSY_AVOIDING");
        return;
    }

    switch (cmd) {
        case 'F':
            setMode(MODE_MANUAL);
            driveForward();
            isDrivingForward = true;
            Serial.println("CMD_FORWARD");
            break;
        case 'B':
            setMode(MODE_MANUAL);
            driveBackward();
            isDrivingForward = false;
            Serial.println("CMD_BACKWARD");
            break;
        case 'L':
            setMode(MODE_MANUAL);
            performTurnLeft();
            isDrivingForward = false;
            Serial.println("CMD_LEFT_TURN");
            break;
        case 'R':
            setMode(MODE_MANUAL);
            turnRight();
            isDrivingForward = false;
            Serial.println("CMD_RIGHT_TURN");
            break;
        case 'S':
            avoiding = false;             
            setMode(MODE_IDLE);
            stopMotors();
            isDrivingForward = false;
            Serial.println("CMD_STOP");
            break;
        case 'P':
            setMode(MODE_AUTO_PATROL);
            break;
        case 'M':
            setMode(MODE_MANUAL);
            break;
        default:
            Serial.print("UNKNOWN_COMMAND,");
            Serial.println(cmd);
            break;
    }
}

void parseSerialCommand(const String& commandLine) {
    String cmd = commandLine;
    cmd.trim();
    if (cmd.length() == 0) return;

    if (cmd.startsWith("G,")) {
        const int comma = cmd.indexOf(',', 2);
        int left = 0;
        int right = 0;
        if (comma < 0 ||
            cmd.indexOf(',', comma + 1) >= 0 ||
            !parsePwmValue(cmd.substring(2, comma), left) ||
            !parsePwmValue(cmd.substring(comma + 1), right)) {
            Serial.println("ERR: INVALID_SPEED_COMMAND");
            setMode(MODE_IDLE);
            return;
        }
        if (avoiding) {
            Serial.println("BUSY_AVOIDING");
            return;
        }

        left = constrain(left, -REMOTE_SPEED_LIMIT, REMOTE_SPEED_LIMIT);
        right = constrain(right, -REMOTE_SPEED_LIMIT, REMOTE_SPEED_LIMIT);
        setMode(MODE_REMOTE);
        driveSides(left, right);
        isDrivingForward = left + right > 0;
        Serial.println("CMD_SPEED");
        return;
    }

    if (cmd.equalsIgnoreCase("PATROL") || cmd.equalsIgnoreCase("AUTO") || cmd.equalsIgnoreCase("P")) {
        executeCommand('P');
        return;
    }
    if (cmd.equalsIgnoreCase("STOP") || cmd.equalsIgnoreCase("PAUSE") || cmd.equalsIgnoreCase("S")) {
        executeCommand('S');
        return;
    }
    if (cmd.equalsIgnoreCase("RESUME") || cmd.equalsIgnoreCase("START") ||
        cmd.equalsIgnoreCase("F") || cmd.equalsIgnoreCase("FORWARD")) {
        executeCommand('F');
        return;
    }
    if (cmd.equalsIgnoreCase("MANUAL") || cmd.equalsIgnoreCase("M")) {
        executeCommand('M');
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
    if (cmd.length() == 1) {
        executeCommand(toupper(cmd.charAt(0)));
        return;
    }
    Serial.print("UNKNOWN_COMMAND,");
    Serial.println(cmd);
}
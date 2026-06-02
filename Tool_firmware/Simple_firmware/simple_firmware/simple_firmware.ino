#include <PostNeoSWSerial.h>
#include "HilFrameArduino.h"

// ============================================================
// Pin mapping
// ============================================================

#define LL_SIGNAL 3        // Low level sensor
#define HL_SIGNAL 2        // High level sensor

#define RX_PIN 8           // RX from Raspberry / RS485
#define TX_PIN 7           // TX to Raspberry / RS485

#define TOOL_DOWN_PIN 4    // Move down - LOW to activate
#define TOOL_UP_PIN 5      // Move up   - LOW to activate

#define CURRENT_SIGNAL A0  // Current sensor ACS712-5A, 185 mV/A

#define ID 1               // Tool ID

// ============================================================
// Configuration
// ============================================================

// LJ12A3-4-Z/BX NPN sensors with external voltage divider:
// active sensor -> Arduino input LOW
#define SENSOR_ACTIVE_LEVEL LOW

// Relay module:
// LOW  = active
// HIGH = inactive
#define RELAY_ACTIVE_LEVEL LOW
#define RELAY_INACTIVE_LEVEL HIGH

#define MOVEMENT_TIMEOUT_MS 5000UL
#define CHECK_PERIOD_MS 100UL
#define DIRECTION_DEAD_TIME_MS 100UL

#define RS485_ACK_DELAY_MS 50UL
#define SERIAL_FRAME_TIMEOUT_MS 150UL

// Current sensor configuration
#define ADC_REF_VOLTAGE 5.0f
#define CURRENT_SENSITIVITY_V_PER_A 0.185f
#define CURRENT_LIMIT_A 1.2f

// ============================================================
// Protocol
// ============================================================
//
// Raspberry -> Arduino:
//
// float[0] = device_id
// float[1] = command
//
// Commands:
// 1 = UP
// 2 = DOWN
//
// Arduino -> Raspberry:
//
// float[0] = device_id
// float[1] = response_type
// float[2] = command
// float[3] = result_code
//
// ============================================================

enum CommandCode {
  CMD_UP = 1,
  CMD_DOWN = 2,
  CMD_STATUS = 3
};

enum ResponseType {
  RESPONSE_ACK = 1,
  RESPONSE_STATUS = 2
};

enum CommandResult {
  RESULT_OK = 0,
  RESULT_INVALID_COMMAND = 1,
  RESULT_REJECTED_AT_TOP = 2,
  RESULT_REJECTED_AT_BOTTOM = 3,
  RESULT_RELAY_CONFLICT = 4
};

// ============================================================
// Serial and HIL frame
// ============================================================

PostNeoSWSerial rs485Serial(RX_PIN, TX_PIN);

HilFrameArduino<64> rxFrame;
HilFrameArduino<64> txFrame;

// ============================================================
// Current measurement
// ============================================================

float currentZeroVoltage = 2.5f;

float readCurrentVoltage()
{
  int raw = analogRead(CURRENT_SIGNAL);
  return ((float)raw * ADC_REF_VOLTAGE) / 1023.0f;
}

float calibrateCurrentZeroVoltage()
{
  const int samples = 100;
  float acc = 0.0f;

  for (int i = 0; i < samples; i++) {
    acc += readCurrentVoltage();
    delay(5);
  }

  return acc / samples;
}

float readCurrentA()
{
  float voltage = readCurrentVoltage();
  float current = (voltage - currentZeroVoltage) / CURRENT_SENSITIVITY_V_PER_A;

  if (current < 0.0f) {
    current = -current;
  }

  return current;
}

// ============================================================
// Sensor read
// ============================================================

bool isHLActive()
{
  return digitalRead(HL_SIGNAL) == SENSOR_ACTIVE_LEVEL;
}

bool isLLActive()
{
  return digitalRead(LL_SIGNAL) == SENSOR_ACTIVE_LEVEL;
}

// ============================================================
// Safe relay control
// ============================================================

void forceBothRelaysOff()
{
  digitalWrite(TOOL_UP_PIN, RELAY_INACTIVE_LEVEL);
  digitalWrite(TOOL_DOWN_PIN, RELAY_INACTIVE_LEVEL);
}

void stopTool()
{
  forceBothRelaysOff();
}

bool areBothRelaysActive()
{
  return digitalRead(TOOL_UP_PIN) == RELAY_ACTIVE_LEVEL &&
         digitalRead(TOOL_DOWN_PIN) == RELAY_ACTIVE_LEVEL;
}

bool activateUpRelaySafely()
{
  // DOWN must be inactive before UP goes active.
  digitalWrite(TOOL_DOWN_PIN, RELAY_INACTIVE_LEVEL);
  delay(DIRECTION_DEAD_TIME_MS);

  if (digitalRead(TOOL_DOWN_PIN) != RELAY_INACTIVE_LEVEL) {
    forceBothRelaysOff();
    return false;
  }

  digitalWrite(TOOL_UP_PIN, RELAY_ACTIVE_LEVEL);

  if (areBothRelaysActive()) {
    forceBothRelaysOff();
    return false;
  }

  return true;
}

bool activateDownRelaySafely()
{
  // UP must be inactive before DOWN goes active.
  digitalWrite(TOOL_UP_PIN, RELAY_INACTIVE_LEVEL);
  delay(DIRECTION_DEAD_TIME_MS);

  if (digitalRead(TOOL_UP_PIN) != RELAY_INACTIVE_LEVEL) {
    forceBothRelaysOff();
    return false;
  }

  digitalWrite(TOOL_DOWN_PIN, RELAY_ACTIVE_LEVEL);

  if (areBothRelaysActive()) {
    forceBothRelaysOff();
    return false;
  }

  return true;
}

// ============================================================
// ACK response
// ============================================================

void sendAck(float command, CommandResult result)
{
  delay(RS485_ACK_DELAY_MS);

  txFrame.clear();

  txFrame.addFloat((float)ID);
  txFrame.addFloat((float)RESPONSE_ACK);
  txFrame.addFloat(command);
  txFrame.addFloat((float)result);

  txFrame.writeTo(rs485Serial);
}

// ============================================================
// Blocking movement functions
// ============================================================

void moveUpSimple()
{
  Serial.println("Move UP start");

  if (!activateUpRelaySafely()) {
    Serial.println("Move UP error: relay conflict");
    stopTool();
    return;
  }

  uint32_t startTime = millis();
  bool skipFirstCurrentCheck = true;

  while (true) {
    if (isHLActive()) {
      Serial.println("Move UP stop: HL active");
      break;
    }

    if (millis() - startTime >= MOVEMENT_TIMEOUT_MS) {
      Serial.println("Move UP stop: timeout");
      break;
    }

    if (skipFirstCurrentCheck) {
      skipFirstCurrentCheck = false;
    } else {
      float currentA = readCurrentA();

      if (currentA > CURRENT_LIMIT_A) {
        Serial.print("Move UP stop: overcurrent = ");
        Serial.println(currentA);
        break;
      }
    }

    delay(CHECK_PERIOD_MS);
  }

  stopTool();
  Serial.println("Move UP finished");
}

void moveDownSimple()
{
  Serial.println("Move DOWN start");

  if (!activateDownRelaySafely()) {
    Serial.println("Move DOWN error: relay conflict");
    stopTool();
    return;
  }

  uint32_t startTime = millis();
  bool skipFirstCurrentCheck = true;

  while (true) {
    if (isLLActive()) {
      Serial.println("Move DOWN stop: LL active");
      break;
    }

    if (millis() - startTime >= MOVEMENT_TIMEOUT_MS) {
      Serial.println("Move DOWN stop: timeout");
      break;
    }

    if (skipFirstCurrentCheck) {
      skipFirstCurrentCheck = false;
    } else {
      float currentA = readCurrentA();

      if (currentA > CURRENT_LIMIT_A) {
        Serial.print("Move DOWN stop: overcurrent = ");
        Serial.println(currentA);
        break;
      }
    }

    delay(CHECK_PERIOD_MS);
  }

  stopTool();
  Serial.println("Move DOWN finished");
}

void sendSensorStatus()
{
  delay(RS485_ACK_DELAY_MS);

  txFrame.clear();

  txFrame.addFloat((float)ID);
  txFrame.addFloat((float)RESPONSE_STATUS);
  txFrame.addFloat(isHLActive() ? 1.0f : 0.0f);
  txFrame.addFloat(isLLActive() ? 1.0f : 0.0f);

  txFrame.writeTo(rs485Serial);
}

// ============================================================
// Command processing
// ============================================================

void processCommandFrame()
{
  rxFrame.resetReadIndex();

  float device = 0.0f;
  float command = 0.0f;

  if (!rxFrame.getFloat(device)) {
    Serial.println("Error reading device ID");
    return;
  }

  if ((int)device != ID) {
    Serial.println("Frame ignored: wrong ID");
    return;
  }

  if (!rxFrame.getFloat(command)) {
    Serial.println("Error reading command");
    return;
  }

  Serial.print("Command = ");
  Serial.println(command);

  switch ((int)command) {
    case CMD_UP:
      if (isHLActive()) {
        Serial.println("UP rejected: HL already active");
        sendAck(command, RESULT_REJECTED_AT_TOP);
        return;
      }

      sendAck(command, RESULT_OK);

      // Small pause after ACK before activating relays.
      delay(50);

      moveUpSimple();
      break;

    case CMD_DOWN:
      if (isLLActive()) {
        Serial.println("DOWN rejected: LL already active");
        sendAck(command, RESULT_REJECTED_AT_BOTTOM);
        return;
      }

      sendAck(command, RESULT_OK);

      // Small pause after ACK before activating relays.
      delay(50);

      moveDownSimple();
      break;
    
    case CMD_STATUS:
      Serial.println("STATUS command");
      sendSensorStatus();
      break;

    default:
      Serial.println("Invalid command");
      sendAck(command, RESULT_INVALID_COMMAND);
      break;
  }
}

// ============================================================
// Setup and loop
// ============================================================

void setup()
{
  // Relay pins first, then force safe state.
  pinMode(TOOL_UP_PIN, OUTPUT);
  pinMode(TOOL_DOWN_PIN, OUTPUT);
  forceBothRelaysOff();

  // Sensors use external voltage divider, so INPUT, not INPUT_PULLUP.
  pinMode(HL_SIGNAL, INPUT);
  pinMode(LL_SIGNAL, INPUT);

  Serial.begin(9600);
  rs485Serial.begin(9600);

  Serial.println("Arduino simple tool controller ready");

  currentZeroVoltage = calibrateCurrentZeroVoltage();

  Serial.print("Current zero voltage = ");
  Serial.println(currentZeroVoltage, 3);

  Serial.print("Initial HL = ");
  Serial.println(isHLActive());

  Serial.print("Initial LL = ");
  Serial.println(isLLActive());

  Serial.println("System ready");
}

void loop()
{
  if (rxFrame.readFrom(rs485Serial, SERIAL_FRAME_TIMEOUT_MS)) {
    Serial.println("Frame received");
    processCommandFrame();
  }
}
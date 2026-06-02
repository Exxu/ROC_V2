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

#define ID 1               // Tool ID

// ============================================================
// Configuration
// ============================================================

// Sensors LJ12A3-4-Z/BX NPN with external voltage divider:
// active sensor -> Arduino input LOW
#define SENSOR_ACTIVE_LEVEL LOW

// Relay module:
// LOW  = active
// HIGH = inactive
#define RELAY_ACTIVE_LEVEL LOW
#define RELAY_INACTIVE_LEVEL HIGH

#define MOVEMENT_TIMEOUT_MS 5000UL
#define SENSOR_CHECK_PERIOD_MS 100UL
#define DIRECTION_DEAD_TIME_MS 100UL

#define RS485_ACK_DELAY_MS 50UL
#define SERIAL_FRAME_TIMEOUT_MS 150UL

// ============================================================
// Protocol
// ============================================================

enum CommandCode {
  CMD_UP = 1,
  CMD_DOWN = 2
};

enum ResponseType {
  RESPONSE_ACK = 1
};

enum CommandResult {
  RESULT_OK = 0,
  RESULT_INVALID_COMMAND = 1,
  RESULT_REJECTED_AT_TOP = 2,
  RESULT_REJECTED_AT_BOTTOM = 3
};

// ============================================================
// Serial and HIL frame
// ============================================================

PostNeoSWSerial rs485Serial(RX_PIN, TX_PIN);

HilFrameArduino<64> rxFrame;
HilFrameArduino<64> txFrame;

// ============================================================
// Basic sensor read
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
//
// Arduino -> Raspberry:
//
// float[0] = device_id
// float[1] = RESPONSE_ACK
// float[2] = command
// float[3] = result_code
//
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
    Serial.println("Relay conflict during UP");
    stopTool();
    return;
  }

  uint32_t startTime = millis();

  while (true) {
    if (isHLActive()) {
      Serial.println("Move UP stop: HL active");
      break;
    }

    if (millis() - startTime >= MOVEMENT_TIMEOUT_MS) {
      Serial.println("Move UP stop: timeout");
      break;
    }

    delay(SENSOR_CHECK_PERIOD_MS);
  }

  stopTool();
  Serial.println("Move UP finished");
}

void moveDownSimple()
{
  Serial.println("Move DOWN start");

  if (!activateDownRelaySafely()) {
    Serial.println("Relay conflict during DOWN");
    stopTool();
    return;
  }

  uint32_t startTime = millis();

  while (true) {
    if (isLLActive()) {
      Serial.println("Move DOWN stop: LL active");
      break;
    }

    if (millis() - startTime >= MOVEMENT_TIMEOUT_MS) {
      Serial.println("Move DOWN stop: timeout");
      break;
    }

    delay(SENSOR_CHECK_PERIOD_MS);
  }

  stopTool();
  Serial.println("Move DOWN finished");
}

// ============================================================
// Command processing
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
  pinMode(TOOL_UP_PIN, OUTPUT);
  pinMode(TOOL_DOWN_PIN, OUTPUT);

  forceBothRelaysOff();

  // Sensors use external voltage divider, so INPUT, not INPUT_PULLUP.
  pinMode(HL_SIGNAL, INPUT);
  pinMode(LL_SIGNAL, INPUT);

  Serial.begin(9600);
  rs485Serial.begin(9600);

  Serial.println("Arduino simple tool controller ready");

  Serial.print("Initial HL = ");
  Serial.println(isHLActive());

  Serial.print("Initial LL = ");
  Serial.println(isLLActive());
}

void loop()
{
  if (rxFrame.readFrom(rs485Serial, SERIAL_FRAME_TIMEOUT_MS)) {
    Serial.println("Frame received");
    processCommandFrame();
  }
}
#include <PostNeoSWSerial.h>
#include <avr/interrupt.h>
#include "HilFrameArduino.h"

// ============================================================
// Pin mapping
// ============================================================

#define LL_SIGNAL 3        // Low level sensor - INT1
#define HL_SIGNAL 2        // High level sensor - INT0

#define RX_PIN 8           // RX from Raspberry / RS485 module
#define TX_PIN 7           // TX to Raspberry / RS485 module

#define TOOL_DOWN_PIN 4    // Move down - LOW to activate
#define TOOL_UP_PIN 5      // Move up   - LOW to activate

#define CURRENT_SIGNAL A0  // ACS712-like current sensor, 185 mV/A
#define CURRENT_ADC_CHANNEL 0  // A0 = ADC0

#define ID 1               // Tool ID

// ============================================================
// Logic configuration
// ============================================================

// LJ12A3-4-Z/BX NPN sensors with external voltage divider:
// active sensor -> Arduino input LOW
#define SENSOR_ACTIVE_LEVEL LOW

// Relay module:
// LOW  = relay active
// HIGH = relay inactive
#define RELAY_ACTIVE_LEVEL LOW
#define RELAY_INACTIVE_LEVEL HIGH

// Fixed safety parameters for this tool
#define MOVEMENT_TIMEOUT_UP_MS    5000UL
#define MOVEMENT_TIMEOUT_DOWN_MS  5000UL

// H-bridge relay safety dead time
#define DIRECTION_DEAD_TIME_MS 100UL

// Current protection
#define CURRENT_LIMIT_A 1.2f
#define CURRENT_IGNORE_TIME_MS 50UL
#define CURRENT_SAMPLE_PERIOD_MS 10UL
#define CURRENT_FILTER_ALPHA 0.2f

// ADC conversion
#define ADC_REF_VOLTAGE 5.0f
#define CURRENT_SENSITIVITY_V_PER_A 0.185f

// Serial frame timeout
#define SERIAL_FRAME_TIMEOUT_MS 25UL

// ============================================================
// Protocol definitions
// ============================================================

enum CommandCode {
  CMD_STOP = 0,
  CMD_UP = 1,
  CMD_DOWN = 2,
  CMD_RESET_FAULT = 3,
  CMD_STATUS_REQUEST = 4
};

enum ResponseType {
  RESPONSE_ACK = 1,
  RESPONSE_STATUS = 2
};

enum CommandResult {
  RESULT_OK = 0,
  RESULT_REJECTED_FAULT = 1,
  RESULT_REJECTED_AT_TOP = 2,
  RESULT_REJECTED_AT_BOTTOM = 3,
  RESULT_INVALID_COMMAND = 4,
  RESULT_ALREADY_MOVING = 5,
  RESULT_REJECTED_SENSOR_CONFLICT = 6
};

enum ToolPublicStatus {
  STATUS_UNKNOWN = 0,
  STATUS_AT_BOTTOM = 1,
  STATUS_AT_TOP = 2,
  STATUS_MOVING_UP = 3,
  STATUS_MOVING_DOWN = 4,
  STATUS_STOPPED_BETWEEN_LIMITS = 5,
  STATUS_FAULT = 6
};

enum ToolState {
  TOOL_IDLE = 0,
  TOOL_PREPARE_UP,
  TOOL_MOVING_UP,
  TOOL_PREPARE_DOWN,
  TOOL_MOVING_DOWN,
  TOOL_FAULT
};

enum FaultCode {
  FAULT_NONE = 0,
  FAULT_TIMEOUT_UP = 1,
  FAULT_TIMEOUT_DOWN = 2,
  FAULT_OVERCURRENT_UP = 3,
  FAULT_OVERCURRENT_DOWN = 4,
  FAULT_RELAY_CONFLICT = 5,
  FAULT_SENSOR_CONFLICT = 6
};

// ============================================================
// Serial and HIL frame
// ============================================================

PostNeoSWSerial rs485Serial(RX_PIN, TX_PIN);

HilFrameArduino<64> rxFrame;
HilFrameArduino<64> txFrame;

// ============================================================
// Global state
// ============================================================

ToolState toolState = TOOL_IDLE;
FaultCode faultCode = FAULT_NONE;

uint32_t stateStartMillis = 0;
uint32_t movementStartMillis = 0;
uint32_t movementTimeoutMs = 0;

// ============================================================
// Sensor states updated by external interruptions
// ============================================================

volatile bool hlActiveISR = false;
volatile bool llActiveISR = false;

void onHLChange()
{
  hlActiveISR = digitalRead(HL_SIGNAL) == SENSOR_ACTIVE_LEVEL;
}

void onLLChange()
{
  llActiveISR = digitalRead(LL_SIGNAL) == SENSOR_ACTIVE_LEVEL;
}

bool isHLActive()
{
  noInterrupts();
  bool value = hlActiveISR;
  interrupts();

  return value;
}

bool isLLActive()
{
  noInterrupts();
  bool value = llActiveISR;
  interrupts();

  return value;
}

// ============================================================
// ADC current measurement by interruption
// ============================================================

volatile uint16_t currentAdcRawISR = 0;
volatile bool currentAdcNewSampleISR = false;

uint32_t lastCurrentSampleRequestMillis = 0;

float currentZeroVoltage = 2.5f;
float filteredCurrentA = 0.0f;

ISR(ADC_vect)
{
  currentAdcRawISR = ADC;
  currentAdcNewSampleISR = true;
}

float readCurrentVoltageBlocking()
{
  int raw = analogRead(CURRENT_SIGNAL);
  return ((float)raw * ADC_REF_VOLTAGE) / 1023.0f;
}

float calibrateCurrentZeroVoltage()
{
  const int samples = 100;
  float acc = 0.0f;

  for (int i = 0; i < samples; i++) {
    acc += readCurrentVoltageBlocking();
    delay(5);
  }

  return acc / samples;
}

void setupCurrentAdcInterrupt()
{
  // AVcc as ADC reference, ADC0/A0 as input channel.
  ADMUX = (1 << REFS0) | (CURRENT_ADC_CHANNEL & 0x07);

  // Enable ADC, enable ADC interrupt, prescaler = 128.
  // ADC clock = 16 MHz / 128 = 125 kHz.
  ADCSRA = (1 << ADEN)  |
           (1 << ADIE)  |
           (1 << ADPS2) |
           (1 << ADPS1) |
           (1 << ADPS0);

  // Free-running disabled. Conversion is requested manually.
  ADCSRB = 0;

  // Disable digital input buffer on ADC0/A0 to reduce noise.
  DIDR0 |= (1 << ADC0D);
}

void requestCurrentAdcConversion()
{
  if ((ADCSRA & (1 << ADSC)) == 0) {
    ADCSRA |= (1 << ADSC);
  }
}

void updateCurrentMeasurement()
{
  uint32_t now = millis();

  if (now - lastCurrentSampleRequestMillis >= CURRENT_SAMPLE_PERIOD_MS) {
    lastCurrentSampleRequestMillis = now;
    requestCurrentAdcConversion();
  }

  uint16_t raw = 0;
  bool hasNewSample = false;

  noInterrupts();
  if (currentAdcNewSampleISR) {
    raw = currentAdcRawISR;
    currentAdcNewSampleISR = false;
    hasNewSample = true;
  }
  interrupts();

  if (hasNewSample) {
    float voltage = ((float)raw * ADC_REF_VOLTAGE) / 1023.0f;
    float current = (voltage - currentZeroVoltage) / CURRENT_SENSITIVITY_V_PER_A;

    if (current < 0.0f) {
      current = -current;
    }

    filteredCurrentA =
      (1.0f - CURRENT_FILTER_ALPHA) * filteredCurrentA +
      CURRENT_FILTER_ALPHA * current;
  }
}

bool isOverCurrent()
{
  if (millis() - movementStartMillis < CURRENT_IGNORE_TIME_MS) {
    return false;
  }

  return filteredCurrentA > CURRENT_LIMIT_A;
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
  // DOWN must be inactive before UP is activated.
  digitalWrite(TOOL_DOWN_PIN, RELAY_INACTIVE_LEVEL);

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
  // UP must be inactive before DOWN is activated.
  digitalWrite(TOOL_UP_PIN, RELAY_INACTIVE_LEVEL);

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

void setFault(FaultCode code)
{
  forceBothRelaysOff();

  faultCode = code;
  toolState = TOOL_FAULT;

  Serial.print("FAULT = ");
  Serial.println((int)faultCode);
}

// ============================================================
// Public status and responses
// ============================================================

bool hasSensorConflict()
{
  return isHLActive() && isLLActive();
}

bool isMovingOrPreparing()
{
  return toolState == TOOL_PREPARE_UP ||
         toolState == TOOL_MOVING_UP ||
         toolState == TOOL_PREPARE_DOWN ||
         toolState == TOOL_MOVING_DOWN;
}

ToolPublicStatus getToolPublicStatus()
{
  bool hl = isHLActive();
  bool ll = isLLActive();

  if (toolState == TOOL_FAULT) {
    return STATUS_FAULT;
  }

  if (hl && ll) {
    return STATUS_UNKNOWN;
  }

  switch (toolState) {
    case TOOL_PREPARE_UP:
    case TOOL_MOVING_UP:
      return STATUS_MOVING_UP;

    case TOOL_PREPARE_DOWN:
    case TOOL_MOVING_DOWN:
      return STATUS_MOVING_DOWN;

    case TOOL_IDLE:
      if (hl) {
        return STATUS_AT_TOP;
      }

      if (ll) {
        return STATUS_AT_BOTTOM;
      }

      return STATUS_STOPPED_BETWEEN_LIMITS;

    default:
      return STATUS_UNKNOWN;
  }
}

void sendAck(float command, CommandResult result)
{
  txFrame.clear();

  txFrame.addFloat((float)ID);
  txFrame.addFloat((float)RESPONSE_ACK);
  txFrame.addFloat(command);
  txFrame.addFloat((float)result);

  txFrame.writeTo(rs485Serial);
}

void sendToolStatus()
{
  txFrame.clear();

  txFrame.addFloat((float)ID);
  txFrame.addFloat((float)RESPONSE_STATUS);
  txFrame.addFloat((float)getToolPublicStatus());
  txFrame.addFloat((float)faultCode);
  txFrame.addFloat(isHLActive() ? 1.0f : 0.0f);
  txFrame.addFloat(isLLActive() ? 1.0f : 0.0f);
  txFrame.addFloat(filteredCurrentA);

  txFrame.writeTo(rs485Serial);
}

// ============================================================
// Movement commands
// ============================================================

CommandResult startUpTool()
{
  if (toolState == TOOL_FAULT) {
    Serial.println("Cannot move UP: tool in FAULT");
    return RESULT_REJECTED_FAULT;
  }

  if (hasSensorConflict()) {
    setFault(FAULT_SENSOR_CONFLICT);
    return RESULT_REJECTED_SENSOR_CONFLICT;
  }

  if (isMovingOrPreparing()) {
    Serial.println("UP rejected: tool already moving");
    return RESULT_ALREADY_MOVING;
  }

  if (isHLActive()) {
    Serial.println("UP rejected: HL already active");
    stopTool();
    toolState = TOOL_IDLE;
    return RESULT_REJECTED_AT_TOP;
  }

  stopTool();

  movementTimeoutMs = MOVEMENT_TIMEOUT_UP_MS;
  stateStartMillis = millis();
  filteredCurrentA = 0.0f;
  toolState = TOOL_PREPARE_UP;

  Serial.println("Prepare UP");

  return RESULT_OK;
}

CommandResult startDownTool()
{
  if (toolState == TOOL_FAULT) {
    Serial.println("Cannot move DOWN: tool in FAULT");
    return RESULT_REJECTED_FAULT;
  }

  if (hasSensorConflict()) {
    setFault(FAULT_SENSOR_CONFLICT);
    return RESULT_REJECTED_SENSOR_CONFLICT;
  }

  if (isMovingOrPreparing()) {
    Serial.println("DOWN rejected: tool already moving");
    return RESULT_ALREADY_MOVING;
  }

  if (isLLActive()) {
    Serial.println("DOWN rejected: LL already active");
    stopTool();
    toolState = TOOL_IDLE;
    return RESULT_REJECTED_AT_BOTTOM;
  }

  stopTool();

  movementTimeoutMs = MOVEMENT_TIMEOUT_DOWN_MS;
  stateStartMillis = millis();
  filteredCurrentA = 0.0f;
  toolState = TOOL_PREPARE_DOWN;

  Serial.println("Prepare DOWN");

  return RESULT_OK;
}

void resetFault()
{
  stopTool();

  faultCode = FAULT_NONE;
  toolState = TOOL_IDLE;
  filteredCurrentA = 0.0f;

  Serial.println("Fault reset");
}

// ============================================================
// Tool state machine update
// ============================================================

void updateTool()
{
  uint32_t now = millis();

  if (areBothRelaysActive()) {
    setFault(FAULT_RELAY_CONFLICT);
    return;
  }

  if (hasSensorConflict()) {
    setFault(FAULT_SENSOR_CONFLICT);
    return;
  }

  switch (toolState) {
    case TOOL_IDLE:
      stopTool();
      break;

    case TOOL_PREPARE_UP:
      if (now - stateStartMillis >= DIRECTION_DEAD_TIME_MS) {
        if (!activateUpRelaySafely()) {
          setFault(FAULT_RELAY_CONFLICT);
          break;
        }

        movementStartMillis = now;
        filteredCurrentA = 0.0f;
        toolState = TOOL_MOVING_UP;

        Serial.println("Moving UP");
      }
      break;

    case TOOL_MOVING_UP:
      if (isHLActive()) {
        stopTool();
        toolState = TOOL_IDLE;
        Serial.println("UP finished: HL reached");
      }
      else if (now - movementStartMillis >= movementTimeoutMs) {
        setFault(FAULT_TIMEOUT_UP);
      }
      else if (isOverCurrent()) {
        setFault(FAULT_OVERCURRENT_UP);
      }
      break;

    case TOOL_PREPARE_DOWN:
      if (now - stateStartMillis >= DIRECTION_DEAD_TIME_MS) {
        if (!activateDownRelaySafely()) {
          setFault(FAULT_RELAY_CONFLICT);
          break;
        }

        movementStartMillis = now;
        filteredCurrentA = 0.0f;
        toolState = TOOL_MOVING_DOWN;

        Serial.println("Moving DOWN");
      }
      break;

    case TOOL_MOVING_DOWN:
      if (isLLActive()) {
        stopTool();
        toolState = TOOL_IDLE;
        Serial.println("DOWN finished: LL reached");
      }
      else if (now - movementStartMillis >= movementTimeoutMs) {
        setFault(FAULT_TIMEOUT_DOWN);
      }
      else if (isOverCurrent()) {
        setFault(FAULT_OVERCURRENT_DOWN);
      }
      break;

    case TOOL_FAULT:
      stopTool();
      break;
  }
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
// 0 = stop
// 1 = up
// 2 = down
// 3 = reset fault
// 4 = status request
//
// Arduino -> Raspberry ACK:
//
// float[0] = device_id
// float[1] = RESPONSE_ACK
// float[2] = command
// float[3] = result_code
//
// Arduino -> Raspberry STATUS:
//
// float[0] = device_id
// float[1] = RESPONSE_STATUS
// float[2] = tool_status
// float[3] = fault_code
// float[4] = HL_active
// float[5] = LL_active
// float[6] = filtered_current_A
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
    case CMD_STOP:
      Serial.println("STOP command");
      stopTool();
      toolState = TOOL_IDLE;
      sendAck(command, RESULT_OK);
      break;

    case CMD_UP: {
      Serial.println("UP command");
      CommandResult result = startUpTool();
      sendAck(command, result);
      break;
    }

    case CMD_DOWN: {
      Serial.println("DOWN command");
      CommandResult result = startDownTool();
      sendAck(command, result);
      break;
    }

    case CMD_RESET_FAULT:
      Serial.println("RESET FAULT command");
      resetFault();
      sendAck(command, RESULT_OK);
      break;

    case CMD_STATUS_REQUEST:
      Serial.println("STATUS REQUEST command");
      sendToolStatus();
      break;

    default:
      Serial.println("Unknown command");
      sendAck(command, RESULT_INVALID_COMMAND);
      break;
  }
}

// ============================================================
// Setup and loop
// ============================================================

void setup()
{
  // Relay pins first, then force safe state immediately.
  pinMode(TOOL_DOWN_PIN, OUTPUT);
  pinMode(TOOL_UP_PIN, OUTPUT);
  forceBothRelaysOff();

  // Sensors use external voltage divider, so INPUT, not INPUT_PULLUP.
  pinMode(HL_SIGNAL, INPUT);
  pinMode(LL_SIGNAL, INPUT);

  Serial.begin(9600);
  rs485Serial.begin(9600);

  Serial.println("Arduino listo");

  // Initial sensor state before enabling interrupts.
  hlActiveISR = digitalRead(HL_SIGNAL) == SENSOR_ACTIVE_LEVEL;
  llActiveISR = digitalRead(LL_SIGNAL) == SENSOR_ACTIVE_LEVEL;

  attachInterrupt(digitalPinToInterrupt(HL_SIGNAL), onHLChange, CHANGE);
  attachInterrupt(digitalPinToInterrupt(LL_SIGNAL), onLLChange, CHANGE);

  // Current sensor calibration. Motor must be stopped.
  currentZeroVoltage = calibrateCurrentZeroVoltage();

  Serial.print("Current zero voltage = ");
  Serial.println(currentZeroVoltage, 3);

  // Enable ADC interrupt-based measurement after calibration.
  setupCurrentAdcInterrupt();
  requestCurrentAdcConversion();

  Serial.print("Initial HL = ");
  Serial.println(isHLActive());

  Serial.print("Initial LL = ");
  Serial.println(isLLActive());

  Serial.println("System ready");
}

void loop()
{
  if (rxFrame.readFrom(rs485Serial, SERIAL_FRAME_TIMEOUT_MS)) {
    Serial.print("Frame received. Payload bytes: ");
    Serial.println(rxFrame.size());

    processCommandFrame();
  }

  updateCurrentMeasurement();
  updateTool();
}
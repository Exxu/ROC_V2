# Arduino Nano Tool Controller Firmware

This `README.md` documents the firmware for an electrically actuated tool controlled by an Arduino Nano. The tool communicates with a Raspberry Pi using a binary HIL frame protocol over serial/RS485. The Arduino operates as the local controller and handles the critical safety functions.

The system is designed so that the Raspberry Pi sends only high-level commands, while the Arduino locally decides whether the requested movement is safe.

---

## 1. Firmware objective

The firmware controls a tool with two movements:

- move up;
- move down.

It also supervises:

- upper limit sensor `HL`;
- lower limit sensor `LL`;
- actuator current;
- movement timeout;
- sensor conflict;
- relay H-bridge conflict.

The Arduino immediately responds to each command with an `ACK` or `NACK`. The full tool status is sent only when the master requests a `STATUS`.

---

## 2. Communication architecture

The architecture is master-slave:

```text
Raspberry Pi / Master
        |
        | HIL protocol over serial / RS485
        |
Arduino Nano / Slave
        |
        |-- Relay H-bridge
        |-- Tool actuator
        |-- HL sensor
        |-- LL sensor
        |-- Current sensor
```

Main communication rule:

```text
The Arduino never transmits spontaneously.
The Arduino only responds to a transaction initiated by the Raspberry Pi.
```

This is important because it allows multiple tools to share the same RS485 bus without collisions.

---

## 3. Hardware considered

### 3.1 Arduino

- Arduino Nano with ATmega328P.
- Software serial communication using `PostNeoSWSerial`.
- Analog input `A0` for the current sensor.
- External interrupts on `D2` and `D3` for the `HL` and `LL` sensors.

### 3.2 Actuator

The actuator consumes approximately:

```text
1 A during normal operation
```

The firmware cuts movement by overcurrent at:

```text
1.2 A
```

A protection fuse of approximately the following value is also installed:

```text
1.5 A
```

The fuse is the final electrical protection. Functional protection against mechanical jamming is performed in software using the current sensor.

### 3.3 HL and LL sensors

The system uses NPN inductive sensors of the LJ12A3-4-Z/BX type:

- `HL`: upper limit sensor.
- `LL`: lower limit sensor.

Because these sensors operate at 24 V, their output must not be connected directly to the Arduino. An external resistive voltage divider is used. During testing, the voltage measured at the Arduino input was approximately:

```text
voltage divider output to Arduino: ~4.6 V
```

Therefore, the pins are configured as regular inputs:

```cpp
pinMode(HL_SIGNAL, INPUT);
pinMode(LL_SIGNAL, INPUT);
```

`INPUT_PULLUP` is not used.

### 3.4 Current sensor

The firmware assumes an ACS712-type current sensor, or an equivalent sensor, with sensitivity:

```text
185 mV/A
```

In the code:

```cpp
#define CURRENT_SENSITIVITY_V_PER_A 0.185f
```

---

## 4. Pin assignment

| Signal | Arduino pin | Description |
|---|---:|---|
| `HL_SIGNAL` | D2 | Upper limit sensor, external interrupt INT0 |
| `LL_SIGNAL` | D3 | Lower limit sensor, external interrupt INT1 |
| `TOOL_DOWN_PIN` | D4 | Down movement activation |
| `TOOL_UP_PIN` | D5 | Up movement activation |
| `TX_PIN` | D7 | Serial TX to Raspberry Pi / RS485 module |
| `RX_PIN` | D8 | Serial RX from Raspberry Pi / RS485 module |
| `CURRENT_SIGNAL` | A0 | Current sensor analog input |

Definitions used in the firmware:

```cpp
#define LL_SIGNAL 3
#define HL_SIGNAL 2
#define RX_PIN 8
#define TX_PIN 7
#define TOOL_DOWN_PIN 4
#define TOOL_UP_PIN 5
#define CURRENT_SIGNAL A0
#define CURRENT_ADC_CHANNEL 0
#define ID 1
```

---

## 5. Sensor logic

The NPN sensors are active-low:

```text
LOW  -> sensor active
HIGH -> sensor inactive
```

In the firmware:

```cpp
#define SENSOR_ACTIVE_LEVEL LOW
```

Therefore:

```text
HL active -> tool is at the upper position
LL active -> tool is at the lower position
```

The sensor states are updated using external interrupts:

```cpp
attachInterrupt(digitalPinToInterrupt(HL_SIGNAL), onHLChange, CHANGE);
attachInterrupt(digitalPinToInterrupt(LL_SIGNAL), onLLChange, CHANGE);
```

The interrupt service routines only update global state variables:

```cpp
void onHLChange()
{
  hlActiveISR = digitalRead(HL_SIGNAL) == SENSOR_ACTIVE_LEVEL;
}

void onLLChange()
{
  llActiveISR = digitalRead(LL_SIGNAL) == SENSOR_ACTIVE_LEVEL;
}
```

No movement logic is executed inside the interrupt routines.

---

## 6. Safe control of the relay H-bridge

The relay module is active-low:

```cpp
#define RELAY_ACTIVE_LEVEL LOW
#define RELAY_INACTIVE_LEVEL HIGH
```

Therefore:

```text
LOW  -> relay active
HIGH -> relay inactive
```

Critical restriction:

```text
TOOL_UP_PIN and TOOL_DOWN_PIN must never be LOW at the same time.
```

Activating both directions at the same time could create a short circuit in the relay H-bridge.

For this reason, the firmware does not directly activate the output pins from the main logic. Every activation goes through safe functions:

```cpp
bool activateUpRelaySafely();
bool activateDownRelaySafely();
```

For moving up:

```text
1. Deactivate DOWN: TOOL_DOWN_PIN = HIGH
2. Verify that DOWN is inactive
3. Activate UP: TOOL_UP_PIN = LOW
4. Verify that both relays are not active at the same time
```

For moving down:

```text
1. Deactivate UP: TOOL_UP_PIN = HIGH
2. Verify that UP is inactive
3. Activate DOWN: TOOL_DOWN_PIN = LOW
4. Verify that both relays are not active at the same time
```

In addition, every control cycle verifies:

```cpp
if (areBothRelaysActive()) {
  setFault(FAULT_RELAY_CONFLICT);
  return;
}
```

---

## 7. Fixed tool parameters

These parameters are not received from the Raspberry Pi. They are local safety parameters of the actuator and the tool:

```cpp
#define MOVEMENT_TIMEOUT_UP_MS    5000UL
#define MOVEMENT_TIMEOUT_DOWN_MS  5000UL
#define DIRECTION_DEAD_TIME_MS    100UL
#define CURRENT_LIMIT_A           1.2f
#define CURRENT_IGNORE_TIME_MS    50UL
#define CURRENT_SAMPLE_PERIOD_MS  10UL
#define CURRENT_FILTER_ALPHA      0.2f
```

### 7.1 Timeout

If the tool does not reach its target limit sensor within the maximum allowed time, the firmware raises one of the following faults:

```text
FAULT_TIMEOUT_UP
FAULT_TIMEOUT_DOWN
```

### 7.2 Dead time

Before changing or activating direction, the firmware waits:

```text
100 ms
```

This reduces the risk of simultaneous relay commutation.

### 7.3 Current limit

The overcurrent cut-off is configured as:

```text
1.2 A
```

The current filter avoids false trips caused by noise.

---

## 8. Current measurement

Current is measured on `A0` using the ATmega328P ADC.

The ADC uses `AVcc` as reference:

```cpp
#define ADC_REF_VOLTAGE 5.0f
```

The ADC-to-voltage conversion is:

```cpp
voltage = raw * ADC_REF_VOLTAGE / 1023.0f;
```

The current is calculated as:

```cpp
current = abs((voltage - currentZeroVoltage) / CURRENT_SENSITIVITY_V_PER_A);
```

### 8.1 Zero calibration

During startup, with the motor stopped, the firmware measures the resting voltage of the current sensor:

```cpp
currentZeroVoltage = calibrateCurrentZeroVoltage();
```

This compensates the current sensor offset.

### 8.2 ADC interrupt

The ADC interrupt service routine only stores the raw sample:

```cpp
ISR(ADC_vect)
{
  currentAdcRawISR = ADC;
  currentAdcNewSampleISR = true;
}
```

The conversion to amperes and the current filter are executed outside the interrupt routine.

### 8.3 Current filter

The firmware uses an exponential filter:

```cpp
filteredCurrentA =
  (1.0f - CURRENT_FILTER_ALPHA) * filteredCurrentA +
  CURRENT_FILTER_ALPHA * current;
```

The overcurrent protection uses only the filtered current:

```cpp
return filteredCurrentA > CURRENT_LIMIT_A;
```

---

## 9. Serial communication

The firmware uses:

```cpp
#include <PostNeoSWSerial.h>
```

Instance:

```cpp
PostNeoSWSerial rs485Serial(RX_PIN, TX_PIN);
```

Baud rate:

```cpp
rs485Serial.begin(9600);
```

`PostNeoSWSerial` is used to improve coexistence with interrupts compared with `SoftwareSerial`, because the firmware also uses:

- external interrupts for sensors;
- ADC interrupt;
- software serial communication.

---

## 10. HIL protocol

The firmware uses:

```cpp
#include "HilFrameArduino.h"
```

Frames used by the firmware:

```cpp
HilFrameArduino<64> rxFrame;
HilFrameArduino<64> txFrame;
```

The communication payload is based on `float` values.

### 10.1 Command received by the Arduino

Format:

```text
float[0] = device_id
float[1] = command
```

Commands:

| Code | Name | Description |
|---:|---|---|
| 0 | `CMD_STOP` | Stop the tool |
| 1 | `CMD_UP` | Request upward movement |
| 2 | `CMD_DOWN` | Request downward movement |
| 3 | `CMD_RESET_FAULT` | Clear active fault |
| 4 | `CMD_STATUS_REQUEST` | Request tool status |

---

## 11. ACK response

For every command except `CMD_STATUS_REQUEST`, the Arduino immediately responds with an ACK frame.

Format:

```text
float[0] = device_id
float[1] = RESPONSE_ACK
float[2] = command
float[3] = result_code
```

Results:

| Code | Result | Description |
|---:|---|---|
| 0 | `RESULT_OK` | Command accepted |
| 1 | `RESULT_REJECTED_FAULT` | Tool is in fault state |
| 2 | `RESULT_REJECTED_AT_TOP` | Tool is already at the upper limit |
| 3 | `RESULT_REJECTED_AT_BOTTOM` | Tool is already at the lower limit |
| 4 | `RESULT_INVALID_COMMAND` | Invalid command |
| 5 | `RESULT_ALREADY_MOVING` | Tool is already moving |
| 6 | `RESULT_REJECTED_SENSOR_CONFLICT` | Sensor conflict detected |

`RESULT_OK` means that the command was accepted. It does not mean that the movement has finished.

---

## 12. STATUS response

The status is sent only when the master sends:

```text
CMD_STATUS_REQUEST
```

Format:

```text
float[0] = device_id
float[1] = RESPONSE_STATUS
float[2] = tool_status
float[3] = fault_code
float[4] = HL_active
float[5] = LL_active
float[6] = filtered_current_A
```

Public tool states:

| Code | State | Description |
|---:|---|---|
| 0 | `STATUS_UNKNOWN` | Unknown state |
| 1 | `STATUS_AT_BOTTOM` | Tool is at the lower position |
| 2 | `STATUS_AT_TOP` | Tool is at the upper position |
| 3 | `STATUS_MOVING_UP` | Tool is moving up |
| 4 | `STATUS_MOVING_DOWN` | Tool is moving down |
| 5 | `STATUS_STOPPED_BETWEEN_LIMITS` | Tool is stopped between limits |
| 6 | `STATUS_FAULT` | Tool is in fault state |

---

## 13. Faults

| Code | Fault | Description |
|---:|---|---|
| 0 | `FAULT_NONE` | No fault |
| 1 | `FAULT_TIMEOUT_UP` | Timeout while moving up |
| 2 | `FAULT_TIMEOUT_DOWN` | Timeout while moving down |
| 3 | `FAULT_OVERCURRENT_UP` | Overcurrent while moving up |
| 4 | `FAULT_OVERCURRENT_DOWN` | Overcurrent while moving down |
| 5 | `FAULT_RELAY_CONFLICT` | UP and DOWN active simultaneously |
| 6 | `FAULT_SENSOR_CONFLICT` | HL and LL active simultaneously |

---

## 14. Recommended Raspberry Pi flow

### Move up

```text
1. Master -> Arduino: ID, CMD_UP
2. Arduino -> Master: ID, RESPONSE_ACK, CMD_UP, RESULT_OK
3. Master waits 100 ms or 200 ms
4. Master -> Arduino: ID, CMD_STATUS_REQUEST
5. Arduino -> Master: STATUS_MOVING_UP or STATUS_AT_TOP
```

### Move down

```text
1. Master -> Arduino: ID, CMD_DOWN
2. Arduino -> Master: ID, RESPONSE_ACK, CMD_DOWN, RESULT_OK
3. Master waits 100 ms or 200 ms
4. Master -> Arduino: ID, CMD_STATUS_REQUEST
5. Arduino -> Master: STATUS_MOVING_DOWN or STATUS_AT_BOTTOM
```

### Fault

If a fault occurs:

```text
tool_status = STATUS_FAULT
fault_code  = fault code
```

To clear the fault:

```text
Master -> Arduino: ID, CMD_RESET_FAULT
```

---

## 15. Raspberry Pi command examples

### Move up

```cpp
Frame request;
request.addFloat(1.0f);  // device ID
request.addFloat(1.0f);  // CMD_UP
hilSerial->sendFrame(request);
```

### Move down

```cpp
Frame request;
request.addFloat(1.0f);  // device ID
request.addFloat(2.0f);  // CMD_DOWN
hilSerial->sendFrame(request);
```

### Stop

```cpp
Frame request;
request.addFloat(1.0f);  // device ID
request.addFloat(0.0f);  // CMD_STOP
hilSerial->sendFrame(request);
```

### Reset fault

```cpp
Frame request;
request.addFloat(1.0f);  // device ID
request.addFloat(3.0f);  // CMD_RESET_FAULT
hilSerial->sendFrame(request);
```

### Request status

```cpp
Frame request;
request.addFloat(1.0f);  // device ID
request.addFloat(4.0f);  // CMD_STATUS_REQUEST
hilSerial->sendFrame(request);
```

---

## 16. Firmware files

The main firmware file is:

```text
ToolController.ino
```

The firmware depends on:

```text
HilFrameArduino.h
PostNeoSWSerial
```

`HilFrameArduino.h` implements binary HIL frame reading and writing. `PostNeoSWSerial` implements software serial communication on the configured pins.

---

## 17. Recommended safety measures

Software does not replace physical protection. The following protections are recommended:

- fuse in series with the actuator power supply;
- current-limited power supply;
- wires sized for the maximum expected current;
- reverse polarity protection;
- flyback diodes or a relay module with coil protection;
- physical/electrical interlock between relays if possible;
- verification with a multimeter that D2 and D3 never receive more than 5 V;
- initial tests without mechanical load;
- tests using a current-limited supply.

---

## 18. Test checklist

```text
[ ] Verify that HL and LL reach the Arduino with a maximum of 5 V.
[ ] Confirm that active HL is read as LOW.
[ ] Confirm that active LL is read as LOW.
[ ] Confirm that relays are inactive when the Arduino is powered.
[ ] Confirm that UP and DOWN never activate simultaneously.
[ ] Verify HIL communication using STATUS_REQUEST.
[ ] Verify ACK for STOP, UP, DOWN, and RESET_FAULT.
[ ] Test movement without load.
[ ] Test stopping by HL.
[ ] Test stopping by LL.
[ ] Test timeout by temporarily disconnecting the corresponding limit sensor.
[ ] Test overcurrent with a low threshold during bench testing.
[ ] Confirm that the fuse is in series with the actuator power supply.
```

---

## 19. Final protocol summary

Input to Arduino:

```text
ID, COMMAND
```

ACK output:

```text
ID, RESPONSE_ACK, COMMAND, RESULT_CODE
```

STATUS output:

```text
ID, RESPONSE_STATUS, TOOL_STATUS, FAULT_CODE, HL_ACTIVE, LL_ACTIVE, FILTERED_CURRENT_A
```

Communication principle:

```text
The Arduino never transmits spontaneously.
It only responds to a request initiated by the Raspberry Pi.
```

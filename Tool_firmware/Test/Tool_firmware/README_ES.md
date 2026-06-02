# Firmware de control de herramienta con Arduino Nano

Este `README.md` documenta el firmware de una herramienta accionada por un actuador eléctrico controlado mediante Arduino Nano. La herramienta se comunica con una Raspberry Pi usando un protocolo HIL binario sobre serial/RS485. El Arduino actúa como controlador local y ejecuta las funciones críticas de seguridad.

El sistema fue diseñado para que la Raspberry mande únicamente órdenes de alto nivel, mientras que el Arduino decide localmente si el movimiento es seguro.

---

## 1. Objetivo del firmware

El firmware controla una herramienta con dos movimientos:

- subir;
- bajar.

Además, supervisa:

- sensor superior `HL`;
- sensor inferior `LL`;
- corriente del actuador;
- timeout de movimiento;
- conflicto de sensores;
- conflicto de relés del puente H.

El Arduino responde inmediatamente a cada comando con un `ACK` o `NACK`. El estado completo de la herramienta se envía solo cuando el master solicita un `STATUS`.

---

## 2. Arquitectura de comunicación

La arquitectura es master-slave:

```text
Raspberry Pi / Master
        |
        | Protocolo HIL sobre serial / RS485
        |
Arduino Nano / Slave
        |
        |-- Puente H de relés
        |-- Actuador de herramienta
        |-- Sensor HL
        |-- Sensor LL
        |-- Sensor de corriente
```

Regla principal de comunicación:

```text
El Arduino nunca transmite espontáneamente.
El Arduino solo responde a una transacción iniciada por la Raspberry.
```

Esto es importante para permitir varias herramientas en el mismo bus RS485 sin colisiones.

---

## 3. Hardware considerado

### 3.1 Arduino

- Arduino Nano con ATmega328P.
- Comunicación serial por software usando `PostNeoSWSerial`.
- Entrada analógica `A0` para sensor de corriente.
- Interrupciones externas en `D2` y `D3` para sensores `HL` y `LL`.

### 3.2 Actuador

El actuador consume aproximadamente:

```text
1 A en operación normal
```

El firmware corta por sobrecorriente en:

```text
1.2 A
```

Además, se instaló un fusible de protección de aproximadamente:

```text
1.5 A
```

El fusible es una protección eléctrica final. La protección funcional contra trabamiento se hace por software usando el sensor de corriente.

### 3.3 Sensores HL y LL

Se usan sensores inductivos NPN tipo LJ12A3-4-Z/BX:

- `HL`: sensor de límite superior.
- `LL`: sensor de límite inferior.

Como los sensores trabajan con 24 V, su salida no debe conectarse directamente al Arduino. Se usa un divisor resistivo externo. En las pruebas se midió aproximadamente:

```text
salida del divisor hacia Arduino: ~4.6 V
```

Por eso los pines se configuran como entrada normal:

```cpp
pinMode(HL_SIGNAL, INPUT);
pinMode(LL_SIGNAL, INPUT);
```

No se usa `INPUT_PULLUP`.

### 3.4 Sensor de corriente

Se considera un sensor tipo ACS712 o equivalente con sensibilidad:

```text
185 mV/A
```

En el código:

```cpp
#define CURRENT_SENSITIVITY_V_PER_A 0.185f
```

---

## 4. Asignación de pines

| Señal | Pin Arduino | Descripción |
|---|---:|---|
| `HL_SIGNAL` | D2 | Sensor de límite superior, interrupción externa INT0 |
| `LL_SIGNAL` | D3 | Sensor de límite inferior, interrupción externa INT1 |
| `TOOL_DOWN_PIN` | D4 | Activación de movimiento hacia abajo |
| `TOOL_UP_PIN` | D5 | Activación de movimiento hacia arriba |
| `TX_PIN` | D7 | TX serial hacia Raspberry / módulo RS485 |
| `RX_PIN` | D8 | RX serial desde Raspberry / módulo RS485 |
| `CURRENT_SIGNAL` | A0 | Entrada analógica del sensor de corriente |

Definición usada:

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

## 5. Lógica de sensores

Los sensores NPN activos en bajo se interpretan así:

```text
LOW  -> sensor activo
HIGH -> sensor inactivo
```

En el firmware:

```cpp
#define SENSOR_ACTIVE_LEVEL LOW
```

Por lo tanto:

```text
HL activo -> herramienta arriba
LL activo -> herramienta abajo
```

Los sensores se actualizan mediante interrupciones:

```cpp
attachInterrupt(digitalPinToInterrupt(HL_SIGNAL), onHLChange, CHANGE);
attachInterrupt(digitalPinToInterrupt(LL_SIGNAL), onLLChange, CHANGE);
```

Las ISR solo actualizan variables globales:

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

No se ejecuta lógica de movimiento dentro de las interrupciones.

---

## 6. Control seguro del puente H de relés

El módulo de relés es activo en bajo:

```cpp
#define RELAY_ACTIVE_LEVEL LOW
#define RELAY_INACTIVE_LEVEL HIGH
```

Por tanto:

```text
LOW  -> relé activo
HIGH -> relé inactivo
```

Restricción crítica:

```text
TOOL_UP_PIN y TOOL_DOWN_PIN nunca pueden estar en LOW al mismo tiempo.
```

Esto podría provocar un cortocircuito en el puente H.

Por eso el firmware no activa los pines directamente desde la lógica principal. Toda activación pasa por funciones seguras:

```cpp
bool activateUpRelaySafely();
bool activateDownRelaySafely();
```

Para subir:

```text
1. Desactivar DOWN: TOOL_DOWN_PIN = HIGH
2. Verificar que DOWN está inactivo
3. Activar UP: TOOL_UP_PIN = LOW
4. Verificar que no estén ambos activos
```

Para bajar:

```text
1. Desactivar UP: TOOL_UP_PIN = HIGH
2. Verificar que UP está inactivo
3. Activar DOWN: TOOL_DOWN_PIN = LOW
4. Verificar que no estén ambos activos
```

Además, en cada ciclo de control se verifica:

```cpp
if (areBothRelaysActive()) {
  setFault(FAULT_RELAY_CONFLICT);
  return;
}
```

---

## 7. Parámetros fijos por herramienta

Estos parámetros no se reciben desde la Raspberry. Son propios del actuador y de la herramienta:

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

Si la herramienta no alcanza su sensor final dentro del tiempo máximo, se genera una falla:

```text
FAULT_TIMEOUT_UP
FAULT_TIMEOUT_DOWN
```

### 7.2 Dead time

Antes de cambiar o activar dirección se espera:

```text
100 ms
```

Esto reduce el riesgo de conmutación simultánea de relés.

### 7.3 Límite de corriente

El corte por sobrecorriente está configurado en:

```text
1.2 A
```

El filtro evita disparos falsos por ruido.

---

## 8. Lectura de corriente

La lectura de corriente se realiza en `A0` mediante el ADC del ATmega328P.

El ADC usa como referencia `AVcc`:

```cpp
#define ADC_REF_VOLTAGE 5.0f
```

La conversión de ADC a voltaje se hace como:

```cpp
voltage = raw * ADC_REF_VOLTAGE / 1023.0f;
```

La corriente se calcula como:

```cpp
current = abs((voltage - currentZeroVoltage) / CURRENT_SENSITIVITY_V_PER_A);
```

### 8.1 Calibración de cero

Al iniciar, con el motor detenido, se mide el voltaje de reposo del sensor:

```cpp
currentZeroVoltage = calibrateCurrentZeroVoltage();
```

Esto compensa el offset del sensor de corriente.

### 8.2 ADC por interrupción

La ISR del ADC solo guarda la muestra cruda:

```cpp
ISR(ADC_vect)
{
  currentAdcRawISR = ADC;
  currentAdcNewSampleISR = true;
}
```

El cálculo en amperios y el filtro se hacen fuera de la interrupción.

### 8.3 Filtro de corriente

Se usa un filtro exponencial:

```cpp
filteredCurrentA =
  (1.0f - CURRENT_FILTER_ALPHA) * filteredCurrentA +
  CURRENT_FILTER_ALPHA * current;
```

La protección por sobrecorriente usa solo la corriente filtrada:

```cpp
return filteredCurrentA > CURRENT_LIMIT_A;
```

---

## 9. Comunicación serial

Se usa:

```cpp
#include <PostNeoSWSerial.h>
```

Instancia:

```cpp
PostNeoSWSerial rs485Serial(RX_PIN, TX_PIN);
```

Velocidad:

```cpp
rs485Serial.begin(9600);
```

`PostNeoSWSerial` se usa para mejorar la convivencia con interrupciones respecto a `SoftwareSerial`, ya que el firmware también usa:

- interrupciones externas para sensores;
- interrupción del ADC;
- comunicación serial por software.

---

## 10. Protocolo HIL

El firmware usa:

```cpp
#include "HilFrameArduino.h"
```

Frames usados:

```cpp
HilFrameArduino<64> rxFrame;
HilFrameArduino<64> txFrame;
```

La comunicación se basa en `float`.

### 10.1 Comando recibido por Arduino

Formato:

```text
float[0] = device_id
float[1] = command
```

Comandos:

| Código | Nombre | Descripción |
|---:|---|---|
| 0 | `CMD_STOP` | Detiene la herramienta |
| 1 | `CMD_UP` | Solicita subir |
| 2 | `CMD_DOWN` | Solicita bajar |
| 3 | `CMD_RESET_FAULT` | Limpia falla |
| 4 | `CMD_STATUS_REQUEST` | Solicita status |

---

## 11. Respuesta ACK

Para todos los comandos excepto `CMD_STATUS_REQUEST`, el Arduino responde inmediatamente con un ACK.

Formato:

```text
float[0] = device_id
float[1] = RESPONSE_ACK
float[2] = command
float[3] = result_code
```

Resultados:

| Código | Resultado | Descripción |
|---:|---|---|
| 0 | `RESULT_OK` | Comando aceptado |
| 1 | `RESULT_REJECTED_FAULT` | Herramienta en falla |
| 2 | `RESULT_REJECTED_AT_TOP` | Ya está arriba |
| 3 | `RESULT_REJECTED_AT_BOTTOM` | Ya está abajo |
| 4 | `RESULT_INVALID_COMMAND` | Comando inválido |
| 5 | `RESULT_ALREADY_MOVING` | Ya está moviéndose |
| 6 | `RESULT_REJECTED_SENSOR_CONFLICT` | Conflicto de sensores |

`RESULT_OK` significa que el comando fue aceptado, no que el movimiento terminó.

---

## 12. Respuesta STATUS

El status se envía solo cuando el master manda:

```text
CMD_STATUS_REQUEST
```

Formato:

```text
float[0] = device_id
float[1] = RESPONSE_STATUS
float[2] = tool_status
float[3] = fault_code
float[4] = HL_active
float[5] = LL_active
float[6] = filtered_current_A
```

Estados públicos:

| Código | Estado | Descripción |
|---:|---|---|
| 0 | `STATUS_UNKNOWN` | Estado desconocido |
| 1 | `STATUS_AT_BOTTOM` | Herramienta abajo |
| 2 | `STATUS_AT_TOP` | Herramienta arriba |
| 3 | `STATUS_MOVING_UP` | Subiendo |
| 4 | `STATUS_MOVING_DOWN` | Bajando |
| 5 | `STATUS_STOPPED_BETWEEN_LIMITS` | Parada entre límites |
| 6 | `STATUS_FAULT` | Herramienta en falla |

---

## 13. Fallas

| Código | Falla | Descripción |
|---:|---|---|
| 0 | `FAULT_NONE` | Sin falla |
| 1 | `FAULT_TIMEOUT_UP` | Timeout al subir |
| 2 | `FAULT_TIMEOUT_DOWN` | Timeout al bajar |
| 3 | `FAULT_OVERCURRENT_UP` | Sobrecorriente al subir |
| 4 | `FAULT_OVERCURRENT_DOWN` | Sobrecorriente al bajar |
| 5 | `FAULT_RELAY_CONFLICT` | UP y DOWN activos simultáneamente |
| 6 | `FAULT_SENSOR_CONFLICT` | HL y LL activos simultáneamente |

---

## 14. Flujo recomendado desde Raspberry

### Subir

```text
1. Master -> Arduino: ID, CMD_UP
2. Arduino -> Master: ID, RESPONSE_ACK, CMD_UP, RESULT_OK
3. Master espera 100 ms o 200 ms
4. Master -> Arduino: ID, CMD_STATUS_REQUEST
5. Arduino -> Master: STATUS_MOVING_UP o STATUS_AT_TOP
```

### Bajar

```text
1. Master -> Arduino: ID, CMD_DOWN
2. Arduino -> Master: ID, RESPONSE_ACK, CMD_DOWN, RESULT_OK
3. Master espera 100 ms o 200 ms
4. Master -> Arduino: ID, CMD_STATUS_REQUEST
5. Arduino -> Master: STATUS_MOVING_DOWN o STATUS_AT_BOTTOM
```

### Falla

Si ocurre una falla:

```text
tool_status = STATUS_FAULT
fault_code  = código de falla
```

Para salir de falla:

```text
Master -> Arduino: ID, CMD_RESET_FAULT
```

---

## 15. Ejemplos de comandos desde Raspberry

### Subir

```cpp
Frame request;
request.addFloat(1.0f);  // device ID
request.addFloat(1.0f);  // CMD_UP
hilSerial->sendFrame(request);
```

### Bajar

```cpp
Frame request;
request.addFloat(1.0f);  // device ID
request.addFloat(2.0f);  // CMD_DOWN
hilSerial->sendFrame(request);
```

### Detener

```cpp
Frame request;
request.addFloat(1.0f);  // device ID
request.addFloat(0.0f);  // CMD_STOP
hilSerial->sendFrame(request);
```

### Resetear falla

```cpp
Frame request;
request.addFloat(1.0f);  // device ID
request.addFloat(3.0f);  // CMD_RESET_FAULT
hilSerial->sendFrame(request);
```

### Solicitar status

```cpp
Frame request;
request.addFloat(1.0f);  // device ID
request.addFloat(4.0f);  // CMD_STATUS_REQUEST
hilSerial->sendFrame(request);
```

---

## 16. Archivos del firmware

El firmware principal está en:

```text
ToolController.ino
```

El firmware depende de:

```text
HilFrameArduino.h
PostNeoSWSerial
```

`HilFrameArduino.h` implementa la lectura y escritura de frames HIL binarios. `PostNeoSWSerial` implementa la comunicación serial por software en los pines definidos.

---

## 17. Seguridad recomendada

El software no reemplaza las protecciones físicas. Se recomienda:

- fusible en serie con la alimentación del actuador;
- fuente con límite de corriente;
- cables dimensionados para la corriente máxima;
- protección contra inversión de polaridad;
- diodos flyback o módulo de relés con protección;
- enclavamiento eléctrico físico entre relés si es posible;
- verificación con multímetro de que D2 y D3 nunca reciben más de 5 V;
- pruebas iniciales sin carga mecánica;
- pruebas con fuente limitada en corriente.

---

## 18. Checklist de pruebas

```text
[ ] Verificar que HL y LL llegan al Arduino con máximo 5 V.
[ ] Confirmar que HL activo se lee como LOW.
[ ] Confirmar que LL activo se lee como LOW.
[ ] Confirmar que los relés están inactivos al energizar el Arduino.
[ ] Confirmar que UP y DOWN nunca se activan simultáneamente.
[ ] Verificar comunicación HIL con STATUS_REQUEST.
[ ] Verificar ACK para STOP, UP, DOWN y RESET_FAULT.
[ ] Probar movimiento sin carga.
[ ] Probar parada por HL.
[ ] Probar parada por LL.
[ ] Probar timeout desconectando temporalmente el sensor correspondiente.
[ ] Probar sobrecorriente con un umbral bajo en banco.
[ ] Confirmar que el fusible está en serie con la alimentación del actuador.
```

---

## 19. Resumen del protocolo final

Entrada al Arduino:

```text
ID, COMMAND
```

Salida ACK:

```text
ID, RESPONSE_ACK, COMMAND, RESULT_CODE
```

Salida STATUS:

```text
ID, RESPONSE_STATUS, TOOL_STATUS, FAULT_CODE, HL_ACTIVE, LL_ACTIVE, FILTERED_CURRENT_A
```

Principio de comunicación:

```text
El Arduino nunca transmite espontáneamente.
Solo responde a una solicitud iniciada por la Raspberry.
```

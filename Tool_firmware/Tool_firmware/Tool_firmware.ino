#include <SoftwareSerial.h>

#define RX_PIN 8   // D5 recibe datos
#define TX_PIN 7   // D6 transmite datos
#define TOOL_UP_PIN 5  // D5 in LOW FOR TOOL UP, !!NEVER BOTH IN LOW!!
#define TOOL_DOWN_PIN 4  //D4 in LOW FOR TOOL DOW, !!NEVER BOTH IN LOW!!  
#define BAUDRATE 9600  // 2 Mbps, máxima velocidad típica estable en Nano 16 MHz

SoftwareSerial Serial485(RX_PIN,TX_PIN);

void setup() {
  pinMode(4, OUTPUT);  // Configura el pin D2 como salida
  pinMode(5, OUTPUT);  // Configura el pin D2 como salida
  digitalWrite(5, HIGH); // Enciende / manda 5V al pin D2
  digitalWrite(4, HIGH); // Enciende / manda 5V al pin D2
  
  Serial.begin(BAUDRATE);     // USB hacia la PC
  Serial485.begin(BAUDRATE);   // Serial por D8/D7

  Serial.println("Arduino listo");
}

void loop() {
  // Ejemplo de envío por D6
  Serial485.println("Hola");
  delay(1000);
}

void upTool(){
  digitalWrite(TOOL_DOWN_PIN, HIGH); // Deactivate tool_down first for security
  delay(100);
  digitalWrite(TOOL_UP_PIN, LOW); // Enciende / manda 5V al pin D2
  delay(5000);
  digitalWrite(5, HIGH); // Enciende / manda 5V al pin D2
}

void downTool(){
  digitalWrite(TOOL_UP_PIN, HIGH); // Deactivate tool_up first for security
  delay(100);
  digitalWrite(TOOL_DOWN_PIN, LOW); // Enciende / manda 5V al pin D2
  delay(5000);
  digitalWrite(TOOL_DOWN_PIN, HIGH); // Enciende / manda 5V al pin D2
}
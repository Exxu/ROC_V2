#include <SoftwareSerial.h>

#define RX_PIN 8   // D8 recibe datos
#define TX_PIN 7   // D7 transmite datos
#define TOOL_DOWN_PIN 4   // D7 transmite datos
#define TOOL_UP_PIN 5   // D7 transmite datos


SoftwareSerial rs485Serial(RX_PIN, TX_PIN);

void setup() {
  // put your setup code here, to run once:
  pinMode(TOOL_DOWN_PIN, OUTPUT);  // Configura el pin D2 como salida
  pinMode(5, OUTPUT);  // Configura el pin D2 como salida
  digitalWrite(TOOL_UP_PIN, HIGH); // Enciende / manda 5V al pin D2
  digitalWrite(TOOL_DOWN_PIN, HIGH); // Enciende / manda 5V al pin D2

   Serial.begin(9600);     // Serial USB hacia la PC
  rs485Serial.begin(9600);   // Serial por software en D8/D7

  Serial.println("Arduino listo");

}

void loop() {
  // put your main code here, to run repeatedly:
  rs485Serial.println("Hola");
  delay(1000);
}

void upTool(){
  digitalWrite(TOOL_DOWN_PIN, HIGH); // Enciende / manda 5V al pin D2
  delay(100);
  digitalWrite(TOOL_UP_PIN, LOW); // Enciende / manda 5V al pin D2
  delay(5000);
  digitalWrite(TOOL_UP_PIN, HIGH); // Enciende / manda 5V al pin D2
}

void downTool(){
  digitalWrite(TOOL_UP_PIN, HIGH); // Enciende / manda 5V al pin D2
  delay(100);
  digitalWrite(TOOL_DOWN_PIN, LOW); // Enciende / manda 5V al pin D2
  delay(5000);
  digitalWrite(TOOL_DOWN_PIN, HIGH); // Enciende / manda 5V al pin D2
}
#include <SoftwareSerial.h>
#include "HilFrameArduino.h"

#define RX_PIN 8   // D8 recibe datos
#define TX_PIN 7   // D7 transmite datos
#define TOOL_DOWN_PIN 4   // D7 transmite datos
#define TOOL_UP_PIN 5   // D7 transmite datos
#define ID 1

SoftwareSerial rs485Serial(RX_PIN, TX_PIN);

HilFrameArduino<64> rxFrame;
HilFrameArduino<64> txFrame;
uint8_t device_id;
uint8_t command;

void setup() {
  // put your setup code here, to run once:
  pinMode(TOOL_DOWN_PIN, OUTPUT);  // Configura el pin D2 como salida
  pinMode(5, OUTPUT);  // Configura el pin D2 como salida
  digitalWrite(TOOL_UP_PIN, HIGH); // Enciende / manda 5V al pin D2
  digitalWrite(TOOL_DOWN_PIN, HIGH); // Enciende / manda 5V al pin D2
  device_id = ID;
  command = 0;

  Serial.begin(9600);     // Serial USB hacia la PC
  rs485Serial.begin(9600);   // Serial por software en D8/D7

  Serial.println("Arduino listo");

}

void loop() {
  // put your main code here, to run repeatedly:
  if (rxFrame.readFrom(rs485Serial, 10)) {
    Serial.print("Frame received. Payload bytes: ");
    Serial.println(rxFrame.size());

    rxFrame.resetReadIndex();

    uint8_t device = 0.0;
    rxFrame.getByte(device);

    if (device == device_id) {

      rxFrame.getByte(command);
      
      switch (command){
        case 1:
          Serial.print("Command 1");
        case 2:
          Serial.print("Command 2");
        default:
          Serial.print("Command = ");
          Serial.println(command);
      }
    }
  }
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
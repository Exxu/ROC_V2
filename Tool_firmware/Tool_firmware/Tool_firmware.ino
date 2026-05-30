#include <SoftwareSerial.h>
#include "HilFrameArduino.h"

#define LL_SIGNAL 3        // Low leve tool sensor
#define HL_SIGNAL 2        // High leve tool sensor
#define RX_PIN 8           // RX of RS485 transmiter
#define TX_PIN 7           // TX of RS485 transmiter
#define TOOL_DOWN_PIN 4    // Move down tool _ Low to activate !!NEVER ACTIVATE BOTH!!
#define TOOL_UP_PIN 5      // Move up tool _ Low to activate !!NEVER ACTIVATE BOTH!! 
#define ID 1               // ID of the Tool
#define CURRENT_SIGNAL A0  //Current sensor 185mV / A

SoftwareSerial rs485Serial(RX_PIN, TX_PIN);

HilFrameArduino<64> rxFrame;
HilFrameArduino<64> txFrame;
float device_id;
float command;

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
  if (rxFrame.readFrom(rs485Serial, 500)) {
    Serial.print("Frame received. Payload bytes: ");
    Serial.println(rxFrame.size());

    rxFrame.resetReadIndex();

    float device = 0.0;
    rxFrame.getFloat(device);

    if (device == device_id) {

      rxFrame.getFloat(command);
      
      switch ((int)command){
        case 1:
          Serial.println("Command 1");
          break;
        case 2:
          Serial.println("Command 2");
          break;
        default:
          Serial.print("Command = ");
          Serial.println(command);
      }
    }else{
       Serial.print("No ID");
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
void setup() {
  // put your setup code here, to run once:
  pinMode(4, OUTPUT);  // Configura el pin D2 como salida
  pinMode(5, OUTPUT);  // Configura el pin D2 como salida
  digitalWrite(5, HIGH); // Enciende / manda 5V al pin D2
  digitalWrite(4, HIGH); // Enciende / manda 5V al pin D2

}

void loop() {
  // put your main code here, to run repeatedly:
    upTool(); // Enciende / manda 5V al pin D2
    delay(1000);

    downTool();  // Apaga / manda 0V al pin D2
    delay(1000);
}

void upTool(){
  digitalWrite(4, HIGH); // Enciende / manda 5V al pin D2
  delay(100);
  digitalWrite(5, LOW); // Enciende / manda 5V al pin D2
  delay(5000);
  digitalWrite(5, HIGH); // Enciende / manda 5V al pin D2
}

void downTool(){
  digitalWrite(5, HIGH); // Enciende / manda 5V al pin D2
  delay(100);
  digitalWrite(4, LOW); // Enciende / manda 5V al pin D2
  delay(5000);
  digitalWrite(4, HIGH); // Enciende / manda 5V al pin D2
}
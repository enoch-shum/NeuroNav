#include <Arduino.h>

// put function declarations here:
int myFunction(int, int);
int input = 27;

void setup() {
  // put your setup code here, to run once:
  int result = myFunction(2, 3);
  pinMode(input, OUTPUT)
}

void loop() {
  // put your main code here, to run repeatedly:
    delay(3000);
  digitalWrite(input, HIGH);
  delay(3000);
  digitalWrite(input, LOW);
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}
#include <Arduino.h>

#include <ESP32Servo.h>

Servo servo;

#define SERVO_PIN 18

void setup() {
  // put your setup code here, to run once:
  
  servo.attach(SERVO_PIN);
  delay(100);
}

void loop() {
  // put your main code here, to run repeatedly:
    for (int pos = 0; pos <= 180; pos += 1) { // goes from 0 degrees to 180 degrees
    // in steps of 1 degree
    servo.write(pos);              // tell servo to go to position in variable 'pos'
    delay(15);                       // waits 15ms for the servo to reach the position
  }
    for (int pos = 180; pos >= 0; pos -= 1) { // goes from 180 degrees to 0 degrees
        servo.write(pos);              // tell servo to go to position in variable 'pos'
        delay(15);                       // waits 15ms for the servo to reach the position
    }
  
  
}
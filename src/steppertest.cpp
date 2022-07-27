/*
 Stepper Motor Test
 2022 EricSnis

 This program is to test the connection of a stepper motor to the stepper driver.
 The motor will rotate clockwise then counterclockwise repeatedly.

 The motor driver is attached to the following pins on the arduino
  Driver Input 1 - Arduino 19
  Driver Input 2 - Arduino 18
  Driver Input 3 - Arduino 5
  Driver Input 4 - Arduino 17
  Pins can be changed to suit your particular set up.
  Note: GPIO 6-11 are used for flash memory and cannot be used

 */

#include <Arduino.h>
#include <Stepper.h>

const int Stepper1In1 = 19; // Input pin 1 of stepper driver
const int Stepper1In2 = 18; // Input pin 2 of stepper driver
const int Stepper1In3 = 5;  // Input pin 3 of stepper driver
const int Stepper1In4 = 17; // Input pin 4 of stepper driver

const int stepsPerRevolution = 2048; // number of steps for one revolution of motor
const int stepperSpeed = 10;         // Speed of motor in RPM

Stepper stepper1(stepsPerRevolution, Stepper1In1, Stepper1In3, Stepper1In2, Stepper1In4); // Create instance of stepper for stepper 1

void setup()
{
  Serial.begin(115200);            // Set-up serial port for debugging
  stepper1.setSpeed(stepperSpeed); // Set stepper speed in RPM
}

void loop()
{
  // Rotate one revolution in clockwise direction:
  Serial.println("rotating clockwise");
  stepper1.step(stepsPerRevolution);
  delay(500);

  // Rotate one revolution in counterclockwise direction:
  Serial.println("rotating counterclockwise");
  stepper1.step(-stepsPerRevolution);
  delay(500);
}
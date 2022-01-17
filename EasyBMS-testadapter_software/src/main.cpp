#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"


void setup() 
{
  // put your setup code here, to run once:
    pinMode(D6, OUTPUT);
}

void loop() 
{
  // put your main code here, to run repeatedly:
  digitalWrite(D6, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(1000);                       // wait for a second
  digitalWrite(D6, LOW);    // turn the LED off by making the voltage LOW
  delay(1000);   
}
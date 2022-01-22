/*
  Dieser Testcode soll alle Hardwarefunktionen der Testbench durchgehen und prüfen
*/


#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <string.h>
#include <ESP8266WiFi.h>
#include <uMQTTBroker.h>



#define OUT_LED_RED D5
#define OUT_LED_GREEN D6
#define OUT_MOSFET1 D7
#define OUT_MOSFET2 D8
#define IN_SW_PUSH A0
#define IN_SW_DIP1 D0
#define IN_SW_DIP2 D3
#define IN_SW_DIP3 D4
#define OLED_RESET     0 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
uMQTTBroker myBroker;
PubSubClient client(espClient);

int buttonStatePush;
int buttonStateDIP1;
int buttonStateDIP2;
int buttonStateDIP3;
int step;               //counter for the step sequence
String StepMessage;
String StepMessage2;
long int timer1;
long int timedelay1;

void read_all_buttons();
void testdrawchar(void);
void testregime();
void writeMessageOnDisplay(String message);
void writeMessage2OnDisplay(String message);
bool checkTimer();
void setTimer(long int timedelay);
void writeStepOnDisplay();



// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(OUT_LED_RED, OUTPUT);
  pinMode(OUT_LED_GREEN, OUTPUT);
  pinMode(OUT_MOSFET1, OUTPUT);
  pinMode(OUT_MOSFET2, OUTPUT);  
  pinMode(IN_SW_PUSH, INPUT);
  pinMode(IN_SW_DIP1, INPUT);
  pinMode(IN_SW_DIP2, INPUT);
  pinMode(IN_SW_DIP3, INPUT);

  Serial.begin(9600);
  step=0;
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) 
  { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.display();
  delay(2000); // Pause for 2 seconds
  Serial.println("Konfiguriere soft-AP ... ");
  boolean result = WiFi.softAP("WemosAP", "geheim123");
  
  Serial.print("Verbindung wurde ");
  if(result == false){
    Serial.println("NICHT ");
  }
  Serial.println("erfolgreich aufgebaut!");
    // Start the broker
  Serial.println("Starting MQTT broker");
  myBroker.init();
  client.subscribe("inTopic");

  //Clear the buffer
  display.clearDisplay();
  testdrawchar();      // Draw characters of the default font
  delay(500); // Pause for 2 seconds
  display.clearDisplay();
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.println(F("Setup abgeschlossen"));
  display.display();
  delay(500);                       // wait for a second       
}




// the loop function runs over and over again forever
void loop() {

  read_all_buttons();
  testregime();
  display.clearDisplay();
  writeStepOnDisplay();
  writeMessageOnDisplay(StepMessage);
  writeMessage2OnDisplay(StepMessage2);
  display.display();

  delay(100);                       // wait for a second

}

void testregime()
{
  switch (step) {
  case 0:
    //Initial step
    StepMessage="Test bereit";
    StepMessage2="Taster druecken";
    if(buttonStatePush)
    {  
      step=10;
      setTimer(1000);
    }
    break;

  case 10:
    //
    StepMessage="Starte Kommunikation";
    StepMessage2="Sende befehle";
    if(checkTimer())
      step=20;
    break;

  case 20:
    //
    step=30;
    setTimer(1000);

  case 30:
    //
    StepMessage="Antwort erhalten";
    digitalWrite(OUT_LED_GREEN, HIGH); 
    StepMessage2="Alles gut";
    if(checkTimer())
    {
      step=0;
      digitalWrite(OUT_LED_GREEN, LOW);  
    }
    break;

  default:
    //Fehler
    StepMessage="Fehler Schrittkette";
    break;
  }
}

void setTimer(long int timedelay)
{
  timer1=millis();
  timedelay1= timedelay;
}

bool checkTimer()
{
  int milli=millis();
  //Serial.print("Milli:");
  //Serial.println(milli);
  if (milli>(timer1+timedelay1))
    {
      return true;
    }
  else
    {
      return false;
    }
}

void read_all_buttons()
{
  int sensorValue = analogRead(IN_SW_PUSH);
  //Serial.println(sensorValue);
  if(sensorValue<400)
    buttonStatePush=1;
  else
    buttonStatePush=0;

  buttonStateDIP1= !digitalRead(IN_SW_DIP1);
  buttonStateDIP2= !digitalRead(IN_SW_DIP2);
  buttonStateDIP3= !digitalRead(IN_SW_DIP3);
  
}

void testdrawchar(void){
  display.clearDisplay();

  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  // Not all the characters will fit on the display. This is normal.
  // Library will draw what it can and the rest will be clipped.
  for(int16_t i=0; i<256; i++) {
    if(i == '\n') display.write(' ');
    else          display.write(i);
  }

  display.display();
  delay(2000);
}


void writeMessageOnDisplay(String message)
{
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(0, 10);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.println(message);
}

void writeMessage2OnDisplay(String message)
{
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(0, 20);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.println(message);
}


void writeStepOnDisplay()
{
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(100, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.println(step);
 
}
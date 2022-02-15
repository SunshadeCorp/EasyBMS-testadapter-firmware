/*
  Dieser Testcode soll alle Hardwarefunktionen der Testbench durchgehen und prüfen
*/

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <string.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

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

const char * wifi_ssid = "Vodafone-A054";
const char * wifi_pw = "ZfPsTdk74daGbGFp";
const char * mqtt_host = "xxx.xxx.xxx.xxx";
const char * mqtt_user = "";
const char * mqtt_pw = "";
const uint16_t mqtt_port = 1883;

String availability_topic = "easybms-test-adapter/available";

WiFiClient wifiClient;
PubSubClient client(mqtt_host, mqtt_port, wifiClient);

int buttonStatePush;
int buttonStateDIP1;
int buttonStateDIP2;
int buttonStateDIP3;
String StepMessage;
String StepMessage2;
long int timer1;
long int timedelay1;
TestState state;

void read_all_buttons();
void testdrawchar(void);
void testregime();
void writeMessageOnDisplay(String message);
void writeMessage2OnDisplay(String message);
bool checkTimer();
void setTimer(long int timedelay);
void writeStepOnDisplay();

enum class TestState {
  button_pressed,
  write_on_display,
  idle,
  measure_cell_voltage,
};

/*
TODO:

Welche Topics müssen subscribed werden?
Welche Topics müssen published werden?

Topics to subscribe:
module-number: 0-17 // TODO: does it start at 0?
cell-number: 0-11 // TODO: does it start at 0?

esp-module/{{module-number}}/uptime
esp-module/{{module-number}}/cell/{{cell-number}}/is_balancing
esp-module/{{module-number}}/cell/{{cell-number}}/voltage
esp-module/{{module-number}}/module_voltage
esp-module/{{module-number}}/module_temps
esp-module/{{module-number}}/chip_temp
esp-module/{{module-number}}/timediff
*/

void printWifiStatus(wl_status_t status) {
    switch(status) {
        case WL_CONNECTED:
            Serial.println("Connected");
            break;
        case WL_NO_SSID_AVAIL:
            Serial.println("No SSID Available");
            break;
        case WL_CONNECT_FAILED:
            Serial.println("Conneciton Failed");
            break;
        case WL_IDLE_STATUS:
            Serial.println("Idle Status");
            break;
        case WL_DISCONNECTED:
            Serial.println("Disconnected");
            break;
        default:
            return;
    }
}

void setup_wifi(String ssid, String password) {
  // Print MAC-Address
  String deviceId = WiFi.macAddress();
  Serial.print("Device ID: ");
  Serial.println(deviceId);

  // Connect to Wifi
  WiFi.mode(WIFI_STA);
  WiFi.hostname("easybms-test-adapter");
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
 
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect_mqtt()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_host, mqtt_user, mqtt_pw, availability_topic.c_str(), 0, true, "offline"))
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(availability_topic.c_str(), "online", true);

      // Routes to subscribe
      int module_number = 0; 
      client.subscribe((String("esp-module/") + module_number + "/uptime").c_str());
      client.subscribe((String("esp-module/") + module_number + "/module_voltage").c_str());
      client.subscribe((String("esp-module/") + module_number + "/module_temps").c_str());
      client.subscribe((String("esp-module/") + module_number + "/chip_temp").c_str());
      client.subscribe((String("esp-module/") + module_number + "/timediff").c_str());

      int number_of_cells = 12;
      for(int cell_number = 0; cell_number < number_of_cells; cell_number++) {
        client.subscribe((String("esp-module/") + module_number + "/cell/" + cell_number + "/is_balancing").c_str());
        client.subscribe((String("esp-module/") + module_number + "/cell/" + cell_number + "/voltage").c_str());
      } 
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());

      Serial.println(" try again in 3 seconds");
      delay(3000);
      return;
    }
  }
}

void mqtt_callback(char *topic, byte *payload, unsigned int length) {
  String topic_string = String(topic);
  String payload_string = String();
  payload_string.concat((char *) payload, length);

  if (topic_string == "xyz") {
    if (payload_string == "xy") {
    } else if (payload_string == "off") {
    }
  }
}

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

  Serial.begin(74880);
  setup_wifi(wifi_ssid, wifi_pw);
  client.setCallback(mqtt_callback);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) 
  { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.display();
  delay(2000); // Pause for 2 seconds
  Serial.println("Configuring soft-AP ... ");
  
    // Start the broker
  Serial.println("Starting MQTT broker");
  
  //Clear the buffer
  display.clearDisplay();
  testdrawchar(); // Draw characters of the default font
  delay(500); // Pause for 2 seconds
  display.clearDisplay();
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.println(F("Setup abgeschlossen"));
  display.display();
  delay(500);                       // wait for a second       

  Serial.println("Finished Setup");
}


void publish_switch_state() {
  //client.publish(state_topic.c_str(), get_switch_state().c_str(), true);
  //client.publish(available_topic.c_str(), "online", true);

  auto module_number = 0;
  auto cell_number = 0;
  auto balance_time_ms = 25000; 


  client.publish((String("esp-module/") + module_number + "/cell/" + cell_number + "/balance_request").c_str(), String(balance_time_ms).c_str(), true);
}

void write_on_display(String message1, String message2) {
  display.clearDisplay();
  writeStepOnDisplay();
  writeMessageOnDisplay(message1);
  writeMessage2OnDisplay(message2);
  display.display();
}

// the loop function runs over and over again forever
void loop() {
  Serial.println("Starting loop");
  read_all_buttons();
  testregime();

  switch (state) {
    //case TestState::button_pressed:
    //  break;

    case TestState::idle:
      break;

    //case TestState::write_on_display:
    //  write_on_display()
    //  break;

    default:
      Serial.println("Unknown Test State");
  }

}


void testregime()
{
  write_on_display("Test bereit", "Taster drücken")
  wait_for_button();



  /*
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
  */
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
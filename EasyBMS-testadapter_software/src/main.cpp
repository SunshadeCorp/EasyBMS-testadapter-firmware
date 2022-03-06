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

enum class TestState {
  idle,
  button_pressed,
  test_conf_slave,
  test_cell_voltages_zero,
  activate_cell_voltages,
  test_cell_voltages_real,
  test_cell_balancing,
  test_aux_voltage,
  test_temp_voltages,
  test_finished,
};

struct DipSwitch {
  uint8_t DIP1 : 1;
  uint8_t DIP2 : 1;
  uint8_t DIP3 : 1;
};

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

String StepMessage;
String StepMessage2;
long int timer1;
long int timedelay1;
TestState state;

auto module_number = 0;
auto mac_address = 0x0000000000000000;
auto balance_time_ms = 5000;
int number_of_cells = 12;

void read_all_buttons();
void testdrawchar(void);
void testregime();
void writeMessageOnDisplay(String message);
void writeMessage2OnDisplay(String message);
bool checkTimer();
void setTimer(long int timedelay);
void writeStepOnDisplay();

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

bool read_switch()
{
  return analogRead(IN_SW_PUSH) < 400;
}

DipSwitch read_dip_switches()
{
  DipSwitch switches;
  switches.DIP1 = !digitalRead(IN_SW_DIP1);
  switches.DIP2 = !digitalRead(IN_SW_DIP2);
  switches.DIP3 = !digitalRead(IN_SW_DIP3);

  return switches;
  
}

void write_on_display(String message1, String message2) {
  display.clearDisplay();
  writeStepOnDisplay();
  writeMessageOnDisplay(message1);
  writeMessage2OnDisplay(message2);
  display.display();
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
  write_on_display("Test bereit", "Taster drücken");
}


bool publish_balance_start(int cell_number) {
    return client.publish((String("esp-module/") + module_number + "/cell/" + cell_number + "/balance_request").c_str(), String(balance_time_ms).c_str(), true);
}

bool publish_meas_total_voltage() {
  return client.publish((String("esp-module/") + mac_address + "/set_config").c_str(), String(String(module_number) + ",1,0").c_str(), true);
}

// the loop function runs over and over again forever
void loop() {
  static int balancing_cell_counter = 0;
  static bool test_passed = false;
  Serial.println("Starting loop");

  switch (state) {
    case TestState::idle:
      if(read_switch())
      {
        state = TestState::button_pressed;
      }
      break;
    case TestState::button_pressed:
      write_on_display("Taster gedrückt", "Test startet");
      read_dip_switches();
      digitalWrite(OUT_MOSFET2, HIGH);
      digitalWrite(OUT_LED_GREEN, LOW);
      digitalWrite(OUT_LED_RED, LOW);
      state = TestState::test_conf_slave;
      break;
    case TestState::test_conf_slave:
      write_on_display("Test konfiguration", "über MQTT");
      // configure slave over MQTT based on his published MAC address
      state = TestState::test_cell_voltages_zero;
      break;
    case TestState::test_cell_voltages_zero:
      write_on_display("Test 0 V", "Zellspannungen");
      // check if all cell voltages like expected (0 V)
      state = TestState::activate_cell_voltages;
      break;
    case TestState::activate_cell_voltages:
      write_on_display("Aktiviere", "Zellspannungen");
      digitalWrite(OUT_MOSFET1, HIGH);
      state = TestState::test_cell_voltages_real;
      break;
    case TestState::test_cell_voltages_real:
      write_on_display("Test echte", "Zellspannungen");
      // check if all cell voltage are like expected (total voltage / number_of_cells)
      state = TestState::test_cell_balancing;
      break;
    case TestState::test_cell_balancing:
      if(balancing_cell_counter < number_of_cells)
      {
        publish_balance_start(++balancing_cell_counter);
        write_on_display("Test balancing", "Zelle " + String(balancing_cell_counter));
        // wait balance_time_ms
        //check if cell voltage, which is balanced, drops to approx. 1,11V (resistor divider)
      }
      else
      {
        balancing_cell_counter = 0;
        state = TestState::test_aux_voltage;
      }
      break;
    case TestState::test_aux_voltage:
      write_on_display("Test ADC", "Gesamtspannung");
      publish_meas_total_voltage();
      // check if total voltage is expected value (Voltage of test supply)
      state = TestState::test_temp_voltages;
      break;
    case TestState::test_temp_voltages:
      write_on_display("Test Temperatursensoren", "Gesamtspannung");
      state = TestState::test_finished;
      break;
    case TestState::test_finished:
      if(test_passed)
      {
        digitalWrite(OUT_LED_GREEN, HIGH);
        write_on_display("Test", "PASSED");
      }
      else
      {
        digitalWrite(OUT_LED_RED, HIGH);
        write_on_display("Test", "FAILED!");
      }

      digitalWrite(OUT_MOSFET1, LOW);
      digitalWrite(OUT_MOSFET2, LOW);
      state = TestState::idle;
      break;

    default:
      Serial.println("Unknown Test State");
      state = TestState::idle;
      break;
  }

}


void testregime()
{
//  wait_for_button();

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
 
}
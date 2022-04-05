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

#include "credentials.h"

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

bool skip_zero_voltages_test = false;

struct DipSwitch {
  uint8_t DIP1 : 1;
  uint8_t DIP2 : 1;
  uint8_t DIP3 : 1;
};

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

String availability_topic = "easybms-test-adapter/available";
String log_topic = "easybms-test-adapter/log";

WiFiClient wifiClient;
PubSubClient client(mqtt_server, mqtt_port, wifiClient);

unsigned long timer1;
unsigned long voltages_timer;

String module_number = "1";
String mac_address = "undefined";
auto balance_time_ms = 5000;
unsigned int number_of_cells = 12;
bool config_successfull = false;


float voltages[12];
float total_sys_volt;
float module_temps[2];

void read_all_buttons();
String split(String s, char parser, int index);
void testregime();
bool timerPassed();
void setTimer(long int timedelay);

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
    if (client.connect(mqtt_server, mqtt_username, mqtt_password, availability_topic.c_str(), 0, true, "offline"))
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(availability_topic.c_str(), "online", true);

      client.subscribe("esp-module/+/available");
      //client.subscribe("esp-module/testroute/available");

      // Routes to subscribe
      client.subscribe(String("esp-module/" + module_number + "/available").c_str());
      client.subscribe(String("esp-module/" + module_number + "/uptime").c_str());
      client.subscribe(String("esp-module/" + module_number + "/module_voltage").c_str());
      client.subscribe(String("esp-module/" + module_number + "/module_temps").c_str());
      client.subscribe(String("esp-module/" + module_number + "/chip_temp").c_str());
      client.subscribe(String("esp-module/" + module_number + "/timediff").c_str());
      client.subscribe(String("esp-module/" + module_number + "/total_system_voltage").c_str());

      for(unsigned int cell_number = 0; cell_number < number_of_cells; cell_number++) {
        client.subscribe(String("esp-module/" + module_number + "/cell/" + cell_number + "/is_balancing").c_str());
        client.subscribe(String("esp-module/" + module_number + "/cell/" + cell_number + "/voltage").c_str());
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

void log(const char* s) {
  Serial.println(s);
  reconnect_mqtt();
  client.publish(log_topic.c_str(), s, false);
}

void log(String s) {
  log(s.c_str());
}

void mqtt_callback(char *topic, byte *payload, unsigned int length) {
  String topic_string = String(topic);
  String payload_string;
  payload_string.concat((char *) payload, length);
  log("Callback came with topic: ");
  log(topic);
  if (topic_string.endsWith("available"))
  {
    String parsed  = topic_string.substring(topic_string.indexOf("/") + 1, topic_string.lastIndexOf("/"));

    if (parsed.length() < 3 && parsed == module_number)
    {
      config_successfull=true;
    }  
    else
    {
      mac_address = parsed;
    } 
  }
  else if(topic_string.endsWith("module_voltage"))
  {

  }
  else if(topic_string.endsWith("module_temps"))
  {
    module_temps[0] = split(topic_string, ',', 0).toFloat();
    module_temps[1] = split(topic_string, ',', 1).toFloat();
  }
   else if(topic_string.endsWith("chip_temp"))
  {

  }
  else if(topic_string.endsWith("total_system_voltage"))
  {
    total_sys_volt = payload_string.toFloat();
  }
  else if(topic_string.endsWith("is_balancing"))
  {

  }
  else if(topic_string.endsWith("voltage"))
  {
    int cell_nr  = split(topic_string, '/', 3).toInt();

    voltages[cell_nr - 1] = payload_string.toFloat();
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

String split(String s, char parser, int index) {
  String rs="";
  int parserCnt=0;
  int rFromIndex=0, rToIndex=-1;
  while (index >= parserCnt) {
    rFromIndex = rToIndex+1;
    rToIndex = s.indexOf(parser,rFromIndex);
    if (index == parserCnt) {
      if (rToIndex == 0 || rToIndex == -1) return "";
      return s.substring(rFromIndex,rToIndex);
    } else parserCnt++;
  }
  return rs;
}

void write_on_display(String line1, String line2 = "") {
  //Clear the buffer
  display.clearDisplay();

  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(0, 2);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.println(line1);

  if(line2 != "")
  {
    //display.setCursor(0, 20);     // Start at top-left corner
    display.println(line2);
  }

  display.display();
}

void write_meas_on_display(String desc, float* arr, size_t nr)
{
  //Clear the buffer
  display.clearDisplay();

  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(0, 2);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.println(desc);

  for(unsigned int i = 0; i < nr; i++)
  {
    display.print(arr[i], 3);
    display.print("V,");
  }

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
  setup_wifi(ssid, password);
  reconnect_mqtt();

  client.setCallback(mqtt_callback);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) 
  { // Address 0x3D for 128x64
    log(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  
  // Start the broker
  write_on_display("Setup abgeschlossen");
  delay(2000);                       // wait for 2 seconds       
  log("Finished Setup");
}


bool publish_balance_start(int cell_number) {
    reconnect_mqtt();
    return client.publish(String("esp-module/" + module_number + "/cell/" + cell_number + "/balance_request").c_str(), String(balance_time_ms).c_str(), true);
}

bool publish_slave_config() {
  reconnect_mqtt();
  return client.publish(String("esp-module/" + mac_address + "/set_config").c_str(), String(module_number + ",1,0").c_str(), true);   // Slave konfiguriert mit Modulnummer 1 und als Gesamtspannungsmesser
}

void reset_measurements() {
  for(unsigned int i = 0; i < number_of_cells; i++)
  {
    voltages[i] = NAN;
  }

  module_temps[0] = NAN;
  module_temps[1] = NAN;
  total_sys_volt = NAN;
}

// the loop function runs over and over again forever
void loop() {
  static TestState state;

  static unsigned int balancing_cell_counter = 0;
  static bool test_passed = true;
  static bool button_pressed = false;
  reconnect_mqtt();
  client.loop();

  if(read_switch())
  {
    button_pressed = true;
  }

  if(!timerPassed())
    return;

  setTimer(2000);   // 2 seconds minimum loop time for every step

  switch (state) {
    case TestState::idle:
      write_on_display("Test bereit", "Taster druecken");
      if(button_pressed)
      {
        state = TestState::button_pressed;
        log("TestState:button pressed");
      }
      break;
    case TestState::button_pressed:
      write_on_display("Taster gedrueckt", "Test startet");
      log("button pressed, going on to test_conf_slave");
      read_dip_switches();
      digitalWrite(OUT_MOSFET2, HIGH);
      digitalWrite(OUT_LED_GREEN, LOW);
      digitalWrite(OUT_LED_RED, LOW);
      state = TestState::test_conf_slave;
      log("TestState::test_conf_slave");
      break;
    case TestState::test_conf_slave:
      if(mac_address == "undefined")
      {  
        //Serial.println("No slave available");
        break;
      }
      publish_slave_config();
      write_on_display("Test konfiguration", "ueber MQTT");
      
      if(config_successfull)
      {
        reset_measurements();
        state = TestState::test_cell_voltages_zero;
        log("TestState::test_cell_voltages_zero");
        set_voltages_timer(10000);
      }       
        
      break;
    case TestState::test_cell_voltages_zero:

      if(skip_zero_voltages_test) {
        state = TestState::activate_cell_voltages;
        log("Skip 0V Test: Test disabled");
        log("TestState::activate_cell_voltages");
        break;
      }

      write_meas_on_display("Test 0 V Zellspannungen: ", voltages, number_of_cells);

      // check if all cell voltages like expected (0 V)
      for (unsigned int i = 0; i < number_of_cells; i++)
      {
        if (voltages[i] == NAN) {
          // Not all values have arrived yet.
          if(voltages_timeouted()) {
            test_passed = false;
            state = TestState::test_finished;
            log("TestState::test_finished");
            log("Test failed: Timeout while waiting for voltages from BMS slave.");
          }

          break;
        }

        if (voltages[i] > 0.1)
        {
          test_passed = false;
          state = TestState::test_finished;
          log("TestState::test_finished");
          break;
        }
      }

      // Go to the next test if successful
      state = TestState::activate_cell_voltages;
      log("TestState::activate_cell_voltages");
      break;
    case TestState::activate_cell_voltages:
      reset_measurements();
      write_on_display("Aktiviere", "Zellspannungen");
      digitalWrite(OUT_MOSFET1, HIGH);
      setTimer(5000);
      set_voltages_timer(10000);
      state = TestState::test_cell_voltages_real;
      log("TestState::test_cell_voltages_real");
      break;
    case TestState::test_cell_voltages_real:
      write_meas_on_display("Test echte Zellspannungen", voltages, number_of_cells);
      // check if all cell voltage are like expected (total voltage / number_of_cells)
      for (unsigned int i = 0; i < number_of_cells; i++)
      {
        if (voltages[i] == NAN) {
          // Not all voltages are ready yet.

          if(voltages_timeouted()) {
            test_passed = false;
            state = TestState::test_finished;
            log("TestState::test_finished");
            log("Test failed: Timeout while waiting for voltages from BMS slave.");
          }

          break;
        }

        if (voltages[i] < 3.9 || voltages[i] > 4.1)
        {
          test_passed = false;
          state = TestState::test_finished;
          log("Cell voltage: " + String(voltages[i]));
          log("Test was not passed: Cell voltage is not between 3.9 and 4.1");
          log("TestState::test_finished");
          break;
        }
      }
      
      // Go to balancing test if successful
      state = TestState::test_cell_balancing;
      log("TestState::test_cell_balancing");
      break;
    case TestState::test_cell_balancing:
      if(balancing_cell_counter != 0)
      {
        //check if cell voltage, which is balanced, drops to approx. 1,11V (resistor divider)
        auto cell_voltage = voltages[balancing_cell_counter - 1];
        if (cell_voltage < 1.0 || cell_voltage > 1.2)
        {
          test_passed = false;
          state = TestState::test_finished;
          log("Balancing cell voltage: " + String(cell_voltage));
          log("Test was not passed: Cell voltage did not drop to between 1.0 and 1.2");
          log("TestState::test_finished");
          break;
        }
      }

      write_meas_on_display("Test balancing Zelle " + String(balancing_cell_counter), voltages, number_of_cells);

      if(balancing_cell_counter < number_of_cells)
      {
        publish_balance_start(++balancing_cell_counter);
        setTimer(5000);
      }
      else
      {
        balancing_cell_counter = 0;
        state = TestState::test_aux_voltage;
        log("TestState::test_aux_voltage");
      }
      
      break;
    case TestState::test_aux_voltage:
      write_meas_on_display("Test ADC Gesamtspannung", &total_sys_volt, 1);
      // check if total voltage is expected value (Voltage of test supply)
      state = TestState::test_temp_voltages;
      log("TestState::test_temp_voltages");

      if ((total_sys_volt < 47.5) || total_sys_volt > 48.5)
      {
        test_passed = false;
        state = TestState::test_finished;
        log("TestState::test_finished");
      }

      break;
    case TestState::test_temp_voltages:
      write_meas_on_display("Test Temperatursensoren", module_temps, 2);
      state = TestState::test_finished;
      log("TestState::test_finished");

      log("Module Temp 0: " + String(module_temps[0]));
      log("Module Temp 0: " + String(module_temps[1]));

      if (module_temps[0] < 24 || module_temps[0] > 26)
      {
        test_passed = false;
        log("Test was not passed: Module Temp 0 out of expected range.");
      }
      if (module_temps[1] < 24 || module_temps[1] > 26)
      {
        test_passed = false;
        log("Test was not passed: Module Temp 1 out of expected range.");
      }


      break;
    case TestState::test_finished:
      if(test_passed)
      {
        digitalWrite(OUT_LED_GREEN, HIGH);
        write_on_display("Test", "PASSED");
        log("Test passed");
      }
      else
      {
        digitalWrite(OUT_LED_RED, HIGH);
        write_on_display("Test", "FAILED!");
        log("Test failed");
      }

      digitalWrite(OUT_MOSFET1, LOW);
      digitalWrite(OUT_MOSFET2, LOW);
      mac_address = "undefined";
      button_pressed = false;
      state = TestState::idle;
      break;

    default:
      log("Unknown Test State");
      button_pressed = false;
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
  timer1 = millis() + timedelay;
}

bool timerPassed()
{
  return millis() > timer1;
}

void set_voltages_timer(long timeout_time) {
  voltages_timer = millis() + timeout_time;
}

bool voltages_timeouted() {
  return millis() > voltages_timer;
}
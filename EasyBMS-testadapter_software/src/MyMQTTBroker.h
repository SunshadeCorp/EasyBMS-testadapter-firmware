#include "Arduino.h"
#include "uMQTTBroker.h"

/*
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

class MyMQTTBroker: public uMQTTBroker
{
public:
    virtual bool onConnect(IPAddress addr, uint16_t client_count) {
      Serial.println(addr.toString()+" connected");
      return true;
    }

    virtual void onDisconnect(IPAddress addr, String client_id) {
      Serial.println(addr.toString()+" ("+client_id+") disconnected");
    }

    virtual bool onAuth(String username, String password, String client_id) {
      Serial.println("Username/Password/ClientId: "+username+"/"+password+"/"+client_id);
      return true;
    }
    
    virtual void onData(String topic, const char *data, uint32_t length) {
      char data_str[length+1];
      os_memcpy(data_str, data, length);
      data_str[length] = '\0';
      
      Serial.println("received topic '"+topic+"' with data '"+(String)data_str+"'");
      //printClients();
    }

    // Sample for the usage of the client info methods

    virtual void printClients() {
      for (int i = 0; i < getClientCount(); i++) {
        IPAddress addr;
        String client_id;
         
        getClientAddr(i, addr);
        getClientId(i, client_id);
        Serial.println("Client "+client_id+" on addr: "+addr.toString());
      }
    }
};
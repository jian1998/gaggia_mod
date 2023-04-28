
//==========================================
//
// select "LOLIN(WEMOS) D1 R2 & mini" device    
//
//==========================================

//*********************************************
// My network file defines MY_SSID, MY_PASSORD,
// MY_MQTTSERVER_ADDRESS and MY_TIMEZONE.
#include "netconfig.h"       // it is not checked in... 
//*********************************************

// define temperature sensor used (choose one)
//#define TPM36
#define LM135

// uncomment the following line if no WiFi needed
#define NO_WIFI

#ifndef NO_WIFI
#include <ESP8266WiFi.h>
#include <PubSubClient.h>       // MQTT clienrt
#include <ArduinoOTA.h>             // OTA
#endif
#include <Wire.h>                   // I2C Library
#include <time.h>
#include <Adafruit_GFX.h>           // for LED display
#include <Adafruit_SSD1306.h>       // for LED display


#define BUILT_IN_LED    D4
#define ESP_SCL         D5          // LED display
#define ESP_SDA         D6          // LED display
#define ALARM           D8           // alarm if its in steam mode for too long
#define PUMP_ON         D3          // front panel swith to start brew


const char* DEVICE_NAME = "Gaggia";  // device name.
const char* FW_REV = "1.1";         // firmware revision

const float steam_temp = 119.;                // above 119 degree C is considered in steam mode.
const int steam_temp_before_alarm = 600000;   // sound alarm if temperatuer > steam_temp for over 10 min. 
const long last_brew_linger_time_ms = 5000;   // the brew time will stay for 5 sec after brew switch is turned off.

//long lastMsg = 0;
long lastLEDUpdate = 0;
long lastMQTTUpdate = 0;

bool isOn = true;
float temp = 0;
float celsius = 0;
bool isPumpOn = 0;
bool isSteamOn = 0;
long SteamOn_time = 0;
bool alarmOn = true;
float last_brew_time = 0.;

long pumpStartTime_ms = 0;
long pumpStopTime_ms = 0;
Adafruit_SSD1306 display(128,64,&Wire); 



#ifndef NO_WIFI
// Update these with values suitable for your network.
const char* ssid = MY_SSID;                       // SSID of your WiFi
const char* password = MY_PASSORD;                // Your WiFi password
const char* mqtt_server = MY_MQTTSERVER_ADDRESS;  // MQTT brooker address

// See https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv for Timezone codes for your region
// for example: "EST5EDT,M3.2.0,M11.1.0" NY/USA time.
const char* my_timezone = MY_TIMEZONE;    
WiFiClient espClient;
PubSubClient client(espClient);


void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  WiFi.mode(WIFI_STA);   // to disable the access point mode

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  ArduinoOTA.setHostname(DEVICE_NAME);
  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(DEVICE_NAME)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

#endif

void display_opening_screen()
{
  Wire.begin(ESP_SDA, ESP_SCL); /* join i2c bus with SDA=D1 and SCL=D2 of NodeMCU */
  Wire.setClock(100000);  // i2c scl clock freq (1MHz)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  

  // Clear the buffer.
  display.clearDisplay();
 
  // Display Text
  display.setTextSize(1);
  display.setTextColor(WHITE);  
  display.setCursor(2,2);
  display.println("");
  display.println("   Gaggia Classic ");
  display.println("");
  display.println("         Pro");
  display.println("");
  display.println("       FW: " + String(FW_REV));

  display.display();
}

void update_display(float currentTemp, float brewTimeSec)
{
  char sTemp[9];
  char sBrewTime[9];
  dtostrf(currentTemp, 6, 1, sTemp);

  // Clear the buffer.
  display.clearDisplay();
 
  // Display Text
  display.setTextSize(1);
  display.setCursor(1,1);
  //display.println(" ");
  if(brewTimeSec > 0)
  {
    dtostrf(brewTimeSec, 6, 1, sBrewTime);
    display.println("  Brew Time:");
    display.println(" ");
    display.setTextSize(3);
    display.println(sBrewTime);
    display.setTextSize(1);
    display.println("");
    display.println("");
    display.println("  Current Temp:" + String(sTemp));
  }
  else
  {
    //display.println("  Current Temp:");
    display.println(" ");
    display.println(" ");
    display.setTextSize(3);
    display.println(sTemp);
    display.setTextSize(1);
    display.println("");
    display.println("");
#ifndef NO_WIFI
    time_t now;
    time(&now);
    char time_output[30];
    // See http://www.cplusplus.com/reference/ctime/strftime/ for strftime functions
    strftime(time_output, 30, "%a %D %T", localtime(&now)); 
    display.println(time_output);
#endif
  }
  display.display();

  
}

float averageTemp(float currentTemp)
{
  const float alpha = .2;
  static float avgTmp = 20;
  avgTmp = avgTmp + alpha *(currentTemp - avgTmp);
  return avgTmp;
}

void setup(void)
{
  // start serial port
  Serial.begin(115200);
  display_opening_screen();
  
  pinMode(BUILT_IN_LED, OUTPUT);   // built-in LED
  pinMode(ALARM, OUTPUT);
 
  digitalWrite(ALARM, 0);      
#ifndef NO_WIFI
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
 
  setenv("TZ", my_timezone, 1); 
#endif
}

void loop(void)
{
#ifndef NO_WIFI
  ArduinoOTA.handle();

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
#endif
  delay(200);
  long now = millis();

  int rawvoltage = analogRead(A0);
  rawvoltage = analogRead(A0);
#ifdef LM135
//LM135 ouptup go through 200K ohm + 220K(R1) 100K(R2) D1 mini buit-in res. 
 float millivolts = (rawvoltage / 1024.0) * 5000; 
  Serial.print(millivolts);
  Serial.println(" mV");
 //celsius = (millivolts - 500.)/10.;
  celsius = (millivolts)/10. - 273.15;
#endif
#ifdef  TPM36
  float millivolts = (rawvoltage / 1024.0) * 3050; 
  Serial.print(millivolts);
  Serial.println(" mV");
  celsius = (millivolts - 500.)/10.;
#endif
  float avgTemp = averageTemp(celsius);

  if (now - lastLEDUpdate > 1000) 
  {
    //client.publish("ha/temperature/ec155", String(avgTemp).c_str(), true);
    lastLEDUpdate = now;
    isOn = !isOn;
    digitalWrite(BUILT_IN_LED, isOn);

    if(isPumpOn)
    {
      last_brew_time = (millis() - pumpStartTime_ms)/1000.0;
      update_display(avgTemp, last_brew_time );
    }
    else
    {
      if(last_brew_time > 0 && (millis() - pumpStopTime_ms) > last_brew_linger_time_ms) 
        last_brew_time = 0;
      update_display(avgTemp, last_brew_time);      
    }
    
  }
#ifndef NO_WIFI
  if (now - lastMQTTUpdate > 30000) 
  {
    lastMQTTUpdate = now;
    client.publish("ha/temperature/gaggia", String(avgTemp).c_str(), false);
  }
#endif
  if(avgTemp > steam_temp)
  {
    if(!isSteamOn)
    {
      SteamOn_time = now;
      isSteamOn = true;
    }
    else
    {
      //------------------------------------------------------
      // if it is in steam temperature for too long, set alarm. 
      //------------------------------------------------------
      if(now - SteamOn_time > steam_temp_before_alarm)   
      {
        digitalWrite(ALARM, alarmOn);
        alarmOn = !alarmOn;
      }
    }
  }
  else
  {
    if(isSteamOn)
    {
      isSteamOn = false;
      digitalWrite(ALARM, 0);

    }
  }

     
  if(digitalRead(PUMP_ON) == LOW) 
  {
    if(!isPumpOn)
    {
      isPumpOn = true;
      pumpStartTime_ms = millis();
      isSteamOn = false;
      digitalWrite(ALARM, 0);
    }
  }
  else
  {
    if(isPumpOn)
    {
      isPumpOn = false;
      pumpStopTime_ms = millis();
    }
  }



}

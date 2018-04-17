#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <Arduino.h>
#include <WiFiClient.h>
#include <ESP32WebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>

#include "time.h"

#include "DFRobotDFPlayerMini.h"

// Preferences is used to store the value in flash
Preferences preferences;

HardwareSerial mySoftwareSerial(1);
DFRobotDFPlayerMini myDFPlayer;
void printDetail(uint8_t type, int value);

int freq = 5000;
int resolution = 10;

uint8_t redChannel = 0;
uint8_t greenChannel = 1;
uint8_t blueChannel = 2;
uint8_t coolChannel = 3;
uint8_t warmChannel = 4;

const uint8_t red = 25;
const uint8_t green = 26;
const uint8_t blue = 27;
const uint8_t cool = 14;
const uint8_t warm = 4;

uint32_t warmVal, coolVal, redVal, greenVal, blueVal = 0;

uint32_t warmNewVal, coolNewVal, redNewVal, greenNewVal, blueNewVal = 0;

bool profile = false;
bool up = false;

bool profFlag = false;
bool profWait = false;

uint32_t profWaitTimerVal = 0;
uint32_t profWaitTimerStrt = 0;

uint32_t profDelayTime = 2000;
uint32_t profDelayCount = 0;

const int fan = 21;
const int murata1 = 18;
const int murata2 = 19;

String cart1Name = "Menthol";
String cart2Name = "ClO2";

bool cart1Timer = false;
bool cart2Timer = false;

uint32_t cart1SprayTime = 10000;
uint32_t cart1SprayStartTime = 0;
uint32_t cart1WaitTime = 50000;
uint32_t cart1WaitStartTime = 0;

uint32_t cart2SprayTime = 10000;
uint32_t cart2SprayStartTime = 0;
uint32_t cart2WaitTime = 50000;
uint32_t cart2WaitStartTime = 0;

const short int BUILTIN_LED1 = 13;

ESP32WebServer HTTP(80);

const char *ssid = "Kenzen_2G";
const char *password = "goodlife";
const char *ntp_server = "pool.ntp.org";
const float hourOffset = 2;

bool scentTimerFlag = true;
bool clo2TimerFlag = true;

uint8_t lightInc = 0;

void setup()
{
  mySoftwareSerial.begin(9600, SERIAL_8N1, 16, 17); // speed, type, RX, TX
  Serial.begin(115200);

  pinMode(BUILTIN_LED1, OUTPUT); // Initialize the BUILTIN_LED1 pin as an output

  ledcSetup(redChannel, freq, resolution);
  ledcAttachPin(red, redChannel);
  ledcSetup(greenChannel, freq, resolution);
  ledcAttachPin(green, greenChannel);
  ledcSetup(blueChannel, freq, resolution);
  ledcAttachPin(blue, blueChannel);
  ledcSetup(coolChannel, freq, resolution);
  ledcAttachPin(cool, coolChannel);
  ledcSetup(warmChannel, freq, resolution);
  ledcAttachPin(warm, warmChannel);

  pinMode(fan, OUTPUT);
  pinMode(murata1, OUTPUT);
  pinMode(murata2, OUTPUT);

  // Open preference to get the stored values
  preferences.begin("balmyStorage", false);

  //Store the values in the respective variables
  cart1Name = preferences.getString("cartOne", "cart1");
  cart2Name = preferences.getString("cartTwo", "cart2");
  warmNewVal = preferences.getInt("warm", warmVal);
  coolNewVal = preferences.getInt("cool", coolVal);
  redNewVal = preferences.getInt("red", redVal);
  greenNewVal = preferences.getInt("green", greenVal);
  blueNewVal = preferences.getInt("blue", blueVal);
  cart1SprayTime = preferences.getInt("cart1Spray", cart1SprayTime);
  cart1WaitTime = preferences.getInt("cart1Wait", cart1WaitTime);
  cart2SprayTime = preferences.getInt("cart2Spray", cart2SprayTime);
  cart2WaitTime = preferences.getInt("cart2Wait", cart2WaitTime);
  cart1Timer = preferences.getInt("cart1Timer", cart1Timer);
  cart2Timer = preferences.getInt("cart2Timer", cart2Timer);

  preferences.end();

  digitalWrite(BUILTIN_LED1, HIGH); // Turn the LED On by making the voltage HIGH
  // start light profiling to set the light
  coolNewVal = 100;
  profFlag = true;
  profDelayCount = 0;

  digitalWrite(fan, LOW);
  digitalWrite(murata1, LOW);
  digitalWrite(murata2, LOW);

  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid, password);

  WiFi.setHostname("SquarePanel1");

  Serial.print("Connecting to WIFI");
  int t = 0;

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    t = t + 1;
    if (t >= 10)
    {
      ESP.restart();
    }
    delay(500);
  }

  delay(200);

  digitalWrite(BUILTIN_LED1, LOW); //off

  if (WiFi.waitForConnectResult() == WL_CONNECTED)
  {
    configTime(hourOffset * 3600, 3600, ntp_server);

    Serial.printf("Starting HTTP...\n");

    //---------------------------GET Methods -------------------------------

    // Info of the device
    HTTP.on("/info", HTTP_GET, []() {
      String deviceInfo = "{";
      deviceInfo += "\"warmWhite\":" + String(warmVal);
      deviceInfo += ", \"coolWhite\":" + String(coolVal);
      deviceInfo += ", \"red\":" + String(redVal);
      deviceInfo += ", \"green\":" + String(greenVal);
      deviceInfo += ", \"blue\":" + String(blueVal);
      deviceInfo += ", \"cart1State\":" + String(digitalRead(murata1));
      deviceInfo += ", \"cart2State\":" + String(digitalRead(murata2));
      deviceInfo += ", \"cart1Name\":" + cart1Name;
      deviceInfo += ", \"cart2Name\":" + cart2Name;
      deviceInfo += ", \"cart1TimerState\":" + cart1Timer;
      deviceInfo += ", \"cart2TimerState\":" + cart2Timer;
      deviceInfo += ", \"cart1SprayTime\":" + String(cart1SprayTime);
      deviceInfo += ", \"cart1WaitTime\":" + String(cart1WaitTime);
      deviceInfo += ", \"cart2SprayTime\":" + String(cart2SprayTime);
      deviceInfo += ", \"cart2WaitTime\":" + String(cart2WaitTime);
      deviceInfo += "}";
      HTTP.send(200, "text/plain", deviceInfo);
    });

    //---------------------------PUT Methods -------------------------------

    //Cartridge naming HTTP
    HTTP.on("/cartridge/name", HTTP_PUT, []() {
      StaticJsonBuffer<200> jsonBuffer;
      preferences.begin("balmyStorage", false);
      JsonObject &root = jsonBuffer.parseObject(HTTP.arg("plain"));
      String cart1 = root["cartridge1"].as<String>();
      if (cart1 != "")
      {
        cart1Name = cart1;
        preferences.putString("cartOne", cart1);
      }
      String cart2 = root["cartridge2"].as<String>();
      if (cart2 != "")
      {
        cart2Name = cart2;
        preferences.putString("cartTwo", cart2);
      }
      preferences.end();
      HTTP.sendHeader("Access-Control-Allow-Origin", "*");
      HTTP.send(200, "text/json", "{\"reply\":\"Success\"}");
    });

    HTTP.on("/cartridge1/set/time", HTTP_PUT, []() {
      StaticJsonBuffer<200> jsonBuffer;
      JsonObject &root = jsonBuffer.parseObject(HTTP.arg("plain"));
      preferences.begin("balmyStorage", false);
      cart1SprayTime = root["spray"].as<uint32_t>();
      preferences.putInt("cart1Spray", cart1SprayTime);
      cart1WaitTime = root["wait"].as<uint32_t>();
      preferences.putInt("cart1Wait", cart1WaitTime);
      preferences.end();
      HTTP.sendHeader("Access-Control-Allow-Origin", "*");
      HTTP.send(200, "text/json", "{\"reply\":\"Success\"}");
    });

    HTTP.on("/cartridge2/set/time", HTTP_PUT, []() {
      StaticJsonBuffer<200> jsonBuffer;
      JsonObject &root = jsonBuffer.parseObject(HTTP.arg("plain"));
      preferences.begin("balmyStorage", false);
      cart2SprayTime = root["spray"].as<uint32_t>();
      preferences.putInt("cart2Spray", cart2SprayTime);
      cart2WaitTime = root["wait"].as<uint32_t>();
      preferences.putInt("cart2Wait", cart2WaitTime);
      preferences.end();
      HTTP.sendHeader("Access-Control-Allow-Origin", "*");
      HTTP.send(200, "text/json", "{\"reply\":\"Success\"}");
    });

    //---------------------------POST Methods -------------------------------

    //Turn on Cartridge 1
    HTTP.on("/cartridge1/on", HTTP_POST, []() {
      digitalWrite(murata1, HIGH);
      digitalWrite(fan, HIGH);
      HTTP.sendHeader("Access-Control-Allow-Origin", "*");
      HTTP.send(200, "text/json", "{\"reply\":\"Success\"}");
    });

    //Turn off Cartridge 1
    HTTP.on("/cartridge1/off", HTTP_POST, []() {
      digitalWrite(murata1, LOW);
      digitalWrite(fan, LOW);
      HTTP.sendHeader("Access-Control-Allow-Origin", "*");
      HTTP.send(200, "text/json", "{\"reply\":\"Success\"}");
    });

    //Turn on Cartridge 2
    HTTP.on("/cartridge2/on", HTTP_POST, []() {
      digitalWrite(murata2, HIGH);
      digitalWrite(fan, HIGH);
      HTTP.sendHeader("Access-Control-Allow-Origin", "*");
      HTTP.send(200, "text/json", "{\"reply\":\"Success\"}");
    });

    //Turn off Cartridge 2
    HTTP.on("/cartridge2/off", HTTP_POST, []() {
      digitalWrite(murata2, LOW);
      digitalWrite(fan, LOW);
      HTTP.sendHeader("Access-Control-Allow-Origin", "*");
      HTTP.send(200, "text/json", "{\"reply\":\"Success\"}");
    });

    //Turn on Cartridge 1 Timer
    HTTP.on("/cartridge1/timer/on", HTTP_POST, []() {
      preferences.begin("balmyStorage", false);
      cart1Timer = true;
      preferences.putInt("cart1Timer", cart1Timer);
      preferences.end();
      HTTP.sendHeader("Access-Control-Allow-Origin", "*");
      HTTP.send(200, "text/json", "{\"reply\":\"Success\"}");
    });

    //Turn off Cartridge 1 Timer
    HTTP.on("/cartridge1/timer/off", HTTP_POST, []() {
      preferences.begin("balmyStorage", false);
      cart1Timer = false;
      preferences.putInt("cart1Timer", cart1Timer);
      preferences.end();
      digitalWrite(fan, LOW);
      digitalWrite(murata1, LOW);
      HTTP.sendHeader("Access-Control-Allow-Origin", "*");
      HTTP.send(200, "text/json", "{\"reply\":\"Success\"}");
    });

    //Turn on Cartridge 2 Timer
    HTTP.on("/cartridge2/timer/on", HTTP_POST, []() {
      preferences.begin("balmyStorage", false);
      cart2Timer = true;
      preferences.putInt("cart2Timer", cart2Timer);
      preferences.end();
      HTTP.sendHeader("Access-Control-Allow-Origin", "*");
      HTTP.send(200, "text/json", "{\"reply\":\"Success\"}");
    });

    //Turn off Cartridge 2 Timer
    HTTP.on("/cartridge2/timer/off", HTTP_POST, []() {
      preferences.begin("balmyStorage", false);
      cart2Timer = false;
      digitalWrite(fan, LOW);
      digitalWrite(murata2, LOW);
      preferences.putInt("cart2Timer", cart2Timer);
      preferences.end();
      HTTP.sendHeader("Access-Control-Allow-Origin", "*");
      HTTP.send(200, "text/json", "{\"reply\":\"Success\"}");
    });

    HTTP.on("/rgb", HTTP_POST, []() {
      StaticJsonBuffer<200> jsonBuffer;
      JsonObject &root = jsonBuffer.parseObject(HTTP.arg("plain"));

      if (!root.success())
      {
        Serial.println("parseObject() failed");
        return false;
      }
      //Parse value from body
      warmNewVal = root["warmWhite"].as<uint32_t>();
      coolNewVal = root["coolWhite"].as<uint32_t>();
      redNewVal = root["red"].as<uint32_t>();
      greenNewVal = root["green"].as<uint32_t>();
      blueNewVal = root["blue"].as<uint32_t>();

      profFlag = true;
      profDelayCount = 0;

      preferences.begin("balmyStorage", false);
      preferences.putInt("warm", warmNewVal);
      preferences.putInt("cool", coolNewVal);
      preferences.putInt("red", redNewVal);
      preferences.putInt("green", greenNewVal);
      preferences.putInt("blue", blueNewVal);
      preferences.end();

      HTTP.send(200, "application/json", "{\"response\":\"success\"}");
    });

    HTTP.on("/volume/up", HTTP_POST, []() {
      myDFPlayer.volumeUp(); //Volume Down
      Serial.print("Volume is : ");
      Serial.println(myDFPlayer.readVolume());
      HTTP.send(200, "application/json", "{\"response\":\"success\"}");
    });

    HTTP.on("/volume/down", HTTP_POST, []() {
      myDFPlayer.volumeDown(); //Volume Down
      Serial.print("Volume is : ");
      Serial.println(myDFPlayer.readVolume());
      HTTP.send(200, "application/json", "{\"response\":\"success\"}");
    });

    HTTP.on("/volume", HTTP_POST, []() {
      StaticJsonBuffer<200> jsonBuffer;
      JsonObject &root = jsonBuffer.parseObject(HTTP.arg("plain"));

      if (!root.success())
      {
        Serial.println("parseObject() failed");
        return false;
      }

      int vol = root["Volume"].as<int>();
      myDFPlayer.volume(vol); //Set volume value (0~30)
      Serial.print("Volume is : ");
      Serial.println(myDFPlayer.readVolume());
      HTTP.send(200, "application/json", "{\"response\":\"success\"}");
    });

    HTTP.on("/musicOn", HTTP_POST, []() {
      Serial.println("music on");
      myDFPlayer.next();
      HTTP.send(200, "application/json", "{\"response\":\"success\"}");
    });

    HTTP.on("/pause", HTTP_POST, []() {
      Serial.println("music off");
      myDFPlayer.pause();
      HTTP.send(200, "application/json", "{\"response\":\"success\"}");
    });

    HTTP.on("/musicOff", HTTP_POST, []() {
      Serial.println("music off");
      myDFPlayer.stop();
      HTTP.send(200, "application/json", "{\"response\":\"success\"}");
    });

    HTTP.on("/next", HTTP_POST, []() {
      myDFPlayer.next();
      Serial.print(F("Playing File Number: "));
      Serial.println(myDFPlayer.readCurrentFileNumber());
      HTTP.send(200, "application/json", "{\"response\":\"success\"}");
    });

    HTTP.on("/previous", HTTP_POST, []() {
      myDFPlayer.previous();
      Serial.print(F("Playing File Number: "));
      Serial.println(myDFPlayer.readCurrentFileNumber());
      HTTP.send(200, "application/json", "{\"response\":\"success\"}");
    });
    HTTP.begin();
  }
  else
  {
    Serial.printf("WiFi Failed\n");
    while (1)
      delay(100);
  }

  Serial.printf("Ready!\n");
  Serial.println(WiFi.localIP());
  Serial.println();
  Serial.println(F("DFRobot DFPlayer Mini Demo"));
  Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));

  int t1 = 0;

  while (!myDFPlayer.begin(mySoftwareSerial))
  { //Use softwareSerial to communicate with mp3.

    t1 = t1 + 1;
    Serial.println(t1);
    if (t1 >= 10)
    {
      Serial.println(myDFPlayer.readType(), HEX);
      Serial.println(F("Unable to begin:"));
      ESP.restart();
    }
    delay(500);
  }
  Serial.println(F("DFPlayer Mini online."));

  myDFPlayer.setTimeOut(500); //Set serial communictaion time out 500ms

  //----Set volume----
  myDFPlayer.volume(10); //Set volume value (0~30).
  //  myDFPlayer.volumeUp(); //Volume Up
  //  myDFPlayer.volumeDown(); //Volume Down

  //----Set different EQ----
  myDFPlayer.EQ(DFPLAYER_EQ_NORMAL);

  //----Set device we use SD as default----
  //  myDFPlayer.outputDevice(DFPLAYER_DEVICE_U_DISK);
  myDFPlayer.outputDevice(DFPLAYER_DEVICE_SD);

  int delayms = 100;

  myDFPlayer.enableLoopAll();


  //----Read imformation----

//  Serial.println(F("readVolume--------------------"));
//  Serial.println(myDFPlayer.readVolume()); //read current volume
//  Serial.println(F("readFileCounts--------------------"));
//  Serial.println(myDFPlayer.readFileCounts()); //read all file counts in SD card
//  Serial.println(F("readCurrentFileNumber--------------------"));
//  Serial.println(myDFPlayer.readCurrentFileNumber()); //read current play file number
//  Serial.println(F("Readfoldercounts--------------------"));
//  Serial.println(myDFPlayer.readFolderCounts()); //read current play file number
//  Serial.println(F("readFileCountsInFolder--------------------"));
//  Serial.println(myDFPlayer.readFileCountsInFolder('MP3')); //read fill counts in folder SD:/03
//  Serial.println(F("--------------------"));
//  Serial.println(coolNewVal > coolVal);
//  Serial.println(redVal > redNewVal);
//  Serial.println(greenVal > greenNewVal);
//  Serial.println(blueVal > blueNewVal);
//  Serial.println("-------------------------");
}

void loop()
{

  HTTP.handleClient();

  // If the wifi disconnects unexpectedly, try to reconnect
  if (WiFi.status() != WL_CONNECTED)
  {
    ESP.restart();
  }

  //Light Profiling section (whenever there is a change in light, it changes smoothly)
  // Space that does the witing if a profile is schedules after a delay
  if (profWait)
  {
    if (profWaitTimerVal > profWaitTimerStrt)
    {
      profFlag = false;
    }
    else
    {
      profFlag = true;
      profWait = false;
      profWaitTimerStrt += 1;
    }
  }

  //Main loop which does the profiling
  if (profFlag)
  {
    // LightInc is used to achieve 2 seconds delay for changing light values
    if (profDelayCount < profDelayTime)
    {
      if (lightInc > 0)
      {
        lightInc = 0;
        profDelayCount += 1;
        if (warmNewVal > warmVal)
        {
          warmVal += 1;
          ledcWrite(warmChannel, warmVal);
        }
        else if (warmNewVal < warmVal)
        {
          warmVal -= 1;
          ledcWrite(warmChannel, warmVal);
        }
        if (coolNewVal > coolVal)
        {
          coolVal += 1;
          ledcWrite(coolChannel, coolVal);
        }
        else if (coolNewVal < coolVal)
        {
          coolVal -= 1;
          ledcWrite(coolChannel, coolVal);
        }
        if (redNewVal > redVal)
        {
          redVal += 1;
          ledcWrite(redChannel, redVal);
        }
        else if (redNewVal < redVal)
        {
          redVal -= 1;
          ledcWrite(redChannel, redVal);
        }
        if (greenNewVal > greenVal)
        {
          greenVal += 1;
          ledcWrite(greenChannel, greenVal);
        }
        else if (greenNewVal < greenVal)
        {
          greenVal -= 1;
          ledcWrite(greenChannel, greenVal);
        }
        if (blueNewVal > blueVal)
        {
          blueVal += 1;
          ledcWrite(blueChannel, blueVal);
        }
        else if (blueNewVal < blueVal)
        {
          blueVal -= 1;
          ledcWrite(blueChannel, blueVal);
        }
      }
      else
      {
        lightInc += 1;
      }
    }
    else
    {
      profFlag = false;
      profDelayCount = 0;
    }
  }

  if (cart1Timer || cart2Timer)
  {
    // Check if the murata timers are On and also their scent spray times are less and then enable the murata and blower
    if (cart1Timer && cart1SprayStartTime < cart1SprayTime)
    {
      digitalWrite(murata1, HIGH);
      digitalWrite(fan, HIGH);
      cart1WaitStartTime = cart1WaitTime;
      cart1SprayStartTime += 1;
      if (cart1SprayStartTime == cart1SprayTime)
      {
        digitalWrite(murata1, LOW);
        digitalWrite(fan, LOW);
        cart1WaitStartTime = 0;
      }
    }
    if (cart2Timer && cart2SprayStartTime < cart2SprayTime)
    {
      digitalWrite(murata2, HIGH);
      digitalWrite(fan, HIGH);
      cart2WaitStartTime = cart2WaitTime;
      cart2SprayStartTime += 1;
      if (cart2SprayStartTime == cart2SprayTime)
      {
        digitalWrite(murata2, LOW);
        digitalWrite(fan, LOW);
        cart2WaitStartTime = 0;
      }
    }

    // Repeat for wait
    if (cart1Timer && cart1WaitStartTime < cart1WaitTime)
    {
      digitalWrite(murata1, LOW);
      // This is in the case if both the scources should work parallely
      if (!cart2Timer)
      {
        digitalWrite(fan, LOW);
      }
      cart1SprayStartTime = cart1SprayTime;
      cart1WaitStartTime += 1;
      if (cart1WaitStartTime == cart1WaitTime)
      {
        digitalWrite(murata1, HIGH);
        digitalWrite(fan, HIGH);
        cart1SprayStartTime = 0;
      }
    }
    if (cart2Timer && cart2WaitStartTime < cart2WaitTime)
    {
      digitalWrite(murata2, LOW);
      // This is in the case if both the scources should work parallely
      if (!cart1Timer)
      {
        digitalWrite(fan, LOW);
      }
      cart2SprayStartTime = cart2SprayTime;
      cart2WaitStartTime += 1;
      if (cart2WaitStartTime == cart2WaitTime)
      {
        digitalWrite(murata2, HIGH);
        digitalWrite(fan, HIGH);
        cart2SprayStartTime = 0;
      }
    }
  }

  if (!cart1Timer)
  {
    cart1SprayStartTime = 0;
    cart1WaitStartTime = 0;
  }

  if (!cart2Timer)
  {
    cart2SprayStartTime = 0;
    cart2WaitStartTime = 0;
  }

  struct tm timeinfo;

  // get the local time for scheduling
  getLocalTime(&timeinfo);
  // Sceduling the scent and Clo2 on every week day
  /*if (timeinfo.tm_wday > 0 && timeinfo.tm_wday < 6)
  {
    // scent is sceduled from 7am to 5 pm
    // Scent On time scendule
    if (timeinfo.tm_hour == 7 && timeinfo.tm_min == 0 && scentTimerFlag)
    {
      scentTimerFlag = false;
      cart1Timer = true;
      cart2Timer = false;
      preferences.begin("balmyStorage", false);
      preferences.putInt("cart1Timer", cart1Timer);
      preferences.putInt("cart2Timer", cart2Timer);
      preferences.end();
    }
    //Scent Off time scedule
    if (timeinfo.tm_hour == 17 && timeinfo.tm_min == 0 && !scentTimerFlag)
    {
      scentTimerFlag = true;
      cart1Timer = false;
      cart2Timer = false;
      preferences.begin("balmyStorage", false);
      preferences.putInt("cart1Timer", cart1Timer);
      preferences.putInt("cart2Timer", cart2Timer);
      preferences.end();
      digitalWrite(fan, LOW);
      digitalWrite(murata1, LOW);
      digitalWrite(murata2, LOW);
    }

    // Clo2 is sceduled from 11pm to 5 am
    // Scent On time scendule
    if (timeinfo.tm_hour == 23 && timeinfo.tm_min == 0 && clo2TimerFlag)
    {
      clo2TimerFlag = false;
      cart1Timer = false;
      cart2Timer = true;
      preferences.begin("balmyStorage", false);
      preferences.putInt("cart1Timer", cart1Timer);
      preferences.putInt("cart2Timer", cart2Timer);
      preferences.end();
    }
    //Scent Off time scedule
    if (timeinfo.tm_hour == 5 && timeinfo.tm_min == 0 && !clo2TimerFlag)
    {
      clo2TimerFlag = true;
      cart1Timer = false;
      cart2Timer = false;
      preferences.begin("balmyStorage", false);
      preferences.putInt("cart1Timer", cart1Timer);
      preferences.putInt("cart2Timer", cart2Timer);
      preferences.end();
      digitalWrite(fan, LOW);
      digitalWrite(murata1, LOW);
      digitalWrite(murata2, LOW);
    }
  }*/
  delay(1);
}

void printDetail(uint8_t type, int value)
{
  switch (type)
  {
  case TimeOut:
    Serial.println(F("Time Out!"));
    break;
  case WrongStack:
    Serial.println(F("Stack Wrong!"));
    break;
  case DFPlayerCardInserted:
    Serial.println(F("Card Inserted!"));
    break;
  case DFPlayerCardRemoved:
    Serial.println(F("Card Removed!"));
    break;
  case DFPlayerCardOnline:
    Serial.println(F("Card Online!"));
    break;
  case DFPlayerPlayFinished:
    Serial.print(F("Number:"));
    Serial.print(value);
    Serial.println(F(" Play Finished!"));
    break;
  case DFPlayerError:
    Serial.print(F("DFPlayerError:"));
    switch (value)
    {
    case Busy:
      Serial.println(F("Card not found"));
      break;
    case Sleeping:
      Serial.println(F("Sleeping"));
      break;
    case SerialWrongStack:
      Serial.println(F("Get Wrong Stack"));
      break;
    case CheckSumNotMatch:
      Serial.println(F("Check Sum Not Match"));
      break;
    case FileIndexOut:
      Serial.println(F("File Index Out of Bound"));
      break;
    case FileMismatch:
      Serial.println(F("Cannot Find File"));
      break;
    case Advertise:
      Serial.println(F("In Advertise"));
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
}

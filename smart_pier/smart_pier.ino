/*################################# An example to connect thingcontro.io MQTT over TLS 1.2 ###############################
   Using thingcontrol board V 1.0 read ambient temperature and humidity values from an FS200-SHT 1X sensor via I2C send to
   thingcontrol.io by MQTT over TLS 1.2 via WiFi (WiFi Manager)

   JSN-SR04T Ultrasonic Range Finder Ping Mode Test

   Exercises the ultrasonic range finder module and prints out measured distance
   5V - connect to 5V
   GND - connect to ground
   RX/TRIG - connect to digital pin 12.  Can be any digital pin
   TX/ECHO - connect to digital pin 13.  Can be any digital pin
  ########################################################################################################################*/
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <Wire.h>

String version_ = "0.0.1";

#define WIFI_AP ""
#define WIFI_PASSWORD ""

int contentLength = 0;
bool isValidContentType = false;

String deviceToken = "Y05gCljgop4awBh8w6FD";

String host = "thingcontrol.io"; // Host => hosted firmware.bin
String host_ = "raw.githubusercontent.com"; // Host => hosted firmware.bin
String bin = "greenioiot/ultrasonic/main/ultrasonic.ino.bin"; // bin file name with a slash in front.
String vers = "/api/v1/" + deviceToken + "/attributes?clientKeys=version"; // bin file name with a slash in front.

int ver_compare = 0;

char thingsboardServer[] = "mqtt.thingcontrol.io";

String json = "";

unsigned long startMillis;  //some global variables available anywhere in the program
unsigned long startTeleMillis;
unsigned long starSendTeletMillis;
unsigned long currentMillis;
unsigned long dayMillis;
const unsigned long periodSendTelemetry = 10000;  //the value is a number of milliseconds

WiFiClientSecure wifiClient;
PubSubClient client(wifiClient);

int status = WL_IDLE_STATUS;
int PORT = 8883;

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

uint64_t espChipID = ESP.getEfuseMac();

String mac2String(byte ar[]) {
  String s;
  for (byte i = 0; i < 6; ++i)
  {
    char buf[3];
    sprintf(buf, "%02X", ar[i]); // J-M-L: slight modification, added the 0 in the format for padding 
    s += buf;
    if (i < 5) s += ':';
  }
  return s;
}

String getHeaderValue(String header, String headerName) {
  return header.substring(strlen(headerName.c_str()));
}

void execOTA() {
    Serial.println("Connecting to: " + String(host_));
    if (wifiClient.connect(host_.c_str(), 443)) {
      // Connection Succeed.
      // Fecthing the bin

      Serial.println("Fetching Bin: " + String(bin));
  
      // Get the contents of the bin file
      wifiClient.print(String("GET ") + bin + " HTTP/1.1\r\n" +
                   "Host: " + host_ + "\r\n" +
                   "Cache-Control: no-cache\r\n" +
                   "Connection: close\r\n\r\n");
  
      //     Check what is being sent
      Serial.print(String("GET ") + bin + " HTTP/1.1\r\n" +
                   "Host: " + host_ + "\r\n" +
                   "Cache-Control: no-cache\r\n" +
                   "Connection: close\r\n\r\n");
  
      unsigned long timeout = millis();
      while (wifiClient.available() == 0) {
        if (millis() - timeout > 5000) {
          Serial.println("Client Timeout !");
          wifiClient.stop();
          return;
        }
      }
  
      while (wifiClient.available()) {
        // read line till /n
        String line = wifiClient.readStringUntil('\n');
        Serial.println(line);
        // remove space, to check if the line is end of headers
        line.trim();
  
        // if the the line is empty,
        // this is end of headers
        // break the while and feed the
        // remaining `client` to the
        // Update.writeStream();
        if (!line.length()) {
          //headers ended
          break; // and get the OTA started
        }
  
        // Check if the HTTP Response is 200
        // else break and Exit Update
        if (line.startsWith("HTTP/1.1")) {
          if (line.indexOf("200") < 0) {
            Serial.println("Got a non 200 status code from server. Exiting OTA Update.");
            break;
          }
        }
  
        // extract headers here
        // Start with content length
        if (line.startsWith("Content-Length: ")) {
          contentLength = atoi((getHeaderValue(line, "Content-Length: ")).c_str());
          Serial.println("Got " + String(contentLength) + " bytes from server");
        }
  
        // Next, the content type
        if (line.startsWith("Content-Type: ")) {
          String contentType = getHeaderValue(line, "Content-Type: ");
          Serial.println("Got " + contentType + " payload.");
          if (contentType == "application/octet-stream") {
            isValidContentType = true;
          }
        }
      }
    } else {
      // Connect to Thingcontrol.io failed
      // May be try?
      // Probably a choppy network?
      Serial.println("Connection to " + String(host) + " failed. Please check your setup");
      // retry??
      // execOTA();
    }
  
    // Check what is the contentLength and if content type is `application/octet-stream`
    Serial.println("contentLength : " + String(contentLength) + ", isValidContentType : " + String(isValidContentType));
  
    // check contentLength and content type
    if (contentLength && isValidContentType) {
      // Check if there is enough to OTA Update
      bool canBegin = Update.begin(contentLength);
  
      // If yes, begin
      if (canBegin) {
        Serial.println("Begin OTA. This may take 2 - 5 mins to complete. Things might be quite for a while.. Patience!");
        // No activity would appear on the Serial monitor
        // So be patient. This may take 2 - 5mins to complete
        size_t written = Update.writeStream(wifiClient);
  
        if (written == contentLength) {
          Serial.println("Written : " + String(written) + " successfully");
        } else {
          Serial.println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?" );
          // retry??
          // execOTA();
        }
  
        if (Update.end()) {
          Serial.println("OTA done!");
          if (Update.isFinished()) {
            Serial.println("Update successfully completed. Rebooting.");
            ESP.restart();
          } else {
            Serial.println("Update not finished? Something went wrong!");
          }
        } else {
          Serial.println("Error Occurred. Error #: " + String(Update.getError()));
        }
      } else {
        // not enough space to begin OTA
        // Understand the partitions and
        // space availability
        Serial.println("Not enough space to begin OTA");
        wifiClient.flush();
      }
    } else {
      Serial.println("There was no content in the response");
      wifiClient.flush();
    }
}

void _initGetFirmware()
{
  wifiClient.setInsecure();
  latestVersion();
  if (ver_compare != 0) {
    execOTA();
  }
}

void latestVersion() {
  
    if (wifiClient.connect(host.c_str(), 443)) {
      wifiClient.print(String("GET ") + vers + " HTTP/1.1\r\n" +
                   "Host: " + host + "\r\n" +
                   "Cache-Control: no-cache\r\n" +
                   "Connection: close\r\n\r\n");

      unsigned long timeout = millis();
      while (wifiClient.available() == 0) {
        if (millis() - timeout > 5000) {
          Serial.println("Client Timeout !");
          wifiClient.stop();
          return;
        }
      }
  
      while (wifiClient.connected()) {
      String line = wifiClient.readStringUntil('\n');
      if (line == "\r") {
        break;
      }
    }
    String check_ver = "";
    while (wifiClient.available()) {
      char c = wifiClient.read();
      check_ver += c;
    }
    check_ver = check_ver.substring(check_ver.indexOf("\"",10)+1,check_ver.lastIndexOf("\""));
    wifiClient.stop();
    bin = "greenioiot/ultrasonic/" + check_ver + "/ultrasonic.ino.bin";
    ver_compare = check_ver.compareTo(version_);
    }
}

struct Water
{
  String distan;
};

Water sensor[2] ;

const int TRIG_PIN = 22;
const int ECHO_PIN = 21;
float temp_In_C = 20.0;  // Can enter actual air temp here for maximum accuracy
float speed_Of_Sound;          // Calculated speed of sound based on air temp
float distance_Per_uSec;      // Distance sound travels in one microsecond

void setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  // Formula to calculate speed of sound in meters/sec based on temp
  speed_Of_Sound = 331.1 + (0.606 * temp_In_C);
  // Calculate the distance that sound travels in one microsecond in centimeters
  distance_Per_uSec = speed_Of_Sound / 10000.0;
  deviceToken = mac2String((byte*) &espChipID);
  Serial.begin(9600); // Open serial connection to report values to host
  Serial.println(F("Starting... Ambient Temperature/Humidity Monitor"));
  Serial.println();
  Serial.println(F("***********************************"));
  Serial.println(deviceToken);
  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);

  if (!wifiManager.autoConnect("@Thingcontrol_AP")) {
    Serial.println("failed to connect and hit timeout");
    delay(1000);
  }
  client.setServer( thingsboardServer, PORT );
  reconnectMqtt();

  Serial.print("Start.....");
  startMillis = millis();  //initial start time
  dayMillis = millis();
}

void loop()
{
  status = WiFi.status();
  if ( status == WL_CONNECTED)
  {
    if ( !client.connected() )
    {
      reconnectMqtt();
    }
    client.loop();
  }

  currentMillis = millis();  //get the current "time" (actually the number of milliseconds since the program started)
  //send
  if (currentMillis - starSendTeletMillis >= periodSendTelemetry)  //test whether the period has elapsed
  {
    read_Sensor() ;
    delay(2000);
    sendtelemetry();
    starSendTeletMillis = currentMillis;  //IMPORTANT to save the start time of the current LED state.
  }
  if (currentMillis - dayMillis >= 86400000)  //test whether the period has elapsed
  {
    _initGetFirmware();
    dayMillis = currentMillis;
  }
}

void setWiFi()
{
  Serial.println("OK");
  Serial.println("+:Reconfig WiFi  Restart...");
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  if (!wifiManager.startConfigPortal("ThingControlCommand"))
  {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    delay(5000);
  }
  Serial.println("OK");
  Serial.println("+:WiFi Connect");
  client.setServer( thingsboardServer, PORT );  // secure port over TLS 1.2
  reconnectMqtt();
}

void processTele(char jsonTele[])
{
  char *aString = jsonTele;
  Serial.println("OK");
  Serial.print(F("+:topic v1/devices/me/ , "));
  Serial.println(aString);
  client.publish( "v1/devices/me/telemetry", aString);
}

void reconnectMqtt()
{
  if ( client.connect("Thingcontrol_AT", deviceToken.c_str(), NULL) )
  {
    Serial.println( F("Connect MQTT Success."));
    client.subscribe("v1/devices/me/rpc/request/+");
  }
}

void read_Sensor()
{
  float duration, distanceCm, distanceIn, distanceFt;
 
  digitalWrite(TRIG_PIN, HIGH);       // Set trigger pin HIGH 
  delayMicroseconds(15);              // Hold pin HIGH for 20 uSec
  digitalWrite(TRIG_PIN, LOW);        // Return trigger pin back to LOW again.
  duration = pulseIn(ECHO_PIN,HIGH);  // Measure time in uSec for echo to come back.
 
  // convert the time data into a distance in centimeters, inches and feet
  duration = duration / 2.0;  // Divide echo time by 2 to get time in one direction
  distanceCm = duration * distance_Per_uSec;
  distanceIn = distanceCm / 2.54;
  distanceFt = distanceIn / 12.0;
   
  if (distanceCm <= 0){
    Serial.println("Out of range");
  }
  else {
    Serial.print(distanceCm, 0);
    Serial.println("cm,  ");
    sensor[0].distan = distanceCm;
  }
}

void sendtelemetry()
{
  String json = "";
  json.concat("{\"distance\":");
  json.concat(sensor[0].distan);
  json.concat("}");
  Serial.println(json);
  // Length (with one extra character for the null terminator)
  int str_len = json.length() + 1;
  // Prepare the character array (the buffer)
  char char_array[str_len];
  // Copy it over
  json.toCharArray(char_array, str_len);
  processTele(char_array);
}

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <DHT.h> 
#include <LiquidCrystal.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#define AD 0
#define AU 1
#define DEG 2
#define HF 3
#if defined(ESP32)
#include <WiFiMulti.h>
WiFiMulti wifiMulti;
#define DEVICE "ESP32"
#elif defined(ESP8266)
#include <ESP8266WiFiMulti.h>
ESP8266WiFiMulti wifiMulti;
#define DEVICE "ESP8266"
#endif
  

// WiFi AP SSID and password
#define WIFI_SSID "Laura"
#define WIFI_PASSWORD "lauraetro"

// pin definition of LEDs
int red = 22;
int yellow = 21;
int green = 18;

// sensor of temperature and humidity
#define DHTPIN 4 
#define DHTTYPE DHT22 
DHT dht(DHTPIN, DHTTYPE);

// definition of LCD pins
LiquidCrystal lcd(32, 33, 25, 26, 27, 14);


// ------------- API information -----------------
// Month requirement information 
const char* host = "api.openweathermap.org";
const int httpsPort = 443;
// API key of OpenWeatherMap
const char* apiKey = "cca9203fa945ed145a22988130f63f70";

// IPInfo.io API key
const char* ipinfo_token =  "f82301089dc437";

// global variables
int monthNum;
float latitudeNum, longitudeNum;

// InfluxDB configuration
#define INFLUXDB_URL "https://us-east-1-1.aws.cloud2.influxdata.com"
#define INFLUXDB_TOKEN "vM0YeqEoap1uk73cYKSsOw_uO1FXYHlEV61I2dZ_KWm_BJ-4iWaW8nc8r6_Zgq_OsyalVg6OQIvP_O1SdVd8kg=="
#define INFLUXDB_ORG "4c716b38fb971ca1"
#define INFLUXDB_BUCKET "Esp32"
  
// Time zone info
#define TZ_INFO "UTC2"
  
// Declare InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
  
// Data points
Point sensor("measurements");

// Function to obtain UNIX timestamps based on geographical coordinates
unsigned long getUnixTime(float lat, float lon);
unsigned long getUnixTime(float lat, float lon) {
  HTTPClient http;
  String url = "http://" + String(host) + "/data/2.5/weather?appid=" + String(apiKey) + "&lat=" + String(lat) + "&lon=" + String(lon) + "&units=metric";
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode > 0) {
    String response = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);
    unsigned long unixTime = doc["dt"];
    return unixTime;
  }
  http.end();
  return 0;
}

//---------- costumed characters for LCD --------------------
byte arrowDown[8] = {
  B00100,
  B00100,
  B00100,
  B00100,
  B00100,
  B11111,
  B01110,
  B00100
};

byte arrowUp[8] = {
  B00100,
  B01110,
  B11111,
  B00100,
  B00100,
  B00100,
  B00100,
  B00100
};

byte degree[8] = {
  B11000,
  B11000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000
};

byte happyFace[8] = {
  B00000,
  B01010,
  B01010,
  B01010,
  B00000,
  B10001,
  B01110,
  B00000
};



void setup() {
  Serial.begin(115200);
  dht.begin(); // inizializza il sensore DHT
  lcd.begin(16, 2);
  lcd.createChar(0, arrowDown);
  lcd.createChar(1, arrowUp);
  lcd.createChar(2, degree);
  lcd.createChar(3, happyFace);
  lcd.clear();
  lcd.autoscroll();

// ------------ WiFi connection -------------------------

  // Setup wifi
    WiFi.mode(WIFI_STA);
    wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
    lcd.setCursor(0, 0);
    lcd.print("Connecting to "+ String(WIFI_SSID));
    Serial.println("Connecting to "+ String(WIFI_SSID));
  
    while (wifiMulti.run() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Connecting...");
      lcd.setCursor(0, 1);
      lcd.print("Connecting...");    
    }
      lcd.clear();
      lcd.noAutoscroll();
      lcd.print("Connection established!");
      Serial.println("Connection established!");
  
    // Accurate time is necessary for certificate validation and writing in batches
    // We use the NTP servers in your area as provided by: https://www.pool.ntp.org/zone/
    // Syncing progress and the time will be printed to Serial.
    timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
  
    // Check server connection
    if (client.validateConnection()) {
      lcd.print("Connected to InfluxDB:");
      Serial.print("Connected to InfluxDB: ");
      Serial.println(client.getServerUrl());
    } else {
      lcd.print("InfluxDB connection failed: ");
      Serial.print("InfluxDB connection failed: ");
      Serial.println(client.getLastErrorMessage());
    }

  // Add tags to the data point
  sensor.addTag("device", DEVICE);
  sensor.addTag("SSID", WiFi.SSID());

// ------------ Inizialization pin as output ----------------------

  // Initialization of pins as outputs
  pinMode(red, OUTPUT);
  pinMode(yellow, OUTPUT);
  pinMode(green, OUTPUT);

// ------------  OpenWeatherMap API connection to extrapolate the current month --------------
  
  HTTPClient http;
  String url = "http://" + String(host) + "/data/2.5/weather?appid=" + String(apiKey) + "&q=" + String(WiFi.localIP()) + "&units=metric";
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode > 0) {
    String response = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);
    float lat = doc["coord"]["lat"];
    float lon = doc["coord"]["lon"];
    
    // Calculating the date on the basis of geographical coordinates
    setTime(getUnixTime(lat, lon));
    Serial.print("Current Month: ");
    monthNum = month(); 
    Serial.println(monthNum);
  } else {
      Serial.println("Error in HTTP request");
    }
  
// ----------- Connection to IpInfo API in order to get latitude and longitude - hemisphere ---------------------

  HTTPClient http2;
    // url to the api  
    String url1 = "https://ipinfo.io/json?token=";
    url1 += ipinfo_token;

    //Make the request to the API
    http2.begin(url1);
    int httpCode2 = http2.GET();

    if (httpCode2 > 0) {
      Serial.println("Response received");

      // Read the API response
      String payload2 = http2.getString();

      // Create a JSON object to parse the API response
      DynamicJsonDocument doc2(1024);
      deserializeJson(doc2, payload2);

      // extrapolate latitude and longitude       
      String latitude = doc2["loc"].as<String>().substring(0, doc2["loc"].as<String>().indexOf(','));
      String longitude = doc2["loc"].as<String>().substring(doc2["loc"].as<String>().indexOf(',') + 1);

      latitudeNum = latitude.toFloat();
      longitudeNum = longitude.toFloat();
    
      Serial.println("Latitude: " + String(latitudeNum));
      Serial.println("Longitude: " + String(longitudeNum));

      } else {
        Serial.println("Error in HTTP request");
    }
  http2.end();  // release API connection 2
  http.end(); // release API connection 1

} // close setup()

// turn all led of evry time the sketch is load
void all_led_off(){
  digitalWrite(red, LOW); 
  digitalWrite(yellow, LOW); 
  digitalWrite(green, LOW); 
}

// turn needed led on 
void led_turn_on(int led){
  digitalWrite(led, HIGH);  
}

// turn needed led off
void led_turn_off(int led){
  digitalWrite(led, LOW); 
}

void loop() {

//----------------- InfluxDB settings + collect data from sensor ------------------
  // Clear fields for reusing the point.
  sensor.clearFields();

  // check and print temperature and humidity
  float temp = dht.readTemperature(); 
  float hum = dht.readHumidity();

  sensor.addField("Temperature",temp);
  sensor.addField("Humidity",hum);

  // Print what are we exactly writing
  Serial.print("Writing on InfluDB: ");
  Serial.println(sensor.toLineProtocol());

// ---------------- LCD ---------------------------

  lcd.clear();
  Serial.print("Temperature: ");
  Serial.print(temp);
  Serial.print(" °C - Humidity: ");
  Serial.print(hum);
  Serial.println(" %");
  lcd.print("T:"+ (String(temp)).substring(0,4) +" C-H:"+ (String(hum)).substring(0,4)+"%");
  lcd.setCursor(6,0);
  lcd.write(byte(DEG));  
  float hmin=30;
  float hmax=60;
  float correction = 1.04;
  float tmax,tmin;
  lcd.setCursor(0, 1);

  all_led_off();

  // check current hemispere and season
  if((monthNum >= 4 && monthNum <= 9) & (latitudeNum  > 0)){
    tmin = 22.2;
    tmax = 26.6; 
  }else{
    tmin = 20;
    tmax = 23.3;
  }
 
 //---------- turn on/off leds according to standards ----------
  //green zone
  if((temp>(tmin*correction))&(temp<(tmax/correction))&(hum>(hmin*correction))&(hum<(hmax/correction))){
    all_led_off();
    led_turn_on(green);
    lcd.setCursor(0,1);
    lcd.print("T:");
    lcd.write(byte(HF));
    lcd.setCursor(9,1);
    lcd.print("H:");
    lcd.write(byte(HF));
    Serial.println("1");
  }else{
    //Yellow area at top center
    if((temp>(tmin*correction))&(temp<(tmax/correction))&(hum>(hmax/correction))&(hum<(hmax*correction))){
      all_led_off();
      led_turn_on(yellow);
      lcd.setCursor(0,1);
      lcd.print("T:");
      lcd.write(byte(HF));
      lcd.setCursor(9,1);
      lcd.print("H:");
      lcd.write(byte(AD));
      Serial.println("2");
    }else{
      //Yellow area in the lower center
      if((temp>(tmin*correction))&(temp<(tmax/correction))&(hum>(hmin/correction))&(hum<(hmin*correction))){
        all_led_off();
        led_turn_on(yellow);
        lcd.setCursor(0,1);
        lcd.print("T:");
        lcd.write(byte(HF));
        lcd.setCursor(9,1);
        lcd.print("H:");
        lcd.write(byte(AU));
        Serial.println("3");
      }else{
        //Central right yellow area
        if((temp>(tmax/correction))&(temp<(tmax*correction))&(hum>(hmin*correction))&(hum<(hmax/correction))){
          all_led_off();
          led_turn_on(yellow);
          lcd.setCursor(0,1);
          lcd.print("T:");
          lcd.write(byte(AD));
          lcd.setCursor(9,1);
          lcd.print("H:");
          lcd.write(byte(HF));
          Serial.println("4");
        }else{
          //Central left yellow area
          if((temp>(tmin/correction))&(temp<(tmin*correction))&(hum>(hmin*correction))&(hum<(hmax/correction))){
            all_led_off();
            led_turn_on(yellow);
            lcd.setCursor(0,1);
            lcd.print("T:");
            lcd.write(byte(AU));
            lcd.setCursor(9,1);
            lcd.print("H:");
            lcd.write(byte(HF));
            Serial.println("5");
          }else{
            //Upper right yellow area
            if((temp>(tmax/correction))&(temp<(tmax*correction))&(hum>(hmax/correction))&(hum<(hmax*correction))){
                all_led_off();
                led_turn_on(yellow);
                lcd.setCursor(0,1);
                lcd.print("T:");
                lcd.write(byte(AD));
                lcd.setCursor(9,1);
                lcd.print("H:");
                lcd.write(byte(AD));
                Serial.println("6");
              }else{
                  //Yellow area Top left
                  if((temp<(tmin*correction))&(temp>(tmin/correction))&(hum>(hmax/correction))&(hum<(hmax*correction))){
                    all_led_off();
                    led_turn_on(yellow);
                    lcd.setCursor(0,1);
                    lcd.print("T:");
                    lcd.write(byte(AU));
                    lcd.setCursor(9,1);
                    lcd.print("H:");
                    lcd.write(byte(AD));
                    Serial.println("7");
                  }else{
                      //Lower right yellow area
                      if((temp>(tmax/correction))&(temp<(tmax*correction))&(hum>(hmin/correction))&(hum<(hmin*correction))){
                        all_led_off();
                        led_turn_on(yellow);
                        lcd.setCursor(0,1);
                        lcd.print("T:");
                        lcd.write(byte(AD));
                        lcd.setCursor(9,1);
                        lcd.print("H:");
                        lcd.write(byte(AU));
                        Serial.println("8");
                      }else{
                          //Lower right yellow area
                          if((temp<(tmin*correction))&(temp>(tmin/correction))&(hum>(hmin/correction))&(hum<(hmin*correction))){
                            all_led_off();
                            led_turn_on(yellow);
                            lcd.setCursor(0,1);
                            lcd.print("T:");
                            lcd.write(byte(AU));
                            lcd.setCursor(9,1);
                            lcd.print("H:");
                            lcd.write(byte(AU));
                            Serial.println("9");
                          }else{
                              //I now consider the red areas of the led
                              //Central high red
                              if((hum>(hmax*correction))&(temp>(tmin*correction))&(temp<(tmax/correction))){
                                all_led_off();
                                led_turn_on(red);
                                lcd.setCursor(0,1);
                                lcd.print("T:");
                                lcd.write(byte(HF));
                                lcd.setCursor(9,1);
                                lcd.print("H:");
                                lcd.write(byte(AD));
                                Serial.println("10");
                              }else{
                                //Central low red
                                if((hum<(hmin/correction))&(temp>(tmin*correction))&(temp<(tmax/correction))){
                                  all_led_off();
                                  led_turn_on(red);
                                  lcd.setCursor(0,1);
                                  lcd.print("T:");
                                  lcd.write(byte(HF));
                                  lcd.setCursor(9,1);
                                  lcd.print("H:");
                                  lcd.write(byte(AU));
                                  Serial.println("11");
                                }else{
                                  //Central right red
                                  if((temp>(tmax*correction))&(hum>(hmin*correction))&(hum<(hmax/correction))){
                                    all_led_off();
                                    led_turn_on(red);
                                    lcd.setCursor(0,1);
                                    lcd.print("T:");
                                    lcd.write(byte(AD));
                                    lcd.setCursor(9,1);
                                    lcd.print("H:");
                                    lcd.write(byte(HF));
                                    Serial.println("12");
                                  }else{
                                    //Central left red
                                    if((temp<(tmin/correction))&(hum>(hmin*correction))&(hum<(hmax/correction))){
                                      all_led_off();
                                      led_turn_on(red);
                                      lcd.setCursor(0,1);
                                      lcd.print("T:");
                                      lcd.write(byte(AU));
                                      lcd.setCursor(9,1);
                                      lcd.print("H:");
                                      lcd.write(byte(HF));
                                      Serial.println("13");
                                    }else{
                                      //Red top right
                                      if((temp>(tmax/correction))&(hum>(hmax/correction))){
                                        all_led_off();
                                        led_turn_on(red);
                                        lcd.setCursor(0,1);
                                        lcd.print("T:");
                                        lcd.write(byte(AD));
                                        lcd.setCursor(9,1);
                                        lcd.print("H:");
                                        lcd.write(byte(AD));
                                        Serial.println("14");
                                      }else{
                                      //Red top left
                                      if((temp<(tmin*correction))&(hum>(hmax/correction))){
                                        all_led_off();
                                        led_turn_on(red);
                                        lcd.setCursor(0,1);
                                        lcd.print("T:");
                                        lcd.write(byte(AU));
                                        lcd.setCursor(9,1);
                                        lcd.print("H:");
                                        lcd.write(byte(AD));
                                        Serial.println("15");
                                      }else{
                                      //Red bottom right
                                      if((temp>(tmax/correction))&(hum<(hmin*correction))){
                                        all_led_off();
                                        led_turn_on(red);
                                        lcd.setCursor(0,1);
                                        lcd.print("T:");
                                        lcd.write(byte(AD));
                                        lcd.setCursor(9,1);
                                        lcd.print("H:");
                                        lcd.write(byte(AU));
                                        Serial.println("16");
                                      }else{
                                        //Red lower left
                                        if((temp<(tmin*correction))&(hum<(hmin*correction))){
                                        all_led_off();
                                        led_turn_on(red);
                                        lcd.setCursor(0,1);
                                        lcd.print("T:");
                                        lcd.write(byte(AU));
                                        lcd.setCursor(9,1);
                                        lcd.print("H:");
                                        lcd.write(byte(AU));
                                        Serial.println("17");     
                                        }else{
                                          Serial.println("error");
                                          lcd.clear();
                                          lcd.setCursor(0, 0);
                                          lcd.noAutoscroll();
                                          lcd.print("ERROR!");
                                          //ERROR, I'm not in any area of the chart
                                        }
                                      }
                                      }
                                    }
                                  }
                                }  
                              }
                            }
                          }
                        }
                      }
                    } 
                  }
                }
    }
  }
  }
  // Check WiFi connection and reconnect if needed
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
  }
    // Write point
  if (!client.writePoint(sensor)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
    Serial.println("Waiting 5 second");
  }
// wait 15 sec to get another measurement
delay(15000);     
    
} 

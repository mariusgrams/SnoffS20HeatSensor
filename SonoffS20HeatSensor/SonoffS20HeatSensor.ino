#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <EEPROM.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .dht-labels{
      font-size: 1.5rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
   input { font-size: 2.0rem; }
  </style>
</head>
<body>
  <h2>Wohnmobil Heizung</h2>
  <p>
    <span class="dht-labels">Aktuelle Temperatur: </span><br> 
    <span id="temperature">%TEMPERATURE%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <form action="/get">
      <span class="dht-labels">Schalten bei: </span><br>
      <span class="dht-labels">
        <input pattern="^\d*(\.\d{0,2})?$" name="input1" value="%AUTO_TEMP%" size="25">
        <input type="submit" value="Speichern">
      </span>
    </form>
  </p>
</body>
<script>
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("temperature").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/temperature", true);
  xhttp.send();
}, 1000);
</script>
</html>)rawliteral";

//Const values
const int PIN_RELAY = 12;
const int PIN_LED = 13;
char *WIFI_SSD  = "Wohnmobil_Heizung"; 
char *WIFI_PW  = "Schalke04!"; 
const unsigned long INTERVAL = 1000UL;
unsigned long previousMillis = 0UL;
IPAddress local_IP(192,168,4,22);
IPAddress gateway(192,168,4,9);
IPAddress subnet(255,255,255,0);
const char* PARAM_INPUT_1 = "input1";
#define EEPROM_SIZE 12

//Member variables
AsyncWebServer server(80);
float temperatur = 0.0;
float autoTemperatur = 22.0;

void setup() {
  Serial.begin(9600); 
  pinMode(PIN_RELAY, OUTPUT);
  setupWifi();
  setupWebserver();
  EEPROM.begin(EEPROM_SIZE);
  autoTemperatur = readAutoTempValue();
}

void setupWifi() {
  Serial.print("Setting soft-AP configuration ... ");
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");

  Serial.print("Setting soft-AP ... ");
  boolean result = WiFi.softAP(WIFI_SSD, WIFI_PW);
  if(result == true)
  {
    Serial.println("Ready");
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Failed!");
  }
  setLedEnabled(false);
}

void setupWebserver() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(temperatur).c_str());
  });

  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    String inputParam;

    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputParam = PARAM_INPUT_1;
      Serial.print("Get Input from key: ");
      Serial.print(inputParam);
      Serial.print(" Value: ");
      Serial.println(inputMessage);
      autoTemperatur = inputMessage.toFloat();
      writeAutoTempValue(autoTemperatur);
    }
    else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    
    request->send_P(200, "text/html", index_html, processor);
  });
 
  server.begin();
}

void loop() {
  readTemperatureValue();
}

void setLedEnabled(bool isOn) {
  //Info on = low
  if (isOn) {
    digitalWrite(PIN_LED, LOW);
  } else {
    digitalWrite(PIN_LED, HIGH);
  } 
}

/**
 * Set Relay and red status LED
 */
void setRelayEnabled(bool isOn) {
    if (isOn) {
    digitalWrite(PIN_RELAY, HIGH);
  } else {
    digitalWrite(PIN_RELAY, LOW);
  } 
}

// Replaces placeholder with DHT values
String processor(const String& var){
  //Serial.println(var);
  if(var == "TEMPERATURE"){
    return String(temperatur);
  } 

  if(var == "AUTO_TEMP"){
    return String(autoTemperatur);
  } 
  return String();
}

void readTemperatureValue() {
  unsigned long currentMillis = millis();

  if(currentMillis - previousMillis > INTERVAL)
  {
    temperatur = random(100,200);
    Serial.print("Temp: ");
    Serial.println(temperatur);
    if (temperatur >= 150) {
      setLedEnabled(true);
      setRelayEnabled(true);
    } else {
      setLedEnabled(false);
      setRelayEnabled(false);
    }
    previousMillis = currentMillis;
  }
}

float readAutoTempValue() {
  float autoTemp;
  EEPROM.get(0, autoTemp);
  Serial.print("AutoTemp value from EPROM: ");
  Serial.println(autoTemp);
  return autoTemp;
}

void writeAutoTempValue(float tempValue) {
  EEPROM.put(0, tempValue);
  EEPROM.commit();
}

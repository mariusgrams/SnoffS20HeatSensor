#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <EEPROM.h>
#include <DHT22.h>

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
const int PIN_TEMP_SENSOR = 2;
char *WIFI_SSD  = "Wohnmobil_Heizung"; 
char *WIFI_PW  = "Schalke04!"; 
const unsigned long INTERVAL = 1000UL;
IPAddress local_IP(192,168,4,22);
IPAddress gateway(192,168,4,9);
IPAddress subnet(255,255,255,0);
const char* PARAM_INPUT_1 = "input1";
#define EEPROM_SIZE 12

//Member variables
AsyncWebServer server(80);
DHT22 dht22(PIN_TEMP_SENSOR);
float temperatur = 0.0;
float switchTemperature = 22.0;
unsigned long lastSwitchedMillis = 0UL;

void setup()
{
  Serial.begin(9600); 
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  setupWifi();
  setupWebserver();
  EEPROM.begin(EEPROM_SIZE);
  switchTemperature = readSwitchTempValue();
  Serial.println("Setup done");
}

void setupWifi()
{
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

void setupWebserver()
{
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
      switchTemperature = inputMessage.toFloat();
      writeSwitchTempValue(switchTemperature);
    }
    else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    
    request->send_P(200, "text/html", index_html, processor);
  });
 
  server.begin();
}

void loop()
{
  checkCurrentTemperature();
}

/**
 * Set the hardware LED to on or off.
 * @param isOn true to turn the LED on, otherwise off.
 */
void setLedEnabled(bool isOn)
{
  //Info on = low
  if (isOn) {
    digitalWrite(PIN_LED, LOW);
  } else {
    digitalWrite(PIN_LED, HIGH);
  } 
}

/**
 * Set the hardware relay value on or off.
 * @param isOn true to turn on the socket/relay, otherwise off.
 */
void setRelayEnabled(bool isOn)
{
  if (isOn) {
    digitalWrite(PIN_RELAY, HIGH);
  } else {
    digitalWrite(PIN_RELAY, LOW);
  } 
}

/**
 * This function is called from the webserver to replace the placeholder with an actual value.
 * @param var placeholder that should be replaced.
 * @return the actual value.
 */
String processor(const String& var){
  if(var == "TEMPERATURE"){
    return String(temperatur);
  } 

  if(var == "AUTO_TEMP"){
    return String(switchTemperature);
  } 
  return String();
}

/**
 * Main logic to control the socket, depending on the current temperature.
 */
void checkCurrentTemperature()
{
  const unsigned long currentMillis = millis();

  if(currentMillis - lastSwitchedMillis > INTERVAL)
  {
    temperatur = dht22.getTemperature();
    Serial.print("Temp: ");
    Serial.println(temperatur);
    if (temperatur <= switchTemperature)
    {
      setLedEnabled(true);
      setRelayEnabled(true);
    }
    else
    {
      setLedEnabled(false);
      setRelayEnabled(false);
    }
    lastSwitchedMillis = currentMillis;
  }
}

/**
 * Read value from EPROM, when the temperatur should be switched.
 */
float readSwitchTempValue()
{
  float switchTemperature;
  EEPROM.get(0, switchTemperature);
  Serial.print("Switch temperature value from EPROM: ");
  Serial.println(switchTemperature);
  return switchTemperature;
}

/**
 * Write value to EPROM, when the temperatur should be switched.
 */
void writeSwitchTempValue(float tempValue)
{
  EEPROM.put(0, tempValue);
  EEPROM.commit();
}

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Servo.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const char* ssid = "OPPO";
const char* password = "b82iwjnr";

const char* http_username = "admin";
const char* http_password = "admin";

const char* PARAM_INPUT_1 = "state";
const char* PARAM_INPUT_2 = "rgb";

const int servoPin = 4; 
const int pirPin = 14; 
const int dhtPin = 12; 
const int redPin = 13; 
const int greenPin = 15; 
const int bluePin = 5; 
const int buzzerPin = 0; 
const int lcdAddress = 0x27;

Servo doorServo;
DHT dht(dhtPin, DHT11);
LiquidCrystal_I2C lcd(lcdAddress, 16, 2);

AsyncWebServer server(80);

bool isAuthenticated = false;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>Home Automation</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {font-family: New Times Roman; display: inline-block; text-align: center;}
    h2 {font-size: 1.8rem;}
    body {max-width: 600px; margin:0px auto; padding-bottom: 10px;}
    .switch {position: relative; display: inline-block; width: 120px; height: 68px} 
    .switch input {display: none}
    .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color:  #FF0000; border-radius: 34px}
    .slider:before {position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #212420; -webkit-transition: .4s; transition: .4s; border-radius: 68px}
    input:checked+.slider {background-color: #44f321}
    input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}
  </style>
</head>
<body>
  <h2>Home Automation</h2>
  <p>Door State: <span id="state">%STATE%</span></p>
  <p>RGB Light:</p>
  <button onclick="sendRGB('r')">Red</button>
  <button onclick="sendRGB('g')">Green</button>
  <button onclick="sendRGB('b')">Blue</button>
  <button onclick="sendRGB('off')">Off</button>
  <br><br>
  <button onclick="logoutButton()">Logout</button>
  %BUTTONPLACEHOLDER%
<script>
function sendRGB(color) {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/update_rgb?rgb=" + color, true);
  xhr.send();
}

function toggleCheckbox(element) {
  var xhr = new XMLHttpRequest();
  if(element.checked){ 
    xhr.open("GET", "/update?state=1", true); 
    document.getElementById("state").innerHTML = "OPEN";  
  }
  else { 
    xhr.open("GET", "/update?state=0", true); 
    document.getElementById("state").innerHTML = "CLOSED";      
  }
  xhr.send();
}

function logoutButton() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/logout", true);
  xhr.send();
  setTimeout(function(){ window.open("/logged-out","_self"); }, 1000);
}
</script>
</body>
</html>
)rawliteral";

const char logout_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
  <p>Logged out or <a href="/">return to homepage</a>.</p>
  <p><strong>Note:</strong> close all web browser tabs to complete the logout process.</p>
</body>
</html>
)rawliteral";

String processor(const String& var){
  if(var == "BUTTONPLACEHOLDER"){
    String buttons ="";
    String outputStateValue = outputState();
    buttons+= "<p><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"output\" " + outputStateValue + "><span class=\"slider\"></span></label></p>";
    return buttons;
  }
  if (var == "STATE"){
    if(doorServo.read() == 90){
      return "OPEN";
    }
    else {
      return "CLOSED";
    }
  }
  return String();
}

String outputState(){
  if(doorServo.read() == 90){
    return "checked";
  }
  else {
    return "";
  }
  return "";
}

void setup(){
  Serial.begin(115200);

  doorServo.attach(servoPin);
  doorServo.write(0);

  dht.begin();

  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");

  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(pirPin, INPUT);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    isAuthenticated = true;
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request){
    isAuthenticated = false;
    request->send(401);
  });

  server.on("/logged-out", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", logout_html, processor);
  });

  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    String inputMessage;
    String inputParam;
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputParam = PARAM_INPUT_1;
      if(inputMessage == "1" && digitalRead(pirPin) == HIGH) {
        doorServo.write(90);
        float temperature = dht.readTemperature();
        lcd.setCursor(0, 0);
        lcd.print("Temp: " + String(temperature) + " C");
        lcd.setCursor(0, 1);
        lcd.print("Welcome Home");
      } else {
        doorServo.write(0);
      }
    } else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    Serial.println(inputMessage);
    request->send(200, "text/plain", "OK");
  });

  server.on("/update_rgb", HTTP_GET, [](AsyncWebServerRequest *request) {
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    String color;
    if (request->hasParam(PARAM_INPUT_2)) {
      color = request->getParam(PARAM_INPUT_2)->value();
      if(color == "r") {
        analogWrite(redPin, 255);
        analogWrite(greenPin, 0);
        analogWrite(bluePin, 0);
      } else if(color == "g") {
        analogWrite(redPin, 0);
        analogWrite(greenPin, 255);
        analogWrite(bluePin, 0);
      } else if(color == "b") {
        analogWrite(redPin, 0);
        analogWrite(greenPin, 0);
        analogWrite(bluePin, 255);
      } else if(color == "off") {
        analogWrite(redPin, 0);
        analogWrite(greenPin, 0);
        analogWrite(bluePin, 0);
      }
    }
    request->send(200, "text/plain", "OK");
  });

  server.begin();
}

void loop() {
  float temperature = dht.readTemperature();
  if(temperature > 30) {
    digitalWrite(buzzerPin, HIGH);
    lcd.setCursor(0, 0);
    lcd.print("FIRE! Temp: " + String(temperature) + " C");
    lcd.setCursor(0, 1);
    lcd.print("FIRE! FIRE! FIRE!");
    analogWrite(redPin, 255);
    analogWrite(greenPin, 0);
    analogWrite(bluePin, 0);
  } else {
    digitalWrite(buzzerPin, LOW);
  }
}

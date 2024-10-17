// Blynk configuration
#define BLYNK_TEMPLATE_ID "TMPL6iYve3-rj"
#define BLYNK_TEMPLATE_NAME "FC"
#define BLYNK_DEVICE_NAME "ROOM COMFORT AUTOMATION"
#define BLYNK_AUTH_TOKEN "NAuvNFiTs5uZ-7dJd-xiTOmMOPkTG1kw"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <DHT.h>
#include <Wire.h>
#include <BH1750.h>

// Blynk auth token
char auth[] = BLYNK_AUTH_TOKEN;
// Your WiFi credentials
char ssid[] = "iPhone SE 2022";
char pass[] = "12345678";

// Pin definitions
#define DHTPIN D7             // DHT11 sensor pin
#define DHTTYPE DHT11
#define RELAY_COOLING D0      // Cooling relay
#define RELAY_HUMIDIFIER D1   // Humidifier relay
#define RELAY_EXHAUST D2      // Exhaust fan relay

// DC Motor (L9110 driver) pins for blinds control
#define MOTOR_IN1 D3          // Motor control pin 1
#define MOTOR_IN2 D4          // Motor control pin 2

// Pins for BH1750 Lightlevel Sensor 
#define SDA_PIN D5
#define SCL_PIN D6

// LED pins for working (green) and error (red)
#define LED_GREEN D8          // Green LED for working status
#define LED_RED D9            // Red LED for error detection

// Blynk virtual pins
#define VPIN_LUX V1           // Lux value display
#define VPIN_TEMP V2          // Temperature value display
#define VPIN_HUMIDITY V3      // Humidity value display
#define VPIN_BLINDS_SWITCH V4 // Blinds automation switch
#define VPIN_MANUAL_BLINDS V5 // Manual blinds control

// Variables
bool blindsAutomationEnabled = true;
bool blindsOpen = false; // Variable to track blinds position
unsigned long motorRunTime = 2000; // Time to run motor to fully open/close blinds (adjust as needed)
unsigned long motorStartTime = 0;  // Track when motor started running
bool motorRunning = false;         // Motor running status
int motorDirection = 0;            // 1 for opening, -1 for closing
int X = 10;

DHT dht(DHTPIN, DHTTYPE);
BH1750 lightMeter;

void setup() {
  Serial.begin(9600); // Default serial communication
  Blynk.begin(auth, ssid, pass);
  
  // Initialize DHT11 sensor and light sensor
  dht.begin();
  
  // Initialize I2C communication for the BH1750 light sensor
  Wire.begin(SDA_PIN, SCL_PIN);
  lightMeter.begin();
  
  // Set pin modes for relays, motor control, and LEDs
  pinMode(RELAY_COOLING, OUTPUT);
  pinMode(RELAY_HUMIDIFIER, OUTPUT);
  pinMode(RELAY_EXHAUST, OUTPUT);
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  // Set initial states for relays, motor, and LEDs
  digitalWrite(RELAY_COOLING, HIGH);  // Relay OFF
  digitalWrite(RELAY_HUMIDIFIER, HIGH); // Relay OFF
  digitalWrite(RELAY_EXHAUST, HIGH);    // Relay OFF
  stopMotor(); // Ensure motor is off at startup
  digitalWrite(LED_GREEN, HIGH);  // Start with both LEDs off
  digitalWrite(LED_RED, HIGH);    // Start with both LEDs off
}

// Blynk switch handlers for automation control
BLYNK_WRITE(VPIN_BLINDS_SWITCH) {
  blindsAutomationEnabled = param.asInt();
}

BLYNK_WRITE(VPIN_MANUAL_BLINDS) {
  int value = param.asInt();
  if (value == 1 && !blindsOpen && !blindsAutomationEnabled) {
    openBlinds(); // Open the blinds
  } else if (value == 0 && blindsOpen) {
    closeBlinds(); // Close the blinds
  }
}

void loop() {
  Blynk.run();

  // Read sensor values
  float lux = lightMeter.readLightLevel();
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Check if any sensor reading is invalid (NaN or lux < 0)
  if (isnan(temperature) || isnan(humidity) || lux < 0) {
    // Error: One or more sensors failed
    digitalWrite(LED_GREEN, LOW);  // Turn off green LED
    digitalWrite(LED_RED, HIGH);   // Turn on red LED to signal an error
  } else {
    // Sensors are working correctly
    digitalWrite(LED_GREEN, HIGH); // Turn on green LED
    digitalWrite(LED_RED, LOW);    // Turn off red LED

    // Write values to Blynk
    Blynk.virtualWrite(VPIN_LUX, lux);
    Blynk.virtualWrite(VPIN_TEMP, temperature);
    Blynk.virtualWrite(VPIN_HUMIDITY, humidity);
    
    // Check and control components based on conditions
    if (blindsAutomationEnabled) {
      controlBlinds();
    }
    controlTemperature();
    controlHumidity();

    // Handle the motor operation in a non-blocking way
    handleMotor();

    // Print sensor readings and system state to serial monitor
    printStatus();
  }

  // Add delay to slow down the serial output
  delay(2000); // 2 seconds delay
}

void controlBlinds() {
  if (!blindsAutomationEnabled) {
    return; // Exit if automation is disabled
  }

  float lux = lightMeter.readLightLevel();

  if (lux < 50 && blindsOpen && !motorRunning) {
    closeBlinds(); // Close the blinds
  } else if (lux >= 50 && lux <= 150 && !blindsOpen && !motorRunning) {
    openBlinds(); // Open the blinds
  } else if (lux > 150 && blindsOpen && !motorRunning) {
    closeBlinds(); // Close the blinds
  }
}

void handleMotor() {
  // If motor is running, check if it should stop based on run time
  if (motorRunning && (millis() - motorStartTime >= motorRunTime)) {
    stopMotor(); // Stop motor after running for specified time
  }
}

void openBlinds() {
  motorStartTime = millis(); // Start time for the motor
  motorRunning = true;
  motorDirection = 1; // Opening blinds
  digitalWrite(MOTOR_IN1, X);
  digitalWrite(MOTOR_IN2, LOW);
  blindsOpen = true; // Set blinds as open
}

void closeBlinds() {
  motorStartTime = millis(); // Start time for the motor
  motorRunning = true;
  motorDirection = -1; // Closing blinds
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, X);
  blindsOpen = false; // Set blinds as closed
}

void stopMotor() {
  motorRunning = false;
  motorDirection = 0;
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
}

void controlTemperature() {
  float temperature = dht.readTemperature();
  if (isnan(temperature)) {
    Serial.println("Failed to read temperature!");
    return;
  }

  if (temperature > 22) {
    digitalWrite(RELAY_COOLING, LOW);  // Turn ON cooling
  } else if (temperature >= 19 && temperature <= 22) {
    digitalWrite(RELAY_COOLING, HIGH); // Turn OFF cooling
  }
}

void controlHumidity() {
  float humidity = dht.readHumidity();
  if (isnan(humidity)) {
    Serial.println("Failed to read humidity!");
    return;
  }

  if (humidity < 40) {
    digitalWrite(RELAY_HUMIDIFIER, LOW); // Turn ON humidifier
    digitalWrite(RELAY_EXHAUST, HIGH);   // Turn OFF exhaust fan
  } else if (humidity >= 40 && humidity <= 60) {
    digitalWrite(RELAY_HUMIDIFIER, HIGH); // Turn OFF humidifier
    digitalWrite(RELAY_EXHAUST, HIGH);   // Turn OFF exhaust fan
  } else if (humidity > 60) {
    digitalWrite(RELAY_HUMIDIFIER, HIGH); // Turn OFF humidifier
    digitalWrite(RELAY_EXHAUST, LOW);    // Turn ON exhaust fan
  }
}

void printStatus() {
  float lux = lightMeter.readLightLevel();
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  Serial.println("----------System Status----------");
  Serial.print("Lux Level: ");
  Serial.println(lux);
  Serial.print("Temperature (°C): ");
  Serial.println(temperature);
  Serial.print("Humidity (%): ");
  Serial.println(humidity);
  Serial.print("Blinds Position: ");
  Serial.println(blindsOpen ? "Open" : "Closed");
  Serial.print("Blinds Automation: ");
  Serial.println(blindsAutomationEnabled ? "Enabled" : "Disabled");
  Serial.print("Cooling Relay State: ");
  Serial.println(digitalRead(RELAY_COOLING) == LOW ? "ON" : "OFF");
  Serial.print("Humidifier Relay State: ");
  Serial.println(digitalRead(RELAY_HUMIDIFIER) == LOW ? "ON" : "OFF");
  Serial.print("Exhaust Fan Relay State: ");
  Serial.println(digitalRead(RELAY_EXHAUST) == LOW ? "ON" : "OFF");
  Serial.println("---------------------------------");
}

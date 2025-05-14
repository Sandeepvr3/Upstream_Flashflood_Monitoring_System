// Include Libraries
#include <SPI.h>
#include <LoRa.h>

// Pin Definitions
#define FLOW_SENSOR_PIN 35
#define RAIN_SENSOR_PIN 34
#define ANEMOMETER_PIN 32
#define TRIG_PIN 27
#define ECHO_PIN 25

// LoRa SX1278 Module Connections
#define LORA_SS 5
#define LORA_RST 14
#define LORA_DIO0 26

// Variables for Sensor Data
volatile int flowCount = 0;
float flowRate = 0;
float rainfall_mm = 0;
float windSpeed = 0;
float waterLevel_cm = 0;

int rainSensorState = 0;
int prevRainSensorState = 0;

// Anemometer Variables
volatile float revolutions = 0;
int windSampleCount = 0;  // Counter for wind speed measurement

// Calibration Constants
const float FLOW_CONVERSION = 7.5;       // Pulses to L/min
const float RAIN_CONVERSION = 0.2794;    // Pulses to mm
const float WIND_CONVERSION = 0.18;      // Pulses to km/h
const float TOTAL_RIVER_DEPTH = 28.0;    // Total depth of the river in cm

// Interrupt Service Routines
void IRAM_ATTR flowISR() {
  flowCount++;
}

void IRAM_ATTR windISR() {
  revolutions++;
}

void setup() {
  Serial.begin(115200);
  
  // LoRa Initialization
  SPI.begin(18, 19, 23, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa Initialization Failed!");
    while (1);
  }
  Serial.println("LoRa SX1278 Initialized Successfully!");
  
  // Sensor Pin Modes
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  pinMode(RAIN_SENSOR_PIN, INPUT_PULLUP);
  pinMode(ANEMOMETER_PIN, INPUT_PULLUP);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Attach Interrupts
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), flowISR, RISING);
  attachInterrupt(digitalPinToInterrupt(ANEMOMETER_PIN), windISR, RISING);
}

void loop() {
  // Calculate Flow Rate
  flowRate = (flowCount * FLOW_CONVERSION);

  // Update Rainfall Logic using State Change Detection
  rainSensorState = digitalRead(RAIN_SENSOR_PIN);
  if (rainSensorState != prevRainSensorState) {
    rainfall_mm += RAIN_CONVERSION;
  }
  prevRainSensorState = rainSensorState;

  // Calculate Wind Speed (every 2 seconds)
  windSpeed = revolutions * WIND_CONVERSION;
  windSampleCount++;

  // Every 30 readings (60 seconds), reset revolutions for accurate long-term measurement
  if (windSampleCount >= 30) {
    revolutions = 0;
    windSampleCount = 0;
  }

  // Measure Water Level with Ultrasonic Sensor
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  float measuredDistance = (0.034 * duration) / 2; // Distance from sensor to water surface

  // Adjusted Water Level Calculation
  waterLevel_cm = TOTAL_RIVER_DEPTH - measuredDistance;

  // Ensure water level does not go negative
  if (waterLevel_cm < 0) {
    waterLevel_cm = 0;
  }

  // Prepare Data String in Consistent Format: rainfall,flow_rate,water_level,wind_speed
  String data = String(rainfall_mm, 2) + "," +
                String(flowRate, 2) + "," +
                String(waterLevel_cm, 2) + "," +
                String(windSpeed, 2);

  // Print Data to Serial Monitor
  Serial.println("Sending Data via LoRa: " + data);

  // Send Sensor Data via LoRa SX1278
  LoRa.beginPacket();
  LoRa.print(data);
  LoRa.endPacket();
  Serial.println("Sensor Data Sent via LoRa: " + data);

  // Reset Flow Counter for Next Reading
  flowCount = 0;

  delay(2000); // Now, data is sent every 2 seconds
}
#include <DHT.h>
#include <EEPROM.h>

// Pin definitions
#define DHT11_PIN 4
#define LDR_PIN A1
#define BUZZER_PIN 8
#define MOTION_PIR_PIN 2
#define GAS_SENSOR_PIN A0
#define RELAY_FAN_PIN 7
#define RELAY_AC_PIN 6
#define STATUS_LED_PIN 13  // Built-in LED for status indication

// LED pins for different rooms/functions
#define LED_LIVING_ROOM A2
#define LED_BEDROOM A3
#define LED_KITCHEN A4
#define LED_BATHROOM A5

// Additional LED pins for A2-A5 control
#define LED_A2_PIN A2
#define LED_A3_PIN A3
#define LED_A4_PIN A4
#define LED_A5_PIN A5

// DHT sensor object
DHT dht(DHT11_PIN, DHT11);

// Sensor reading variables
float temperature = 0.0;
float humidity = 0.0;
int ldrValue = 0;
int gasValue = 0;
int motionDetected = 0;
int gasBaseline = 0;  // Baseline gas reading for calibration

// Device states
bool livingRoomLight = false;
bool bedroomLight = false;
bool kitchenLight = false;
bool bathroomLight = false;
bool fanState = false;
bool acState = false;
bool securityAlarm = false;
bool autoLightMode = true;
bool autoTempControl = true;
bool systemEnabled = true;

// LED A2-A5 states
bool ledA2State = false;
bool ledA3State = false;
bool ledA4State = false;
bool ledA5State = false;

// Temperature alert system
bool tempAlertActive = false;
bool ledBlinkState = false;
unsigned long lastBlinkTime = 0;
unsigned long blinkInterval = 500; // 500ms blink interval

// Temperature thresholds for alert
float tempLowThreshold = 23.0;
float tempHighThreshold = 26.0;

// Timing variables
unsigned long lastMotionTime = 0;
unsigned long lastSensorRead = 0;
unsigned long lastDataSend = 0;
unsigned long lastStatusBlink = 0;
unsigned long autoLightOffDelay = 30000; // 30 seconds
unsigned long sensorReadInterval = 1000;  // 1 second
unsigned long dataSendInterval = 2000;    // 2 seconds

// Error handling
int dhtErrorCount = 0;
int gasErrorCount = 0;
bool systemError = false;

// EEPROM addresses for persistent storage
#define EEPROM_AUTO_LIGHT 0
#define EEPROM_AUTO_TEMP 1
#define EEPROM_GAS_BASELINE 2
#define EEPROM_LED_A2 3
#define EEPROM_LED_A3 4
#define EEPROM_LED_A4 5
#define EEPROM_LED_A5 6

// Temperature thresholds (can be adjusted via commands)
float fanOnTemp = 28.0;
float acOnTemp = 32.0;
float fanOffTemp = 26.0;

// Gas sensor thresholds
int gasAlarmThreshold = 400;
int gasSafeThreshold = 300;

void setup() {
  Serial.begin(9600);
  
  // Initialize DHT sensor
  dht.begin();
  
  // Set pin modes
  pinMode(LDR_PIN, INPUT);
  pinMode(MOTION_PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_FAN_PIN, OUTPUT);
  pinMode(RELAY_AC_PIN, OUTPUT);
  pinMode(LED_LIVING_ROOM, OUTPUT);
  pinMode(LED_BEDROOM, OUTPUT);
  pinMode(LED_KITCHEN, OUTPUT);
  pinMode(LED_BATHROOM, OUTPUT);
  pinMode(LED_A2_PIN, OUTPUT);
  pinMode(LED_A3_PIN, OUTPUT);
  pinMode(LED_A4_PIN, OUTPUT);
  pinMode(LED_A5_PIN, OUTPUT);
  pinMode(GAS_SENSOR_PIN, INPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);
  
  // Initialize outputs to OFF
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RELAY_FAN_PIN, LOW);
  digitalWrite(RELAY_AC_PIN, LOW);
  digitalWrite(LED_LIVING_ROOM, LOW);
  digitalWrite(LED_BEDROOM, LOW);
  digitalWrite(LED_KITCHEN, LOW);
  digitalWrite(LED_BATHROOM, LOW);
  digitalWrite(LED_A2_PIN, LOW);
  digitalWrite(LED_A3_PIN, LOW);
  digitalWrite(LED_A4_PIN, LOW);
  digitalWrite(LED_A5_PIN, LOW);
  digitalWrite(STATUS_LED_PIN, HIGH);  // System ready indicator
  
  // Load settings from EEPROM
  loadSettings();
  
  // Calibrate gas sensor (take baseline reading)
  calibrateGasSensor();
  
  // System startup message
  Serial.println("{\"status\":\"Enhanced Smart Home System Ready\",\"version\":\"3.0\"}");
  
  // Initial sensor reading
  lastSensorRead = millis();
  lastDataSend = millis();
  lastBlinkTime = millis();
}

void loop() {
  unsigned long currentTime = millis();
  
  // Read sensors at specified interval
  if (currentTime - lastSensorRead >= sensorReadInterval) {
    readSensors();
    lastSensorRead = currentTime;
  }
  
  // Process automation logic
  if (systemEnabled) {
    processAutomation();
    processSecurityAlerts();
    processTemperatureAlert();
  }
  
  // Handle LED blinking for temperature alerts
  if (tempAlertActive && currentTime - lastBlinkTime >= blinkInterval) {
    handleTemperatureBlinking();
    lastBlinkTime = currentTime;
  }
  
  // Send data at specified interval
  if (currentTime - lastDataSend >= dataSendInterval) {
    sendSensorData();
    lastDataSend = currentTime;
  }
  
  // Handle incoming commands
  handleSerialCommands();
  
  // Status LED management
  updateStatusLED();
  
  // Small delay to prevent overwhelming the system
  delay(50);
}

void readSensors() {
  // Read DHT sensor with error handling
  float tempReading = dht.readTemperature();
  float humReading = dht.readHumidity();
  
  if (isnan(tempReading) || isnan(humReading)) {
    dhtErrorCount++;
    if (dhtErrorCount > 5) {
      systemError = true;
    }
  } else {
    temperature = tempReading;
    humidity = humReading;
    dhtErrorCount = 0;
  }
  
  // Read other sensors
  ldrValue = analogRead(LDR_PIN);
  gasValue = analogRead(GAS_SENSOR_PIN);
  motionDetected = digitalRead(MOTION_PIR_PIN);
  
  // Validate gas sensor readings
  if (gasValue < 0 || gasValue > 1023) {
    gasErrorCount++;
    if (gasErrorCount > 5) {
      systemError = true;
    }
  } else {
    gasErrorCount = 0;
  }
}

void processAutomation() {
  // Auto lighting based on motion and LDR
  if (autoLightMode) {
    if (motionDetected && ldrValue < 300) { // Dark environment + motion
      if (!livingRoomLight) {
        livingRoomLight = true;
        digitalWrite(LED_LIVING_ROOM, HIGH);
      }
      lastMotionTime = millis();
    }
    
    // Auto turn off lights after no motion
    if (millis() - lastMotionTime > autoLightOffDelay && livingRoomLight) {
      livingRoomLight = false;
      digitalWrite(LED_LIVING_ROOM, LOW);
    }
  }
  
  // Auto temperature control with improved logic
  if (autoTempControl && !isnan(temperature)) {
    // Fan control
    if (temperature > fanOnTemp && !fanState) {
      fanState = true;
      digitalWrite(RELAY_FAN_PIN, HIGH);
    } else if (temperature < fanOffTemp && fanState) {
      fanState = false;
      digitalWrite(RELAY_FAN_PIN, LOW);
    }
    
    // AC control
    if (temperature > acOnTemp && !acState) {
      acState = true;
      digitalWrite(RELAY_AC_PIN, HIGH);
    } else if (temperature < fanOffTemp && acState) {
      acState = false;
      digitalWrite(RELAY_AC_PIN, LOW);
    }
  }
}

void processTemperatureAlert() {
  if (!isnan(temperature)) {
    bool shouldAlert = (temperature < tempLowThreshold || temperature > tempHighThreshold);
    
    if (shouldAlert != tempAlertActive) {
      tempAlertActive = shouldAlert;
      
      if (!tempAlertActive) {
        // Turn off blinking and restore normal LED states
        digitalWrite(LED_A2_PIN, ledA2State ? HIGH : LOW);
        digitalWrite(LED_A3_PIN, ledA3State ? HIGH : LOW);
      }
    }
  }
}

void handleTemperatureBlinking() {
  if (tempAlertActive) {
    ledBlinkState = !ledBlinkState;
    
    // Blink LED A2 and A3 regardless of their normal state
    digitalWrite(LED_A2_PIN, ledBlinkState ? HIGH : LOW);
    digitalWrite(LED_A3_PIN, ledBlinkState ? HIGH : LOW);
  }
}

void processSecurityAlerts() {
  // Gas leak detection with hysteresis
  if (gasValue > gasAlarmThreshold) {
    if (!securityAlarm) {
      securityAlarm = true;
      digitalWrite(BUZZER_PIN, HIGH);
    }
  } else if (gasValue < gasSafeThreshold && securityAlarm) {
    securityAlarm = false;
    digitalWrite(BUZZER_PIN, LOW);
  }
}

void sendSensorData() {
  // Send sensor data as JSON with error status and LED states
  Serial.print("{\"temperature\":");
  Serial.print(isnan(temperature) ? 0 : temperature, 1);
  Serial.print(",\"humidity\":");
  Serial.print(isnan(humidity) ? 0 : humidity, 1);
  Serial.print(",\"ldr\":");
  Serial.print(ldrValue);
  Serial.print(",\"gas\":");
  Serial.print(gasValue);
  Serial.print(",\"motion\":");
  Serial.print(motionDetected);
  Serial.print(",\"livingRoom\":");
  Serial.print(livingRoomLight ? 1 : 0);
  Serial.print(",\"bedroom\":");
  Serial.print(bedroomLight ? 1 : 0);
  Serial.print(",\"kitchen\":");
  Serial.print(kitchenLight ? 1 : 0);
  Serial.print(",\"bathroom\":");
  Serial.print(bathroomLight ? 1 : 0);
  Serial.print(",\"ledA2\":");
  Serial.print(ledA2State ? 1 : 0);
  Serial.print(",\"ledA3\":");
  Serial.print(ledA3State ? 1 : 0);
  Serial.print(",\"ledA4\":");
  Serial.print(ledA4State ? 1 : 0);
  Serial.print(",\"ledA5\":");
  Serial.print(ledA5State ? 1 : 0);
  Serial.print(",\"fan\":");
  Serial.print(fanState ? 1 : 0);
  Serial.print(",\"ac\":");
  Serial.print(acState ? 1 : 0);
  Serial.print(",\"alarm\":");
  Serial.print(securityAlarm ? 1 : 0);
  Serial.print(",\"autoLight\":");
  Serial.print(autoLightMode ? 1 : 0);
  Serial.print(",\"autoTemp\":");
  Serial.print(autoTempControl ? 1 : 0);
  Serial.print(",\"tempAlert\":");
  Serial.print(tempAlertActive ? 1 : 0);
  Serial.print(",\"systemError\":");
  Serial.print(systemError ? 1 : 0);
  Serial.print(",\"dhtErrors\":");
  Serial.print(dhtErrorCount);
  Serial.print(",\"gasBaseline\":");
  Serial.print(gasBaseline);
  Serial.print(",\"uptime\":");
  Serial.print(millis() / 1000);
  Serial.println("}");
  
  // Reset system error flag after reporting
  if (systemError && dhtErrorCount == 0 && gasErrorCount == 0) {
    systemError = false;
  }
}

void handleSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readString();
    command.trim();
    command.toUpperCase();
    
    // Room lighting controls
    if (command == "LIVING_ON") {
      livingRoomLight = true;
      digitalWrite(LED_LIVING_ROOM, HIGH);
    }
    else if (command == "LIVING_OFF") {
      livingRoomLight = false;
      digitalWrite(LED_LIVING_ROOM, LOW);
    }
    else if (command == "BEDROOM_ON") {
      bedroomLight = true;
      digitalWrite(LED_BEDROOM, HIGH);
    }
    else if (command == "BEDROOM_OFF") {
      bedroomLight = false;
      digitalWrite(LED_BEDROOM, LOW);
    }
    else if (command == "KITCHEN_ON") {
      kitchenLight = true;
      digitalWrite(LED_KITCHEN, HIGH);
    }
    else if (command == "KITCHEN_OFF") {
      kitchenLight = false;
      digitalWrite(LED_KITCHEN, LOW);
    }
    else if (command == "BATHROOM_ON") {
      bathroomLight = true;
      digitalWrite(LED_BATHROOM, HIGH);
    }
    else if (command == "BATHROOM_OFF") {
      bathroomLight = false;
      digitalWrite(LED_BATHROOM, LOW);
    }
    
    // LED A2-A5 controls
    else if (command == "LED_A2_ON") {
      ledA2State = true;
      if (!tempAlertActive) digitalWrite(LED_A2_PIN, HIGH);
      saveSettings();
    }
    else if (command == "LED_A2_OFF") {
      ledA2State = false;
      if (!tempAlertActive) digitalWrite(LED_A2_PIN, LOW);
      saveSettings();
    }
    else if (command == "LED_A3_ON") {
      ledA3State = true;
      if (!tempAlertActive) digitalWrite(LED_A3_PIN, HIGH);
      saveSettings();
    }
    else if (command == "LED_A3_OFF") {
      ledA3State = false;
      if (!tempAlertActive) digitalWrite(LED_A3_PIN, LOW);
      saveSettings();
    }
    else if (command == "LED_A4_ON") {
      ledA4State = true;
      digitalWrite(LED_A4_PIN, HIGH);
      saveSettings();
    }
    else if (command == "LED_A4_OFF") {
      ledA4State = false;
      digitalWrite(LED_A4_PIN, LOW);
      saveSettings();
    }
    else if (command == "LED_A5_ON") {
      ledA5State = true;
      digitalWrite(LED_A5_PIN, HIGH);
      saveSettings();
    }
    else if (command == "LED_A5_OFF") {
      ledA5State = false;
      digitalWrite(LED_A5_PIN, LOW);
      saveSettings();
    }
    
    // Climate controls
    else if (command == "FAN_ON") {
      fanState = true;
      digitalWrite(RELAY_FAN_PIN, HIGH);
    }
    else if (command == "FAN_OFF") {
      fanState = false;
      digitalWrite(RELAY_FAN_PIN, LOW);
    }
    else if (command == "AC_ON") {
      acState = true;
      digitalWrite(RELAY_AC_PIN, HIGH);
    }
    else if (command == "AC_OFF") {
      acState = false;
      digitalWrite(RELAY_AC_PIN, LOW);
    }
    
    // Temperature alert controls
    else if (command == "TEMP_ALERT_ON") {
      tempAlertActive = true;
    }
    else if (command == "TEMP_ALERT_OFF") {
      tempAlertActive = false;
      // Restore normal LED states
      digitalWrite(LED_A2_PIN, ledA2State ? HIGH : LOW);
      digitalWrite(LED_A3_PIN, ledA3State ? HIGH : LOW);
    }
    
    // Security
    else if (command == "ALARM_OFF") {
      securityAlarm = false;
      digitalWrite(BUZZER_PIN, LOW);
    }
    
    // Automation toggles
    else if (command == "AUTO_LIGHT_TOGGLE") {
      autoLightMode = !autoLightMode;
      saveSettings();
    }
    else if (command == "AUTO_TEMP_TOGGLE") {
      autoTempControl = !autoTempControl;
      saveSettings();
    }
    
    // System controls
    else if (command == "SYSTEM_ENABLE") {
      systemEnabled = true;
    }
    else if (command == "SYSTEM_DISABLE") {
      systemEnabled = false;
    }
    else if (command == "CALIBRATE_GAS") {
      calibrateGasSensor();
    }
    else if (command == "RESET_ERRORS") {
      dhtErrorCount = 0;
      gasErrorCount = 0;
      systemError = false;
    }
    
    // All controls
    else if (command == "ALL_LIGHTS_OFF") {
      livingRoomLight = bedroomLight = kitchenLight = bathroomLight = false;
      digitalWrite(LED_LIVING_ROOM, LOW);
      digitalWrite(LED_BEDROOM, LOW);
      digitalWrite(LED_KITCHEN, LOW);
      digitalWrite(LED_BATHROOM, LOW);
    }
    else if (command == "ALL_LEDS_OFF") {
      ledA2State = ledA3State = ledA4State = ledA5State = false;
      if (!tempAlertActive) {
        digitalWrite(LED_A2_PIN, LOW);
        digitalWrite(LED_A3_PIN, LOW);
      }
      digitalWrite(LED_A4_PIN, LOW);
      digitalWrite(LED_A5_PIN, LOW);
      saveSettings();
    }
    else if (command == "ALL_OFF") {
      livingRoomLight = bedroomLight = kitchenLight = bathroomLight = false;
      ledA2State = ledA3State = ledA4State = ledA5State = false;
      fanState = acState = false;
      digitalWrite(LED_LIVING_ROOM, LOW);
      digitalWrite(LED_BEDROOM, LOW);
      digitalWrite(LED_KITCHEN, LOW);
      digitalWrite(LED_BATHROOM, LOW);
      if (!tempAlertActive) {
        digitalWrite(LED_A2_PIN, LOW);
        digitalWrite(LED_A3_PIN, LOW);
      }
      digitalWrite(LED_A4_PIN, LOW);
      digitalWrite(LED_A5_PIN, LOW);
      digitalWrite(RELAY_FAN_PIN, LOW);
      digitalWrite(RELAY_AC_PIN, LOW);
      saveSettings();
    }
    
    // Status request
    else if (command == "STATUS") {
      Serial.println("{\"command\":\"status_requested\"}");
    }
    
    // Unknown command
    else if (command.length() > 0) {
      Serial.print("{\"error\":\"Unknown command: ");
      Serial.print(command);
      Serial.println("\"}");
    }
  }
}

void updateStatusLED() {
  unsigned long currentTime = millis();
  
  if (systemError) {
    // Fast blink for system error
    if (currentTime - lastStatusBlink > 250) {
      digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
      lastStatusBlink = currentTime;
    }
  } else if (securityAlarm) {
    // Slow blink for security alarm
    if (currentTime - lastStatusBlink > 1000) {
      digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
      lastStatusBlink = currentTime;
    }
  } else {
    // Steady on for normal operation
    digitalWrite(STATUS_LED_PIN, HIGH);
  }
}

void calibrateGasSensor() {
  Serial.println("{\"status\":\"Calibrating gas sensor...\"}");
  
  // Take multiple readings for baseline
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(GAS_SENSOR_PIN);
    delay(100);
  }
  
  gasBaseline = sum / 10;
  
  // Adjust thresholds based on baseline
  gasAlarmThreshold = gasBaseline + 200;
  gasSafeThreshold = gasBaseline + 100;
  
  // Save baseline to EEPROM
  EEPROM.write(EEPROM_GAS_BASELINE, gasBaseline / 4); // Scale down to fit in byte
  
  Serial.print("{\"status\":\"Gas sensor calibrated\",\"baseline\":");
  Serial.print(gasBaseline);
  Serial.print(",\"alarmThreshold\":");
  Serial.print(gasAlarmThreshold);
  Serial.println("}");
}

void saveSettings() {
  EEPROM.write(EEPROM_AUTO_LIGHT, autoLightMode ? 1 : 0);
  EEPROM.write(EEPROM_AUTO_TEMP, autoTempControl ? 1 : 0);
  EEPROM.write(EEPROM_LED_A2, ledA2State ? 1 : 0);
  EEPROM.write(EEPROM_LED_A3, ledA3State ? 1 : 0);
  EEPROM.write(EEPROM_LED_A4, ledA4State ? 1 : 0);
  EEPROM.write(EEPROM_LED_A5, ledA5State ? 1 : 0);
}

void loadSettings() {
  byte autoLight = EEPROM.read(EEPROM_AUTO_LIGHT);
  byte autoTemp = EEPROM.read(EEPROM_AUTO_TEMP);
  byte savedBaseline = EEPROM.read(EEPROM_GAS_BASELINE);
  byte ledA2 = EEPROM.read(EEPROM_LED_A2);
  byte ledA3 = EEPROM.read(EEPROM_LED_A3);
  byte ledA4 = EEPROM.read(EEPROM_LED_A4);
  byte ledA5 = EEPROM.read(EEPROM_LED_A5);
  
  // Only load if values are valid (EEPROM default is 255)
  if (autoLight == 0 || autoLight == 1) {
    autoLightMode = (autoLight == 1);
  }
  if (autoTemp == 0 || autoTemp == 1) {
    autoTempControl = (autoTemp == 1);
  }
  if (savedBaseline != 255) {
    gasBaseline = savedBaseline * 4; // Scale back up
    gasAlarmThreshold = gasBaseline + 200;
    gasSafeThreshold = gasBaseline + 100;
  }
  
  // Load LED states
  if (ledA2 == 0 || ledA2 == 1) {
    ledA2State = (ledA2 == 1);
    digitalWrite(LED_A2_PIN, ledA2State ? HIGH : LOW);
  }
  if (ledA3 == 0 || ledA3 == 1) {
    ledA3State = (ledA3 == 1);
    digitalWrite(LED_A3_PIN, ledA3State ? HIGH : LOW);
  }
  if (ledA4 == 0 || ledA4 == 1) {
    ledA4State = (ledA4 == 1);
    digitalWrite(LED_A4_PIN, ledA4State ? HIGH : LOW);
  }
  if (ledA5 == 0 || ledA5 == 1) {
    ledA5State = (ledA5 == 1);
    digitalWrite(LED_A5_PIN, ledA5State ? HIGH : LOW);
  }
}

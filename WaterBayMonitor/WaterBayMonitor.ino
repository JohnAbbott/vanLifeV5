#include <Arduino.h>
#include <SPI.h>
#include <mcp2515_can.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiS3.h>
#include <WiFiSSLClient.h>
#include <ArduinoHttpClient.h>

#include "arduino_secrets.h"

// Hardware pins. Keep CAN pins aligned with the Seeed CAN-BUS Shield V2.0 jumpers.
const uint8_t ONE_WIRE_PIN = 4;
const uint8_t FAN_PWM_PIN = 5;
const uint8_t CAN_CS_PIN = 9;
const uint8_t CAN_INT_PIN = 2;

const uint32_t SERIAL_BAUD = 115200;
const unsigned long SENSOR_INTERVAL_MS = 5000;
const unsigned long CAN_INTERVAL_MS = 5000;
const unsigned long WIFI_RETRY_INTERVAL_MS = 30000;
const unsigned long SMS_COOLDOWN_MS = 6UL * 60UL * 60UL * 1000UL;

const float FAN_START_F = 40.0;
const float FAN_FULL_F = 35.0;
const float FAN_OFF_F = 42.0;
const float SMS_ALERT_F = 34.0;
const float SMS_REARM_F = 36.0;
const uint8_t FAN_MIN_PWM = 80;

const uint16_t CAN_ID_BASE = 0x541;
const uint16_t CAN_ID_STATUS = 0x540;
const uint8_t SENSOR_COUNT = 4;

OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);
mcp2515_can CAN0(CAN_CS_PIN);
WiFiSSLClient sslClient;

float temperatureF[SENSOR_COUNT];
DeviceAddress sensorAddresses[SENSOR_COUNT];
uint8_t discoveredSensors = 0;
uint8_t fanPwm = 0;
bool canReady = false;
bool smsArmed = true;
unsigned long lastSensorReadMs = 0;
unsigned long lastCanSendMs = 0;
unsigned long lastWifiAttemptMs = 0;
unsigned long lastSmsMs = 0;

bool isValidTemperature(float value) {
  return value > -100.0 && value < 180.0;
}

float coldestValidTemperature() {
  float coldest = NAN;
  for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
    if (!isValidTemperature(temperatureF[i])) {
      continue;
    }
    if (isnan(coldest) || temperatureF[i] < coldest) {
      coldest = temperatureF[i];
    }
  }
  return coldest;
}

String base64Encode(const String &input) {
  const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String encoded;
  int value = 0;
  int bits = -6;

  for (size_t i = 0; i < input.length(); i++) {
    value = (value << 8) + static_cast<uint8_t>(input[i]);
    bits += 8;
    while (bits >= 0) {
      encoded += alphabet[(value >> bits) & 0x3F];
      bits -= 6;
    }
  }

  if (bits > -6) {
    encoded += alphabet[((value << 8) >> (bits + 8)) & 0x3F];
  }
  while (encoded.length() % 4) {
    encoded += '=';
  }
  return encoded;
}

String urlEncode(const String &value) {
  const char hex[] = "0123456789ABCDEF";
  String encoded;
  for (size_t i = 0; i < value.length(); i++) {
    char c = value[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else {
      encoded += '%';
      encoded += hex[(c >> 4) & 0x0F];
      encoded += hex[c & 0x0F];
    }
  }
  return encoded;
}

void connectWifiIfNeeded() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  unsigned long now = millis();
  if (now - lastWifiAttemptMs < WIFI_RETRY_INTERVAL_MS) {
    return;
  }

  lastWifiAttemptMs = now;
  Serial.print("Connecting to Wi-Fi SSID ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

bool sendTwilioSms(float coldestF) {
  if (strlen(TWILIO_TO_NUMBER) == 0) {
    Serial.println("Twilio destination number is blank; SMS skipped.");
    return false;
  }

  connectWifiIfNeeded();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi is not connected; SMS skipped.");
    return false;
  }

  String path = "/2010-04-01/Accounts/";
  path += TWILIO_ACCOUNT_SID;
  path += "/Messages.json";

  String body = "To=" + urlEncode(TWILIO_TO_NUMBER) +
                "&From=" + urlEncode(TWILIO_FROM_NUMBER) +
                "&Body=" + urlEncode("Water bay temperature alert: coldest sensor is " +
                                      String(coldestF, 1) + " F.");

  HttpClient http(sslClient, "api.twilio.com", 443);
  http.beginRequest();
  http.post(path);
  http.sendHeader("Authorization", "Basic " +
                                       base64Encode(String(TWILIO_ACCOUNT_SID) + ":" +
                                                    String(TWILIO_AUTH_TOKEN)));
  http.sendHeader("Content-Type", "application/x-www-form-urlencoded");
  http.sendHeader("Content-Length", body.length());
  http.beginBody();
  http.print(body);
  http.endRequest();

  int statusCode = http.responseStatusCode();
  String response = http.responseBody();
  Serial.print("Twilio HTTP status: ");
  Serial.println(statusCode);
  if (statusCode < 200 || statusCode >= 300) {
    Serial.println(response);
  }

  http.stop();
  return statusCode >= 200 && statusCode < 300;
}

void discoverTemperatureSensors() {
  sensors.begin();
  discoveredSensors = min<uint8_t>(sensors.getDeviceCount(), SENSOR_COUNT);

  for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
    temperatureF[i] = NAN;
    if (i < discoveredSensors && sensors.getAddress(sensorAddresses[i], i)) {
      sensors.setResolution(sensorAddresses[i], 11);
    }
  }

  Serial.print("Discovered DS18B20 sensors: ");
  Serial.println(discoveredSensors);
}

void readTemperatures() {
  sensors.requestTemperatures();
  for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
    if (i < discoveredSensors) {
      temperatureF[i] = sensors.getTempF(sensorAddresses[i]);
    } else {
      temperatureF[i] = NAN;
    }

    Serial.print("Temp ");
    Serial.print(i + 1);
    Serial.print(": ");
    if (isValidTemperature(temperatureF[i])) {
      Serial.print(temperatureF[i], 1);
      Serial.println(" F");
    } else {
      Serial.println("invalid");
    }
  }
}

void updateFan(float coldestF) {
  if (isnan(coldestF)) {
    fanPwm = 0;
  } else if (coldestF <= FAN_FULL_F) {
    fanPwm = 255;
  } else if (coldestF < FAN_START_F) {
    fanPwm = map(lround(coldestF * 10), lround(FAN_START_F * 10),
                 lround(FAN_FULL_F * 10), FAN_MIN_PWM, 255);
  } else if (coldestF >= FAN_OFF_F) {
    fanPwm = 0;
  }

  analogWrite(FAN_PWM_PIN, fanPwm);
}

void handleSmsAlert(float coldestF) {
  if (isnan(coldestF)) {
    return;
  }

  if (coldestF >= SMS_REARM_F) {
    smsArmed = true;
  }

  unsigned long now = millis();
  bool cooldownExpired = lastSmsMs == 0 || now - lastSmsMs >= SMS_COOLDOWN_MS;
  if (smsArmed && cooldownExpired && coldestF < SMS_ALERT_F) {
    if (sendTwilioSms(coldestF)) {
      lastSmsMs = now;
      smsArmed = false;
    }
  }
}

void setupCan() {
  pinMode(CAN_INT_PIN, INPUT);

  if (CAN0.begin(CAN_500KBPS, MCP_16MHz) == CAN_OK) {
    CAN0.setMode(MODE_NORMAL);
    canReady = true;
    Serial.println("CAN initialized at 500 kbps.");
  } else {
    canReady = false;
    Serial.println("CAN initialization failed.");
  }
}

void sendTemperatureFrame(uint8_t index) {
  int16_t tempTenths = isValidTemperature(temperatureF[index])
                           ? static_cast<int16_t>(lround(temperatureF[index] * 10.0))
                           : INT16_MIN;
  uint8_t payload[8] = {
      index,
      static_cast<uint8_t>(tempTenths & 0xFF),
      static_cast<uint8_t>((tempTenths >> 8) & 0xFF),
      isValidTemperature(temperatureF[index]) ? 1 : 0,
      fanPwm,
      discoveredSensors,
      0,
      0,
  };
  CAN0.sendMsgBuf(CAN_ID_BASE + index, 0, 8, payload);
}

void sendStatusFrame(float coldestF) {
  int16_t coldestTenths = isnan(coldestF)
                              ? INT16_MIN
                              : static_cast<int16_t>(lround(coldestF * 10.0));
  uint8_t flags = 0;
  if (WiFi.status() == WL_CONNECTED) {
    flags |= 0x01;
  }
  if (smsArmed) {
    flags |= 0x02;
  }

  uint8_t payload[8] = {
      static_cast<uint8_t>(coldestTenths & 0xFF),
      static_cast<uint8_t>((coldestTenths >> 8) & 0xFF),
      fanPwm,
      discoveredSensors,
      flags,
      0,
      0,
      0,
  };
  CAN0.sendMsgBuf(CAN_ID_STATUS, 0, 8, payload);
}

void sendCanFrames(float coldestF) {
  if (!canReady) {
    return;
  }

  sendStatusFrame(coldestF);
  for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
    sendTemperatureFrame(i);
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1500);

  pinMode(FAN_PWM_PIN, OUTPUT);
  analogWrite(FAN_PWM_PIN, 0);

  discoverTemperatureSensors();
  setupCan();
  connectWifiIfNeeded();
}

void loop() {
  unsigned long now = millis();
  connectWifiIfNeeded();

  if (now - lastSensorReadMs >= SENSOR_INTERVAL_MS) {
    lastSensorReadMs = now;
    readTemperatures();
    float coldestF = coldestValidTemperature();
    updateFan(coldestF);
    handleSmsAlert(coldestF);
  }

  if (now - lastCanSendMs >= CAN_INTERVAL_MS) {
    lastCanSendMs = now;
    sendCanFrames(coldestValidTemperature());
  }
}

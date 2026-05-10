#include <Arduino.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include "User_Setup.h"
#include <TFT_eSPI.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <Ticker.h>
#include <lvgl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <test.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "printer.h"
#include "myconfig.h"

namespace {
WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);
printer_values cachedValues;
bool hasMqttData = false;
unsigned long lastMqttConnectAttempt = 0;
unsigned long lastMqttPushRequest = 0;
char mqttReportTopic[64] = {0};
char mqttRequestTopic[64] = {0};

uint16_t toPercent(float value)
{
  if (value < 0.0f)
    return 0;
  if (value <= 1.0f)
    return (uint16_t)(value * 100.0f);
  if (value <= 15.0f)
    return (uint16_t)((value / 15.0f) * 100.0f);
  if (value > 100.0f)
    return 100;
  return (uint16_t)value;
}

bool isPrintingState(const String &state)
{
  return state == "RUNNING" || state == "PREPARE" || state == "PAUSE" || state == "SLICING";
}

void requestPrinterPush()
{
  static uint32_t sequence = 1;
  char payload[128];
  snprintf(payload, sizeof(payload), "{\"pushing\":{\"sequence_id\":\"%lu\",\"command\":\"pushall\"}}", (unsigned long)sequence++);
  mqttClient.publish(mqttRequestTopic, payload);
  lastMqttPushRequest = millis();
}

void onMqttMessage(char *topic, byte *payload, unsigned int length)
{
  if (strcmp(topic, mqttReportTopic) != 0)
    return;

  DynamicJsonDocument doc(12288);
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err)
  {
    Serial.print("MQTT JSON parse error: ");
    Serial.println(err.c_str());
    return;
  }

  JsonVariant printRoot = doc["print"];
  if (printRoot.isNull())
    printRoot = doc.as<JsonVariant>();

  if (!printRoot["bed_temper"].isNull())
    cachedValues.bedtemp_actual = (uint16_t)printRoot["bed_temper"].as<float>();
  if (!printRoot["bed_target_temper"].isNull())
    cachedValues.bedtemp_target = (uint16_t)printRoot["bed_target_temper"].as<float>();
  if (!printRoot["nozzle_temper"].isNull())
    cachedValues.tooltemp_actual = (uint16_t)printRoot["nozzle_temper"].as<float>();
  if (!printRoot["nozzle_target_temper"].isNull())
    cachedValues.tooltemp_target = (uint16_t)printRoot["nozzle_target_temper"].as<float>();
  if (!printRoot["chamber_temper"].isNull())
    cachedValues.chamber_temp = (uint16_t)printRoot["chamber_temper"].as<float>();

  if (!printRoot["layer_num"].isNull())
    cachedValues.layer_current = (uint16_t)printRoot["layer_num"].as<int>();
  else if (!printRoot["current_layer"].isNull())
    cachedValues.layer_current = (uint16_t)printRoot["current_layer"].as<int>();

  if (!printRoot["total_layer_num"].isNull())
    cachedValues.layer_total = (uint16_t)printRoot["total_layer_num"].as<int>();
  else if (!printRoot["total_layer"].isNull())
    cachedValues.layer_total = (uint16_t)printRoot["total_layer"].as<int>();

  if (!printRoot["mc_percent"].isNull())
    cachedValues.progress = printRoot["mc_percent"].as<float>();
  else if (!printRoot["progress"].isNull())
    cachedValues.progress = toPercent(printRoot["progress"].as<float>());

  if (!printRoot["cooling_fan_speed"].isNull())
    cachedValues.fan_speed = toPercent(printRoot["cooling_fan_speed"].as<float>());
  else if (!printRoot["fan_gear"].isNull())
    cachedValues.fan_speed = toPercent(printRoot["fan_gear"].as<float>());

  String state = printRoot["gcode_state"].as<String>();
  if (state.length() > 0 && state != "null")
  {
    cachedValues.message = state;
    cachedValues.is_printing = isPrintingState(state);
  }

  int printError = printRoot["print_error"].as<int>();
  cachedValues.has_error = (printError != 0);
  if (printError != 0)
  {
    cachedValues.message = "ERROR " + String(printError);
  }

  hasMqttData = true;
}

bool connectMqtt()
{
  if (mqttClient.connected())
    return true;

  String clientId = "xkn-" + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF), HEX);
  bool ok = mqttClient.connect(clientId.c_str(), "bblp", mqtt_access_code);
  if (!ok)
  {
    Serial.print("MQTT connect failed, state=");
    Serial.println(mqttClient.state());
    return false;
  }

  mqttClient.subscribe(mqttReportTopic);
  requestPrinterPush();
  Serial.println("MQTT connected");
  return true;
}
} // namespace

void printer_mqtt_init()
{
  snprintf(mqttReportTopic, sizeof(mqttReportTopic), "device/%s/report", mqtt_serial);
  snprintf(mqttRequestTopic, sizeof(mqttRequestTopic), "device/%s/request", mqtt_serial);

  if (!mqtt_reject_unauthorized)
  {
    secureClient.setInsecure();
  }

  secureClient.setTimeout(5000);
  mqttClient.setServer(mqtt_host, mqtt_port);
  mqttClient.setCallback(onMqttMessage);
  mqttClient.setBufferSize(12288);
  mqttClient.setKeepAlive(20);
  mqttClient.setSocketTimeout(5);

  cachedValues.message = "MQTT connecting";
  connectMqtt();
}

void printer_mqtt_loop()
{
  if (WiFi.status() != WL_CONNECTED)
    return;

  if (!mqttClient.connected())
  {
    unsigned long now = millis();
    if (now - lastMqttConnectAttempt >= 5000)
    {
      lastMqttConnectAttempt = now;
      connectMqtt();
    }
    return;
  }

  mqttClient.loop();

  unsigned long now = millis();
  if (now - lastMqttPushRequest >= 30000)
  {
    requestPrinterPush();
  }
}

void get_printer_progress(printer_values *pValues)
{
  if (!hasMqttData)
  {
    cachedValues.message = "Waiting MQTT data";
  }
  *pValues = cachedValues;
}

void get_printer_status(printer_values *pValues)
{
  if (!hasMqttData)
  {
    cachedValues.message = "Waiting MQTT data";
  }
  *pValues = cachedValues;
}
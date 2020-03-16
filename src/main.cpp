#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>

#include <type_traits>

#include <dsmr.h>

// Remove all traces of secrets before publishing to github!
#include "secrets.h"

using P1Data = ParsedData<
  /* String */ identification,
  /* String */ p1_version,
  /* String */ timestamp,
  /* String */ equipment_id,
  /* FixedValue */ energy_delivered_tariff1,
  /* FixedValue */ energy_delivered_tariff2,
  /* FixedValue */ energy_returned_tariff1,
  /* FixedValue */ energy_returned_tariff2,
  /* String */ electricity_tariff,
  /* FixedValue */ power_delivered,
  /* FixedValue */ power_returned,
  /* uint32_t */ electricity_failures,
  /* uint32_t */ electricity_long_failures,
  /* String */ electricity_failure_log,
  /* uint32_t */ electricity_sags_l1,
  /* uint32_t */ electricity_sags_l2,
  /* uint32_t */ electricity_sags_l3,
  /* uint32_t */ electricity_swells_l1,
  /* uint32_t */ electricity_swells_l2,
  /* uint32_t */ electricity_swells_l3,
  /* FixedValue */ voltage_l1,
  /* FixedValue */ voltage_l2,
  /* FixedValue */ voltage_l3,
  /* FixedValue */ current_l1,
  /* FixedValue */ current_l2,
  /* FixedValue */ current_l3,
  /* FixedValue */ power_delivered_l1,
  /* FixedValue */ power_delivered_l2,
  /* FixedValue */ power_delivered_l3,
  /* FixedValue */ power_returned_l1,
  /* FixedValue */ power_returned_l2,
  /* FixedValue */ power_returned_l3,
  /* uint16_t */ gas_device_type,
  /* String */ gas_equipment_id,
  /* TimestampedFixedValue */ gas_delivered
>;


const auto server_ip = IPAddress(192, 168, 2, 8);
WiFiClient wifi_client;
PubSubClient client(server_ip, 1883, wifi_client);
const char* client_id = "dsmr-esp";

namespace converthelper {

template <typename ValueT>
String toString(ValueT value) {
  return String(value);
}

template <>
String toString(float value) {
  return String(value, 3);
}

}

/**
 * This illustrates looping over all parsed fields using the
 * ParsedData::applyEach method.
 *
 * When passed an instance of this Printer object, applyEach will loop
 * over each field and call Printer::apply, passing a reference to each
 * field in turn. This passes the actual field object, not the field
 * value, so each call to Printer::apply will have a differently typed
 * parameter.
 *
 * For this reason, Printer::apply is a template, resulting in one
 * distinct apply method for each field used. This allows looking up
 * things like Item::name, which is different for every field type,
 * without having to resort to virtual method calls (which result in
 * extra storage usage). The tradeoff is here that there is more code
 * generated (but due to compiler inlining, it's pretty much the same as
 * if you just manually printed all field names and values (with no
 * cost at all if you don't use the Printer).
 */
struct Printer {
  template<typename Item>
  void apply(Item &i) {
    if (i.present()) {
      //Serial.println(data);
      // This is probably an expensive operation.
      // Find out if this can be improved.
      String topic = String("sensor/dsmr/dsmr-esp/status/") + Item::name + String("_") + Item::unit();
      String value = converthelper::toString(i.val());
      client.publish(topic.c_str(), value.c_str());
      //client.loop();
    }
  }
};

void setLed(bool enable) {
  // Needs inverted signal
  digitalWrite(LED_BUILTIN, !enable);
}

SoftwareSerial p1meter;
P1Reader p1reader(&p1meter);

void setup(){
  pinMode(LED_BUILTIN, OUTPUT);
  setLed(true);
  // RX = D2
  // one telegram is ~822 chars long, so 1024 as buffer size should be ok
  p1meter.begin(115200, SWSERIAL_8N1, D2, -1, true, 850);
  Serial.begin(115200);
  Serial.println("\nSmart meter reader v0.3.0, by Rick van Schijndel");
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  setLed(false);
}

void reconnect() {
  // backoff
  unsigned retry = 0;
  // Loop until we're reconnected

  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(client_id, username, password)) {
      Serial.println("mqtt connected");
    } else {
      Serial.print("mqtt: failed to connect, rc=");
      Serial.print(client.state());
      // First retry every 5 seconds, but back off to once every 30 seconds after retrying a couple of times.
      unsigned retry_time_s = retry < 5 ? 5 : 30;
      Serial.printf(" retrying in %u seconds\n", retry_time_s);
      // Wait n seconds before retrying
      delay(retry_time_s * 1000);
      retry++;
    }
  }
}

unsigned successful = 0;
unsigned failed = 0;

const char* success_topic = "sensor/dsmr/dsmr-esp/status/decode_success";
const char* failure_topic = "sensor/dsmr/dsmr-esp/status/decode_failure";
const char* heap_before_topic = "sensor/dsmr/dsmr-esp/status/free_heap_before";
const char* heap_after_topic = "sensor/dsmr/dsmr-esp/status/free_heap_after";
const char* error_topic = "sensor/dsmr/dsmr-esp/status/error";

void publish(const char* topic, unsigned value) {
    char buffer[(sizeof(value)*8+1)];
    snprintf(buffer, sizeof(buffer), "%u", value);
    client.publish(topic, buffer);
}

void publish(const char* topic, unsigned long value) {
    char buffer[(sizeof(value)*8+1)];
    snprintf(buffer, sizeof(buffer), "%lu", value);
    client.publish(topic, buffer);
}

unsigned passed_loops = 0;
void led_flicker(const unsigned loops) {
  if ((++passed_loops % loops) == 0) {
    setLed(true);
  }
  if ((passed_loops % loops) == 500) {
    setLed(false);
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (p1meter.available()) {
    p1reader.loop();
  }

  if (p1reader.available()) {
    publish(heap_before_topic, ESP.getFreeHeap());
    setLed(true);
    Serial.println("Data available");
    P1Data data;
    String err;
    if (p1reader.parse(&data, &err)) {
      successful += 1;
      data.applyEach(Printer());
    } else {
      failed += 1;
      Serial.println(err);
      client.publish(error_topic, err.c_str());
    }
    Serial.print("Successful: ");
    Serial.print(successful);
    Serial.print(" failed: ");
    Serial.println(failed);
    publish(success_topic, successful);
    publish(failure_topic, failed);
    publish(heap_after_topic, ESP.getFreeHeap());
    setLed(false);
  }

  //led_flicker(100000);
}

